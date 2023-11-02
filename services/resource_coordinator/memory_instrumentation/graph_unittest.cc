// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph.h"

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

namespace {

using base::trace_event::MemoryAllocatorDumpGuid;
using Node = memory_instrumentation::GlobalDumpGraph::Node;
using Process = memory_instrumentation::GlobalDumpGraph::Process;

const MemoryAllocatorDumpGuid kEmptyGuid;

}  // namespace

TEST(GlobalDumpGraphTest, CreateContainerForProcess) {
  GlobalDumpGraph global_dump_graph;

  Process* dump = global_dump_graph.CreateGraphForProcess(10);
  ASSERT_NE(dump, nullptr);

  auto* map = global_dump_graph.process_dump_graphs().find(10)->second.get();
  ASSERT_EQ(dump, map);
}

TEST(GlobalDumpGraphTest, AddNodeOwnershipEdge) {
  GlobalDumpGraph global_dump_graph;
  Node owner(global_dump_graph.shared_memory_graph(), nullptr);
  Node owned(global_dump_graph.shared_memory_graph(), nullptr);

  global_dump_graph.AddNodeOwnershipEdge(&owner, &owned, 1);

  auto& edges = global_dump_graph.edges();
  ASSERT_NE(edges.begin(), edges.end());

  auto& edge = *edges.begin();
  ASSERT_EQ(edge.source(), &owner);
  ASSERT_EQ(edge.target(), &owned);
  ASSERT_EQ(edge.priority(), 1);
}

TEST(GlobalDumpGraphTest, VisitInDepthFirstPostOrder) {
  GlobalDumpGraph graph;
  Process* process_1 = graph.CreateGraphForProcess(1);
  Process* process_2 = graph.CreateGraphForProcess(2);

  Node* c1 = process_1->CreateNode(kEmptyGuid, "c1", false);
  Node* c2 = process_1->CreateNode(kEmptyGuid, "c2", false);
  Node* c2_c1 = process_1->CreateNode(kEmptyGuid, "c2/c1", false);
  Node* c2_c2 = process_1->CreateNode(kEmptyGuid, "c2/c2", false);

  Node* c3 = process_2->CreateNode(kEmptyGuid, "c3", false);
  Node* c3_c1 = process_2->CreateNode(kEmptyGuid, "c3/c1", false);
  Node* c3_c2 = process_2->CreateNode(kEmptyGuid, "c3/c2", false);

  // |c3_c2| owns |c2_c2|.
  graph.AddNodeOwnershipEdge(c3_c2, c2_c2, 1);

  // This method should always call owners and then children before the node
  // itself.
  auto iterator = graph.VisitInDepthFirstPostOrder();
  ASSERT_EQ(iterator.next(), graph.shared_memory_graph()->root());
  ASSERT_EQ(iterator.next(), c1);
  ASSERT_EQ(iterator.next(), c2_c1);
  ASSERT_EQ(iterator.next(), c3_c2);
  ASSERT_EQ(iterator.next(), c2_c2);
  ASSERT_EQ(iterator.next(), c2);
  ASSERT_EQ(iterator.next(), process_1->root());
  ASSERT_EQ(iterator.next(), c3_c1);
  ASSERT_EQ(iterator.next(), c3);
  ASSERT_EQ(iterator.next(), process_2->root());
  ASSERT_EQ(iterator.next(), nullptr);
}

TEST(GlobalDumpGraphTest, VisitInDepthFirstPreOrder) {
  GlobalDumpGraph graph;
  Process* process_1 = graph.CreateGraphForProcess(1);
  Process* process_2 = graph.CreateGraphForProcess(2);

  Node* c1 = process_1->CreateNode(kEmptyGuid, "c1", false);
  Node* c2 = process_1->CreateNode(kEmptyGuid, "c2", false);
  Node* c2_c1 = process_1->CreateNode(kEmptyGuid, "c2/c1", false);
  Node* c2_c2 = process_1->CreateNode(kEmptyGuid, "c2/c2", false);

  Node* c3 = process_2->CreateNode(kEmptyGuid, "c3", false);
  Node* c3_c1 = process_2->CreateNode(kEmptyGuid, "c3/c1", false);
  Node* c3_c2 = process_2->CreateNode(kEmptyGuid, "c3/c2", false);

  // |c2_c2| owns |c3_c2|. Note this is opposite of post-order.
  graph.AddNodeOwnershipEdge(c2_c2, c3_c2, 1);

  // This method should always call owners and then children after the node
  // itself.
  auto iterator = graph.VisitInDepthFirstPreOrder();
  ASSERT_EQ(iterator.next(), graph.shared_memory_graph()->root());
  ASSERT_EQ(iterator.next(), process_1->root());
  ASSERT_EQ(iterator.next(), c1);
  ASSERT_EQ(iterator.next(), c2);
  ASSERT_EQ(iterator.next(), c2_c1);
  ASSERT_EQ(iterator.next(), process_2->root());
  ASSERT_EQ(iterator.next(), c3);
  ASSERT_EQ(iterator.next(), c3_c1);
  ASSERT_EQ(iterator.next(), c3_c2);
  ASSERT_EQ(iterator.next(), c2_c2);
  ASSERT_EQ(iterator.next(), nullptr);
}

