// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/chrome_graph_processor.h"

#include <iostream>

#include "base/memory/shared_memory_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/raw_memory_graph_node.h"

namespace memory_instrumentation {

TEST(ChromeGraphProcessorTest, CreateMemoryGraphWithNoneOperation) {
  ChromeGraphProcessor::MemoryDumpMap process_dumps;

  base::trace_event::MemoryDumpArgs dump_args = {
      .level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(dump_args);

  auto* source = pmd.CreateAllocatorDump("test1/test2/test3");
  source->AddScalar(perfetto::trace_processor::RawMemoryGraphNode::kNameSize,
                    perfetto::trace_processor::RawMemoryGraphNode::kUnitsBytes,
                    10);

  auto* target = pmd.CreateAllocatorDump("target");
  pmd.AddOwnershipEdge(source->guid(), target->guid(), 10);

  auto* weak = pmd.CreateWeakSharedGlobalAllocatorDump(
      base::trace_event::MemoryAllocatorDumpGuid(1));

  process_dumps.emplace(1, &pmd);

  auto global_node = ChromeGraphProcessor::CreateMemoryGraph(
      process_dumps, ChromeGraphProcessor::Operations::kNoneOperation);

  ASSERT_EQ(1u, global_node->process_node_graphs().size());

  auto id_to_dump_it = global_node->process_node_graphs().find(1);
  auto* first_child = id_to_dump_it->second->FindNode("test1");
  ASSERT_NE(first_child, nullptr);
  ASSERT_EQ(first_child->parent(), id_to_dump_it->second->root());

  auto* second_child = first_child->GetChild("test2");
  ASSERT_NE(second_child, nullptr);
  ASSERT_EQ(second_child->parent(), first_child);

  auto* third_child = second_child->GetChild("test3");
  ASSERT_NE(third_child, nullptr);
  ASSERT_EQ(third_child->parent(), second_child);

  auto* direct = id_to_dump_it->second->FindNode("test1/test2/test3");
  ASSERT_EQ(third_child, direct);

  ASSERT_EQ(third_child->entries()->size(), 1ul);

  auto size = third_child->entries()->find(
      perfetto::trace_processor::RawMemoryGraphNode::kNameSize);
  ASSERT_EQ(10ul, size->second.value_uint64);

  ASSERT_TRUE(weak->flags() &
              perfetto::trace_processor::RawMemoryGraphNode::Flags::kWeak);

  auto& edges = global_node->edges();
  auto edge_it = edges.begin();
  ASSERT_EQ(std::distance(edges.begin(), edges.end()), 1l);
  ASSERT_EQ(edge_it->source(), direct);
  ASSERT_EQ(edge_it->target(), id_to_dump_it->second->FindNode("target"));
  ASSERT_EQ(edge_it->priority(), 10);
}

TEST(ChromeGraphProcessorTest, CreateMemoryGraphWithAllOperations) {
  ChromeGraphProcessor::MemoryDumpMap process_dumps;

  base::trace_event::MemoryDumpArgs dump_args = {
      .level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  base::trace_event::ProcessMemoryDump pmd(dump_args);

  auto* source = pmd.CreateAllocatorDump("test1/test2/test3");
  source->AddScalar(perfetto::trace_processor::RawMemoryGraphNode::kNameSize,
                    perfetto::trace_processor::RawMemoryGraphNode::kUnitsBytes,
                    10);

  auto* target = pmd.CreateAllocatorDump("target");
  pmd.AddOwnershipEdge(source->guid(), target->guid(), 10);

  auto* weak = pmd.CreateWeakSharedGlobalAllocatorDump(
      base::trace_event::MemoryAllocatorDumpGuid(1));

  process_dumps.emplace(1, &pmd);
  std::map<base::ProcessId, uint64_t> shared_footprints;
  auto global_node = ChromeGraphProcessor::CreateMemoryGraph(
      process_dumps, ChromeGraphProcessor::Operations::kAllOperations,
      &shared_footprints);

  ASSERT_EQ(1u, global_node->process_node_graphs().size());

  auto id_to_dump_it = global_node->process_node_graphs().find(1);
  auto* first_child = id_to_dump_it->second->FindNode("test1");
  ASSERT_NE(first_child, nullptr);
  ASSERT_EQ(first_child->parent(), id_to_dump_it->second->root());

  auto* second_child = first_child->GetChild("test2");
  ASSERT_NE(second_child, nullptr);
  ASSERT_EQ(second_child->parent(), first_child);

  auto* third_child = second_child->GetChild("test3");
  ASSERT_NE(third_child, nullptr);
  ASSERT_EQ(third_child->parent(), second_child);

  auto* direct = id_to_dump_it->second->FindNode("test1/test2/test3");
  ASSERT_EQ(third_child, direct);

  ASSERT_EQ(third_child->entries()->size(), 2ul);

  auto size = third_child->entries()->find(
      perfetto::trace_processor::RawMemoryGraphNode::kNameSize);
  ASSERT_EQ(10ul, size->second.value_uint64);

  ASSERT_TRUE(weak->flags() &
              perfetto::trace_processor::RawMemoryGraphNode::Flags::kWeak);

  auto& edges = global_node->edges();
  auto edge_it = edges.begin();
  ASSERT_EQ(std::distance(edges.begin(), edges.end()), 1l);
  ASSERT_EQ(edge_it->source(), direct);
  ASSERT_EQ(edge_it->target(), id_to_dump_it->second->FindNode("target"));
  ASSERT_EQ(edge_it->priority(), 10);
}

}  // namespace memory_instrumentation
