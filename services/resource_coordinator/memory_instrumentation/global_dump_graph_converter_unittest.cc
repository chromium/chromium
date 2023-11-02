// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/global_dump_graph_converter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::trace_event::MemoryAllocatorDumpGuid;
using perfetto::trace_processor::GlobalNodeGraph;

TEST(GlobalDumpGraphConverterTest, Convert) {
  auto input = std::make_unique<perfetto::trace_processor::GlobalNodeGraph>();
  perfetto::trace_processor::GlobalNodeGraph::Node* owner = nullptr;
  perfetto::trace_processor::GlobalNodeGraph::Node* owned = nullptr;

  // Adding first process with one node
  {
    GlobalNodeGraph::Process* process = input->CreateGraphForProcess(1);
    perfetto::trace_processor::MemoryAllocatorNodeId node_id{1};
    owner = process->CreateNode(node_id, "test1", false);
    owner->AddEntry(
        "first",
        perfetto::trace_processor::GlobalNodeGraph::Node::Entry::kObjects, 123);
    owner->AddEntry("second", "string");
    owner->set_weak(true);
    owner->set_explicit(true);
    owner->add_not_owned_sub_size(100);
    owner->add_not_owning_sub_size(200);
    owner->set_owned_coefficient(3);
    owner->set_owning_coefficient(5);
    owner->set_cumulative_owned_coefficient(7);
    owner->set_cumulative_owning_coefficient(9);
  }

  // Adding second process with one node
  {
    GlobalNodeGraph::Process* process = input->CreateGraphForProcess(2);
    perfetto::trace_processor::MemoryAllocatorNodeId node_id{2};
    owned = process->CreateNode(node_id, "test2", false);
  }

  // Creating Edge between two previously created nodes
  input->AddNodeOwnershipEdge(owner, owned, 99);

  // Create shared memory graph
  {
    GlobalNodeGraph::Process* process = input->shared_memory_graph();
    perfetto::trace_processor::MemoryAllocatorNodeId node_id{3};
    process->CreateNode(node_id, "test3", false);
  }

  // Convert GlobalDumpGraph to Perfetto GlobalDumpGrap.
  GlobalDumpGraphConverter converter;
  const std::unique_ptr<GlobalDumpGraph> output = converter.Convert(*input);

  ASSERT_EQ(output->process_dump_graphs().size(), 2uL);
  ASSERT_NE(output->process_dump_graphs().find(1),
            output->process_dump_graphs().end());

  {
    const GlobalDumpGraph::Process& process1 =
        *output->process_dump_graphs().at(1);
    ASSERT_EQ(process1.root()->const_children().size(), 1uL);
    const GlobalDumpGraph::Node* node1 = process1.root()->GetChild("test1");
    ASSERT_NE(node1, nullptr);
    ASSERT_EQ(node1->is_weak(), true);
    ASSERT_EQ(node1->is_explicit(), true);
    ASSERT_EQ(node1->not_owned_sub_size(), 100uL);
    ASSERT_EQ(node1->not_owning_sub_size(), 200uL);
    ASSERT_EQ(node1->owned_coefficient(), 3);
    ASSERT_EQ(node1->owning_coefficient(), 5);
    ASSERT_EQ(node1->cumulative_owned_coefficient(), 7);
    ASSERT_EQ(node1->cumulative_owning_coefficient(), 9);
    ASSERT_EQ(node1->const_entries().size(), 2uL);
    ASSERT_NE(node1->const_entries().find("first"),
              node1->const_entries().end());
    ASSERT_EQ(node1->const_entries().at("first").type,
              GlobalDumpGraph::Node::Entry::kUInt64);
    ASSERT_EQ(node1->const_entries().at("first").units,
              GlobalDumpGraph::Node::Entry::kObjects);
    ASSERT_EQ(node1->const_entries().at("first").value_uint64, 123uL);
    ASSERT_NE(node1->const_entries().find("second"),
              node1->const_entries().end());
    ASSERT_EQ(node1->const_entries().at("second").type,
              GlobalDumpGraph::Node::Entry::kString);
    ASSERT_EQ(node1->const_entries().at("second").value_string, "string");

    const GlobalDumpGraph::Process& process2 =
        *output->process_dump_graphs().at(2);
    ASSERT_EQ(process2.root()->const_children().size(), 1uL);
    const GlobalDumpGraph::Node* node2 = process2.root()->GetChild("test2");
    ASSERT_NE(node2, nullptr);

    const auto edges_count =
        std::distance(std::begin(output->edges()), std::end(output->edges()));
    ASSERT_EQ(edges_count, 1L);

    const GlobalDumpGraph::Edge edge = *output->edges().begin();
    ASSERT_EQ(edge.source(), node1);
    ASSERT_EQ(edge.target(), node2);
    ASSERT_EQ(edge.priority(), 99);

    ASSERT_EQ(output->shared_memory_graph()->root()->const_children().size(),
              1uL);
    const GlobalDumpGraph::Node* node3 =
        output->shared_memory_graph()->root()->GetChild("test3");
    ASSERT_NE(node3, nullptr);
  }
}

}  // namespace memory_instrumentation
