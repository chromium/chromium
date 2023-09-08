// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/memory_dump_map_converter.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/trace_event/process_memory_dump.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph_processor.h"

namespace memory_instrumentation {

MemoryDumpMapConverter::MemoryDumpMapConverter() = default;
MemoryDumpMapConverter::~MemoryDumpMapConverter() = default;

perfetto::trace_processor::GraphProcessor::RawMemoryNodeMap
MemoryDumpMapConverter::Convert(const MemoryDumpMap& input) {
  perfetto::trace_processor::GraphProcessor::RawMemoryNodeMap output;

  for (const auto& entry : input) {
    const base::ProcessId process_id = entry.first;
    const base::trace_event::ProcessMemoryDump* process_dump = entry.second;

    if (process_dump == nullptr)
      continue;

    output.emplace(process_id, ConvertProcessMemoryDump(*process_dump));
  }

  return output;
}

std::unique_ptr<MemoryDumpMapConverter::PerfettoProcessMemoryNode>
MemoryDumpMapConverter::ConvertProcessMemoryDump(
    const base::trace_event::ProcessMemoryDump& input) {
  const perfetto::trace_processor::LevelOfDetail level_of_detail =
      ConvertLevelOfDetail(input.dump_args().level_of_detail);

  MemoryDumpMapConverter::PerfettoProcessMemoryNode::MemoryNodesMap nodes_map =
      ConvertAllocatorDumps(input);
  MemoryDumpMapConverter::PerfettoProcessMemoryNode::AllocatorNodeEdgesMap
      edges_map = ConvertAllocatorDumpEdges(input);

  return std::make_unique<PerfettoProcessMemoryNode>(
      level_of_detail, std::move(edges_map), std::move(nodes_map));
}

perfetto::trace_processor::LevelOfDetail
MemoryDumpMapConverter::ConvertLevelOfDetail(
    const base::trace_event::MemoryDumpLevelOfDetail& input) const {
  switch (input) {
    case base::trace_event::MemoryDumpLevelOfDetail::kBackground:
      return perfetto::trace_processor::LevelOfDetail::kBackground;
    case base::trace_event::MemoryDumpLevelOfDetail::kLight:
      return perfetto::trace_processor::LevelOfDetail::kLight;
    case base::trace_event::MemoryDumpLevelOfDetail::kDetailed:
      return perfetto::trace_processor::LevelOfDetail::kDetailed;
  }
  return perfetto::trace_processor::LevelOfDetail::kDetailed;
}

std::vector<perfetto::trace_processor::RawMemoryGraphNode::MemoryNodeEntry>
MemoryDumpMapConverter::ConvertAllocatorDumpEntries(
    const base::trace_event::MemoryAllocatorDump& input) const {
  std::vector<perfetto::trace_processor::RawMemoryGraphNode::MemoryNodeEntry>
      output;

  for (const auto& entry : input.entries()) {
    if (entry.entry_type ==
        base::trace_event::MemoryAllocatorDump::Entry::kUint64) {
      output.emplace_back(entry.name, entry.units, entry.value_uint64);
    } else {
      output.emplace_back(entry.name, entry.units, entry.value_string);
    }
  }
  return output;
}

std::unique_ptr<perfetto::trace_processor::RawMemoryGraphNode>
MemoryDumpMapConverter::ConvertMemoryAllocatorDump(
    const base::trace_event::MemoryAllocatorDump& input) const {
  auto output = std::make_unique<perfetto::trace_processor::RawMemoryGraphNode>(
      input.absolute_name(), ConvertLevelOfDetail(input.level_of_detail()),
      ConvertMemoryAllocatorDumpGuid(input.guid()),
      ConvertAllocatorDumpEntries(input));

  CopyAndConvertAllocatorDumpFlags(input, output.get());

  return output;
}

void MemoryDumpMapConverter::CopyAndConvertAllocatorDumpFlags(
    const base::trace_event::MemoryAllocatorDump& input,
    perfetto::trace_processor::RawMemoryGraphNode* output) const {
  output->clear_flags(output->flags());
  output->set_flags(
      input.flags() & base::trace_event::MemoryAllocatorDump::WEAK
          ? perfetto::trace_processor::RawMemoryGraphNode::kWeak
          : perfetto::trace_processor::RawMemoryGraphNode::kDefault);
}

MemoryDumpMapConverter::PerfettoProcessMemoryNode::MemoryNodesMap
MemoryDumpMapConverter::ConvertAllocatorDumps(
    const base::trace_event::ProcessMemoryDump& input) const {
  MemoryDumpMapConverter::PerfettoProcessMemoryNode::MemoryNodesMap output;
  for (const auto& entry : input.allocator_dumps()) {
    const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& dump =
        entry.second;

    output.emplace(entry.first, ConvertMemoryAllocatorDump(*dump));
  }
  return output;
}

MemoryDumpMapConverter::PerfettoProcessMemoryNode::AllocatorNodeEdgesMap
MemoryDumpMapConverter::ConvertAllocatorDumpEdges(
    const base::trace_event::ProcessMemoryDump& input) const {
  MemoryDumpMapConverter::PerfettoProcessMemoryNode::AllocatorNodeEdgesMap
      output;
  for (const auto& entry : input.allocator_dumps_edges()) {
    std::unique_ptr<perfetto::trace_processor::MemoryGraphEdge> edge =
        ConvertAllocatorDumpEdge(entry.second);
    const perfetto::trace_processor::MemoryAllocatorNodeId source =
        edge->source;
    output.emplace(source, std::move(edge));
  }
  return output;
}

std::unique_ptr<perfetto::trace_processor::MemoryGraphEdge>
MemoryDumpMapConverter::ConvertAllocatorDumpEdge(
    const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge& input)
    const {
  return std::make_unique<perfetto::trace_processor::MemoryGraphEdge>(
      ConvertMemoryAllocatorDumpGuid(input.source),
      ConvertMemoryAllocatorDumpGuid(input.target), input.importance,
      input.overridable);
}

perfetto::trace_processor::MemoryAllocatorNodeId
MemoryDumpMapConverter::ConvertMemoryAllocatorDumpGuid(
    const base::trace_event::MemoryAllocatorDumpGuid& input) const {
  return perfetto::trace_processor::MemoryAllocatorNodeId(input.ToUint64());
}

}  // namespace memory_instrumentation
