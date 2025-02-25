/* Reads a binary file, disassembles it, and performs various call graph analyses. */
#include "conditionalDisable.h"
#ifdef ROSE_BINARY_TEST_DISABLED
#include <iostream>
int main() { std::cout <<"disabled for " <<ROSE_BINARY_TEST_DISABLED <<"\n"; return 1; }
#else

#include "rose.h"
#include <Rose/BinaryAnalysis/FunctionCall.h>

#include <boost/graph/graphviz.hpp>

using namespace Rose;

/* Label the graphviz vertices with function entry addresses rather than vertex numbers. */
template<class FunctionCallGraph>
struct GraphvizVertexWriter {
    typedef typename boost::graph_traits<FunctionCallGraph>::vertex_descriptor Vertex;
    const FunctionCallGraph &g;
    GraphvizVertexWriter(FunctionCallGraph &g): g(g) {}
    void operator()(std::ostream &output, const Vertex &v) {
        SgAsmFunction *func = get(boost::vertex_name, g, v);
        output <<"[ label=\"" <<StringUtility::addrToString(func->get_entry_va()) <<"\" ]";
    }
};

/* Filter that rejects basic block that are uncategorized.  I.e., those blocks that were disassemble but not ultimately
 * linked into the list of known functions.  We excluded these because their control flow information is often nonsensical. */
struct ExcludeLeftovers: public Rose::BinaryAnalysis::FunctionCall::VertexFilter {
    bool operator()(Rose::BinaryAnalysis::FunctionCall */*analyzer*/, SgAsmFunction *func) {
        return func && 0==(func->get_reason() & SgAsmFunction::FUNC_LEFTOVERS);
    }
};

int
main(int argc, char *argv[])
{
    /* Algorithm is first argument. */
    assert(argc>1);
    std::string algorithm = argv[1];
    memmove(argv+1, argv+2, argc-1); /* also copy null ptr */
    --argc;

    /* Parse the binary file */
    SgProject *project = frontend(argc, argv);
    std::vector<SgAsmInterpretation*> interps = SageInterface::querySubTree<SgAsmInterpretation>(project);
    if (interps.empty()) {
        fprintf(stderr, "no binary interpretations found\n");
        exit(1);
    }

    ExcludeLeftovers exclude_leftovers;

    /* Calculate plain old CG over entire interpretation. */
    if (algorithm=="A") {
        typedef Rose::BinaryAnalysis::FunctionCall::Graph CG;
        Rose::BinaryAnalysis::FunctionCall cg_analyzer;
        cg_analyzer.set_vertex_filter(&exclude_leftovers);
        CG cg = cg_analyzer.build_cg_from_ast<CG>(interps.back());
        boost::write_graphviz(std::cout, cg, GraphvizVertexWriter<CG>(cg));
    }

    /* Calculate the call graph from the control flow graph. */
    if (algorithm=="B") {
        typedef Rose::BinaryAnalysis::ControlFlow::Graph CFG;
        typedef Rose::BinaryAnalysis::FunctionCall::Graph CG;
        CFG cfg = Rose::BinaryAnalysis::ControlFlow().build_block_cfg_from_ast<CFG>(interps.back());
        Rose::BinaryAnalysis::FunctionCall cg_analyzer;
        cg_analyzer.set_vertex_filter(&exclude_leftovers);
        CG cg = cg_analyzer.build_cg_from_cfg<CG>(cfg);
        boost::write_graphviz(std::cout, cg, GraphvizVertexWriter<CG>(cg));
    }

    return 0;
}

#endif
