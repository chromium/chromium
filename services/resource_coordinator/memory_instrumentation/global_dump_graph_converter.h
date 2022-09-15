// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GLOBAL_DUMP_GRAPH_CONVERTER_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GLOBAL_DUMP_GRAPH_CONVERTER_H_

#include <map>
#include <memory>
#include <vector>

#include "services/resource_coordinator/memory_instrumentation/graph.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph_processor.h"

namespace memory_instrumentation {

// Converts the Perfetto GlobalNodeGraph to the corresponding defined in
// Chromium type GlobalDumpGraph.
//
// Example usage:
//
// {
//   perfetto::trace_processor::GlobalNodeGraph graph;
//
//   GlobalDumpGraphConverter converter;
//   std::unique_ptr<GlobalDumpGraph> dumpGraph = converter.Convert(graph);
// }
class GlobalDumpGraphConverter {
 public:
  GlobalDumpGraphConverter();
  ~GlobalDumpGraphConverter();
  GlobalDumpGraphConverter(const GlobalDumpGraphConverter&) = delete;
  void operator=(const GlobalDumpGraphConverter&) = delete;

  std::unique_ptr<GlobalDumpGraph> Convert(
      const perfetto::trace_processor::GlobalNodeGraph& input) const;

 private:
  // Map is used during conversion from Perfetto GlobalNodeGraph to Chromium
  // GlobalDumpGraph. It simplifies finding matching nodes during conversion of
  // graph edges.
  using NodePointerPerfettoToChromeMap =
      std::map<const perfetto::trace_processor::GlobalNodeGraph::Node*,
               GlobalDumpGraph::Node*>;

  void CopyAndConvertProcessDumps(
      const perfetto::trace_processor::GlobalNodeGraph& input,
      GlobalDumpGraph* output,
      NodePointerPerfettoToChromeMap* pointer_map) const;

  void CopyAndConvertSharedMemoryGraph(
      const perfetto::trace_processor::GlobalNodeGraph& input,
      GlobalDumpGraph* output,
      NodePointerPerfettoToChromeMap* pointer_map) const;

  void CopyAndConvertNodeTree(
      const perfetto::trace_processor::GlobalNodeGraph::Node* input,
      GlobalDumpGraph::Process* output,
      const std::string& node_path,
      NodePointerPerfettoToChromeMap* pointer_map) const;

  base::trace_event::MemoryAllocatorDumpGuid ConvertMemoryAllocatorDumpGuid(
      const perfetto::trace_processor::MemoryAllocatorNodeId& input) const;

  void CopyNodeMembers(
      const perfetto::trace_processor::GlobalNodeGraph::Node& input,
      GlobalDumpGraph::Node* output) const;

  void CopyAndConvertEdges(
      const perfetto::trace_processor::GlobalNodeGraph& input,
      GlobalDumpGraph* output,
      NodePointerPerfettoToChromeMap* pointer_map) const;

  void CopyAndConvertEdge(
      const perfetto::trace_processor::GlobalNodeGraph::Edge& input,
      GlobalDumpGraph* output,
      const NodePointerPerfettoToChromeMap* pointer_map) const;

  GlobalDumpGraph::Node::Entry::ScalarUnits ConvertScalarUnits(
      const perfetto::trace_processor::GlobalNodeGraph::Node::Entry::ScalarUnits
          input) const;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GLOBAL_DUMP_GRAPH_CONVERTER_H_
