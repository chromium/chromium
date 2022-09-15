// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_MEMORY_DUMP_MAP_CONVERTER_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_MEMORY_DUMP_MAP_CONVERTER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/trace_event/process_memory_dump.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph_processor.h"

namespace memory_instrumentation {

// Converts the Chromium MemoryDumpMap to the corresponding defined in Perfetto
// type RawMemoryNodeMap.
//
// Example usage:
//
// {
//   base::trace_event::ProcessMemoryDump pmd;
//
//   MemoryDumpMapConverter converter;
//   perfetto::trace_processor::GraphProcessor::RawMemoryNodeMap
//       perfettoNodeMap = converter.Convert(pmd);
// }
class MemoryDumpMapConverter {
 public:
  using MemoryDumpMap =
      std::map<base::ProcessId, const base::trace_event::ProcessMemoryDump*>;

  using PerfettoProcessMemoryNode =
      perfetto::trace_processor::RawProcessMemoryNode;

  MemoryDumpMapConverter();
  ~MemoryDumpMapConverter();
  MemoryDumpMapConverter(const MemoryDumpMapConverter&) = delete;
  void operator=(const MemoryDumpMapConverter&) = delete;

  perfetto::trace_processor::GraphProcessor::RawMemoryNodeMap Convert(
      const MemoryDumpMap& input);

 private:
  std::unique_ptr<MemoryDumpMapConverter::PerfettoProcessMemoryNode>
  ConvertProcessMemoryDump(const base::trace_event::ProcessMemoryDump& input);

  perfetto::trace_processor::LevelOfDetail ConvertLevelOfDetail(
      const base::trace_event::MemoryDumpLevelOfDetail& input) const;

  PerfettoProcessMemoryNode::MemoryNodesMap ConvertAllocatorDumps(
      const base::trace_event::ProcessMemoryDump& input) const;

  std::unique_ptr<perfetto::trace_processor::RawMemoryGraphNode>
  ConvertMemoryAllocatorDump(
      const base::trace_event::MemoryAllocatorDump& input) const;

  void CopyAndConvertAllocatorDumpFlags(
      const base::trace_event::MemoryAllocatorDump& input,
      perfetto::trace_processor::RawMemoryGraphNode* output) const;

  std::vector<perfetto::trace_processor::RawMemoryGraphNode::MemoryNodeEntry>
  ConvertAllocatorDumpEntries(
      const base::trace_event::MemoryAllocatorDump& input) const;

  PerfettoProcessMemoryNode::AllocatorNodeEdgesMap ConvertAllocatorDumpEdges(
      const base::trace_event::ProcessMemoryDump& input) const;

  std::unique_ptr<perfetto::trace_processor::MemoryGraphEdge>
  ConvertAllocatorDumpEdge(
      const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge&
          input) const;

  perfetto::trace_processor::MemoryAllocatorNodeId
  ConvertMemoryAllocatorDumpGuid(
      const base::trace_event::MemoryAllocatorDumpGuid& input) const;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_MEMORY_DUMP_MAP_CONVERTER_H_