TEST(ProcessTest, CreateAndFindNode) {
  GlobalDumpGraph global_dump_graph;
  Process graph(1, &global_dump_graph);

  Node* first =
      graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple/test/1", false);
  Node* second =
      graph.CreateNode(MemoryAllocatorDumpGuid(2), "simple/test/2", false);
  Node* third =
      graph.CreateNode(MemoryAllocatorDumpGuid(3), "simple/other/1", false);
  Node* fourth =
      graph.CreateNode(MemoryAllocatorDumpGuid(4), "complex/path", false);
  Node* fifth = graph.CreateNode(MemoryAllocatorDumpGuid(5),
                                 "complex/path/child/1", false);

  ASSERT_EQ(graph.FindNode("simple/test/1"), first);
  ASSERT_EQ(graph.FindNode("simple/test/2"), second);
  ASSERT_EQ(graph.FindNode("simple/other/1"), third);
  ASSERT_EQ(graph.FindNode("complex/path"), fourth);
  ASSERT_EQ(graph.FindNode("complex/path/child/1"), fifth);

  auto& nodes_by_guid = global_dump_graph.nodes_by_guid();
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(1))->second, first);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(2))->second, second);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(3))->second, third);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(4))->second, fourth);
  ASSERT_EQ(nodes_by_guid.find(MemoryAllocatorDumpGuid(5))->second, fifth);
}

TEST(ProcessTest, CreateNodeParent) {
  GlobalDumpGraph global_dump_graph;
  Process graph(1, &global_dump_graph);

  Node* parent = graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple", false);
  Node* child =
      graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple/child", false);

  ASSERT_EQ(parent->parent(), graph.root());
  ASSERT_EQ(child->parent(), parent);
}

TEST(ProcessTest, WeakAndExplicit) {
  GlobalDumpGraph global_dump_graph;
  Process graph(1, &global_dump_graph);

  Node* first =
      graph.CreateNode(MemoryAllocatorDumpGuid(1), "simple/test/1", true);
  Node* second =
      graph.CreateNode(MemoryAllocatorDumpGuid(2), "simple/test/2", false);

  ASSERT_TRUE(first->is_weak());
  ASSERT_FALSE(second->is_weak());

  ASSERT_TRUE(first->is_explicit());
  ASSERT_TRUE(second->is_explicit());

  Node* parent = graph.FindNode("simple/test");
  ASSERT_NE(parent, nullptr);
  ASSERT_FALSE(parent->is_weak());
  ASSERT_FALSE(parent->is_explicit());

  Node* grandparent = graph.FindNode("simple");
  ASSERT_NE(grandparent, nullptr);
  ASSERT_FALSE(grandparent->is_weak());
  ASSERT_FALSE(grandparent->is_explicit());
}

TEST(NodeTest, GetChild) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  ASSERT_EQ(node.GetChild("test"), nullptr);

  Node child(global_dump_graph.shared_memory_graph(), &node);
  node.InsertChild("child", &child);
  ASSERT_EQ(node.GetChild("child"), &child);
}

TEST(NodeTest, InsertChild) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  ASSERT_EQ(node.GetChild("test"), nullptr);

  Node child(global_dump_graph.shared_memory_graph(), &node);
  node.InsertChild("child", &child);
  ASSERT_EQ(node.GetChild("child"), &child);
}

TEST(NodeTest, AddEntry) {
  GlobalDumpGraph global_dump_graph;
  Node node(global_dump_graph.shared_memory_graph(), nullptr);

  node.AddEntry("scalar", Node::Entry::ScalarUnits::kBytes, 100ul);
  ASSERT_EQ(node.entries()->size(), 1ul);

  node.AddEntry("string", "data");
  ASSERT_EQ(node.entries()->size(), 2ul);

  auto scalar = node.entries()->find("scalar");
  ASSERT_EQ(scalar->first, "scalar");
  ASSERT_EQ(scalar->second.units, Node::Entry::ScalarUnits::kBytes);
  ASSERT_EQ(scalar->second.value_uint64, 100ul);

  auto string = node.entries()->find("string");
  ASSERT_EQ(string->first, "string");
  ASSERT_EQ(string->second.value_string, "data");
}

}  // namespace memory_instrumentation