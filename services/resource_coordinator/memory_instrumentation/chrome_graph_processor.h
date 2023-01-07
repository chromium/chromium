// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_CHROME_GRAPH_PROCESSOR_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_CHROME_GRAPH_PROCESSOR_H_

#include <map>
#include <memory>

#include "base/trace_event/process_memory_dump.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph.h"

namespace memory_instrumentation {

// ChromeGraphProcessor is a wrapper for a GraphProcessor from Perfetto. This
// class takes care of all required input and output parameter conversions and
// could be used as a replacement for GraphProcessor class from Chromium.
//
// Example usage:
// {
//   std::map<base::ProcessId, uint64_t> shared_footprints;
//   std::unique_ptr<GlobalDumpGraph> global_graph =
//       ChromeGraphProcessor::CreateMemoryGraph(
//           &shared_footprints,
//           pid_to_pmd ChromeGraphProcessor::Operation::kAllOperations);
// }

using perfetto::trace_processor::GlobalNodeGraph;

class ChromeGraphProcessor {
 public:
  using MemoryDumpMap =
      std::map<base::ProcessId, const base::trace_event::ProcessMemoryDump*>;

  ChromeGraphProcessor() = delete;
  ~ChromeGraphProcessor() = delete;
  ChromeGraphProcessor(const ChromeGraphProcessor&) = delete;
  void operator=(const ChromeGraphProcessor&) = delete;

  enum class Operations : int {
    kNoneOperation = 0,
    kRemoveWeakNodesFromGraph = (1 << 1),
    kComputeSharedFootprintFromGraph = (1 << 2),
    kAddOverheadsAndPropagateEntries = (1 << 3),
    kCalculateSizesForGraph = (1 << 4),
    kGraphWithoutSharedFootprint =
        (kRemoveWeakNodesFromGraph | kAddOverheadsAndPropagateEntries |
         kCalculateSizesForGraph),
    kAllOperations =
        (kRemoveWeakNodesFromGraph | kComputeSharedFootprintFromGraph |
         kAddOverheadsAndPropagateEntries | kCalculateSizesForGraph)

  };

  // This is a wrapper function for the Perfetto GraphProcessor member function
  // perfetto::GraphProcessor::CreateMemoryGraph(). This function combines
  // creation of node graph with some possible operations on that graph
  // performed by other Perfetto GraphProcessor member functions like
  // RemoveWeakNodesFromGraph() or AddOverheadsAndPropagateEntries().
  //
  // Shared footprints is an output parameter which will hold information
  // about computed footprints by distributing the memory of the nodes among the
  // processes which have edges left. This parameter will be filled when bit
  // |kComputeSharedFootprintFromGraph| is set in operations parameter.
  // The valid values of |operations| are |kNoneOperation|, |kAllOperations| and
  // |kGraphWithoutSharedFootprint|
  static std::unique_ptr<GlobalNodeGraph> CreateMemoryGraph(
      const MemoryDumpMap& process_dumps,
      Operations operations,
      std::map<base::ProcessId, uint64_t>* shared_footprints);

  // This is overloaded function to simplify usage if the shared footpints are
  // not needed.
  static std::unique_ptr<GlobalNodeGraph> CreateMemoryGraph(
      const MemoryDumpMap& process_dumps,
      Operations operations);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_CHROME_GRAPH_PROCESSOR_H_
