// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/chrome_graph_processor.h"

#include <list>
#include <memory>
#include <string>

#include "services/resource_coordinator/memory_instrumentation/global_dump_graph_converter.h"
#include "services/resource_coordinator/memory_instrumentation/memory_dump_map_converter.h"

namespace memory_instrumentation {

using perfetto::trace_processor::GraphProcessor;

// static
std::unique_ptr<GlobalNodeGraph> ChromeGraphProcessor::CreateMemoryGraph(
    const MemoryDumpMap& process_dumps,
    Operations operations) {
  operations = static_cast<Operations>(
      static_cast<std::underlying_type<Operations>::type>(operations) &
      ~static_cast<std::underlying_type<Operations>::type>(
          Operations::kComputeSharedFootprintFromGraph));
  return CreateMemoryGraph(process_dumps, operations, nullptr);
}

// static
std::unique_ptr<GlobalNodeGraph> ChromeGraphProcessor::CreateMemoryGraph(
    const MemoryDumpMap& process_dumps,
    Operations operations,
    std::map<base::ProcessId, uint64_t>* shared_footprints) {
  DCHECK(operations == Operations::kNoneOperation ||
         operations == Operations::kGraphWithoutSharedFootprint ||
         operations == Operations::kAllOperations);
  MemoryDumpMapConverter input_converter;
  GraphProcessor::RawMemoryNodeMap memory_node_map =
      input_converter.Convert(process_dumps);

  std::unique_ptr<GlobalNodeGraph> memory_graph =
      GraphProcessor::CreateMemoryGraph(memory_node_map);
  if (operations == Operations::kGraphWithoutSharedFootprint) {
    GraphProcessor::RemoveWeakNodesFromGraph(memory_graph.get());
    GraphProcessor::AddOverheadsAndPropagateEntries(memory_graph.get());
    GraphProcessor::CalculateSizesForGraph(memory_graph.get());
  } else if (operations == Operations::kAllOperations) {
    GraphProcessor::RemoveWeakNodesFromGraph(memory_graph.get());
    DCHECK(shared_footprints);
    shared_footprints->clear();
    auto original =
        GraphProcessor::ComputeSharedFootprintFromGraph(*memory_graph);

    for (const auto& item : original) {
      shared_footprints->emplace(static_cast<base::ProcessId>(item.first),
                                 item.second);
    }
    GraphProcessor::AddOverheadsAndPropagateEntries(memory_graph.get());
    GraphProcessor::CalculateSizesForGraph(memory_graph.get());
  }

  return memory_graph;
}
}  // namespace memory_instrumentation
