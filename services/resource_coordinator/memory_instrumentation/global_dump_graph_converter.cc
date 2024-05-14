// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/global_dump_graph_converter.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/trace_event/process_memory_dump.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph_processor.h"

namespace memory_instrumentation {

using perfetto::trace_processor::GlobalNodeGraph;

GlobalDumpGraphConverter::GlobalDumpGraphConverter() = default;
GlobalDumpGraphConverter::~GlobalDumpGraphConverter() = default;

std::unique_ptr<GlobalDumpGraph> GlobalDumpGraphConverter::Convert(
    const GlobalNodeGraph& input) const {
  NodePointerPerfettoToChromeMap pointer_map;
  auto output = std::make_unique<GlobalDumpGraph>();

  CopyAndConvertProcessDumps(input, output.get(), &pointer_map);

  CopyAndConvertSharedMemoryGraph(input, output.get(), &pointer_map);

  CopyAndConvertEdges(input, output.get(), &pointer_map);

  return output;
}

void GlobalDumpGraphConverter::CopyAndConvertProcessDumps(
    const GlobalNodeGraph& input,
    GlobalDumpGraph* output,
    NodePointerPerfettoToChromeMap* pointer_map) const {
  for (const auto& input_entry : input.process_node_graphs()) {
    const base::ProcessId process_id = input_entry.first;
    const GlobalNodeGraph::Process* input_process = input_entry.second.get();
    if (input_process == nullptr)
      continue;

    GlobalDumpGraph::Process* output_process =
        output->CreateGraphForProcess(process_id);

    CopyAndConvertNodeTree(input_process->root(), output_process, {},
                           pointer_map);
  }
}

void GlobalDumpGraphConverter::CopyAndConvertSharedMemoryGraph(
    const GlobalNodeGraph& input,
    GlobalDumpGraph* output,
    NodePointerPerfettoToChromeMap* pointer_map) const {
  CopyAndConvertNodeTree(input.shared_memory_graph()->root(),
                         output->shared_memory_graph(), {}, pointer_map);
}

void GlobalDumpGraphConverter::CopyAndConvertNodeTree(
    const GlobalNodeGraph::Node* input,
    GlobalDumpGraph::Process* output,
    const std::string& node_path,
    NodePointerPerfettoToChromeMap* pointer_map) const {
  DCHECK(input);

  for (const auto& entry : input->const_children()) {
    const std::string path = node_path + "/" + entry.first;
    const GlobalNodeGraph::Node* raw_child = entry.second;

    GlobalDumpGraph::Node* child =
        output->CreateNode(ConvertMemoryAllocatorDumpGuid(raw_child->id()),
                           path, raw_child->is_weak());

    CopyNodeMembers(*raw_child, child);

    CopyAndConvertNodeTree(raw_child, output, path, pointer_map);
    pointer_map->emplace(raw_child, child);
  }
}

base::trace_event::MemoryAllocatorDumpGuid
GlobalDumpGraphConverter::ConvertMemoryAllocatorDumpGuid(
    const perfetto::trace_processor::MemoryAllocatorNodeId& input) const {
  return base::trace_event::MemoryAllocatorDumpGuid(input.ToUint64());
}

void GlobalDumpGraphConverter::CopyNodeMembers(
    const GlobalNodeGraph::Node& input,
    GlobalDumpGraph::Node* output) const {
  for (const auto& item : input.const_entries()) {
    const std::string& name = item.first;
    const GlobalNodeGraph::Node::Entry& entry = item.second;

    if (entry.type == GlobalNodeGraph::Node::Entry::kUInt64) {
      output->AddEntry(name, ConvertScalarUnits(entry.units),
                       entry.value_uint64);
    } else {
      output->AddEntry(name, entry.value_string);
    }
  }

  output->set_weak(input.is_weak());
  output->set_explicit(input.is_explicit());
  output->add_not_owned_sub_size(input.not_owned_sub_size());
  output->add_not_owning_sub_size(input.not_owning_sub_size());
  output->set_owned_coefficient(input.owned_coefficient());
  output->set_owning_coefficient(input.owning_coefficient());
  output->set_cumulative_owned_coefficient(
      input.cumulative_owned_coefficient());
  output->set_cumulative_owning_coefficient(
      input.cumulative_owning_coefficient());
}

void GlobalDumpGraphConverter::CopyAndConvertEdges(
    const GlobalNodeGraph& input,
    GlobalDumpGraph* output,
    NodePointerPerfettoToChromeMap* pointer_map) const {
  for (const auto& input_edge : input.edges()) {
    CopyAndConvertEdge(input_edge, output, pointer_map);
  }
}

void GlobalDumpGraphConverter::CopyAndConvertEdge(
    const GlobalNodeGraph::Edge& input,
    GlobalDumpGraph* output,
    const NodePointerPerfettoToChromeMap* pointer_map) const {
  DCHECK(input.source());
  DCHECK(input.target());

  GlobalDumpGraph::Node* source = pointer_map->at(input.source());
  GlobalDumpGraph::Node* target = pointer_map->at(input.target());

  DCHECK(source);
  DCHECK(target);

  output->AddNodeOwnershipEdge(source, target, input.priority());
}

GlobalDumpGraph::Node::Entry::ScalarUnits
GlobalDumpGraphConverter::ConvertScalarUnits(
    GlobalNodeGraph::Node::Entry::ScalarUnits input) const {
  using PerfettoScalarUnits = GlobalNodeGraph::Node::Entry::ScalarUnits;
  using ChromeScalarUnits = GlobalDumpGraph::Node::Entry::ScalarUnits;
  switch (input) {
    case PerfettoScalarUnits::kObjects:
      return ChromeScalarUnits::kObjects;
    case PerfettoScalarUnits::kBytes:
      return ChromeScalarUnits::kBytes;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace memory_instrumentation
