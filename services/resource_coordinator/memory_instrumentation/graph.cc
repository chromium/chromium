// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/graph.h"

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_tokenizer.h"

namespace memory_instrumentation {

namespace {

using base::trace_event::MemoryAllocatorDumpGuid;
using Edge = GlobalDumpGraph::Edge;
using PostOrderIterator = GlobalDumpGraph::PostOrderIterator;
using PreOrderIterator = GlobalDumpGraph::PreOrderIterator;
using Process = GlobalDumpGraph::Process;
using Node = GlobalDumpGraph::Node;

}  // namespace

GlobalDumpGraph::GlobalDumpGraph()
    : shared_memory_graph_(
          std::make_unique<Process>(base::kNullProcessId, this)) {}
GlobalDumpGraph::~GlobalDumpGraph() {}

Process* GlobalDumpGraph::CreateGraphForProcess(base::ProcessId process_id) {
  auto id_to_dump_iterator = process_dump_graphs_.emplace(
      process_id, std::make_unique<Process>(process_id, this));
  DCHECK(id_to_dump_iterator.second);  // Check for duplicate pids
  return id_to_dump_iterator.first->second.get();
}

void GlobalDumpGraph::AddNodeOwnershipEdge(Node* owner,
                                           Node* owned,
                                           int importance) {
  // The owner node should not already be associated with an edge.
  DCHECK(!owner->owns_edge());

  all_edges_.emplace_front(owner, owned, importance);
  Edge* edge = &*all_edges_.begin();
  owner->SetOwnsEdge(edge);
  owned->AddOwnedByEdge(edge);
}

Node* GlobalDumpGraph::CreateNode(Process* process_graph, Node* parent) {
  all_nodes_.emplace_front(process_graph, parent);
  return &*all_nodes_.begin();
}

PreOrderIterator GlobalDumpGraph::VisitInDepthFirstPreOrder() {
  std::vector<raw_ptr<Node, VectorExperimental>> roots;
  for (const auto& [process_id, process] :
       base::Reversed(process_dump_graphs_)) {
    roots.push_back(process->root());
  }
  roots.push_back(shared_memory_graph_->root());
  return PreOrderIterator(std::move(roots));
}

PostOrderIterator GlobalDumpGraph::VisitInDepthFirstPostOrder() {
  std::vector<raw_ptr<Node, VectorExperimental>> roots;
  for (const auto& [process_id, process] :
       base::Reversed(process_dump_graphs_)) {
    roots.push_back(process->root());
  }
  roots.push_back(shared_memory_graph_->root());
  return PostOrderIterator(std::move(roots));
}

Process::Process(base::ProcessId pid, GlobalDumpGraph* global_graph)
    : pid_(pid),
      global_graph_(global_graph),
      root_(global_graph->CreateNode(this, nullptr)) {}
Process::~Process() {}

Node* Process::CreateNode(MemoryAllocatorDumpGuid guid,
                          std::string_view path,
                          bool weak) {
  DCHECK(!path.empty());

  std::string path_string(path);
  base::StringTokenizer tokenizer(path_string, "/");

  // Perform a tree traversal, creating the nodes if they do not
  // already exist on the path to the child.
  Node* current = root_;
  while (tokenizer.GetNext()) {
    std::string_view key = tokenizer.token_piece();
    Node* parent = current;
    current = current->GetChild(key);
    if (!current) {
      current = global_graph_->CreateNode(this, parent);
      parent->InsertChild(key, current);
    }
  }

  // The final node should have the weakness specified by the
  // argument and also be considered explicit.
  current->set_weak(weak);
  current->set_explicit(true);

  // The final node should also have the associated |guid|.
  current->set_guid(guid);

  // Add to the global guid map as well if it exists.
  if (!guid.empty())
    global_graph_->nodes_by_guid_.emplace(guid, current);

  return current;
}

Node* Process::FindNode(std::string_view path) {
  DCHECK(!path.empty());

  std::string path_string(path);
  base::StringTokenizer tokenizer(path_string, "/");
  Node* current = root_;
  while (tokenizer.GetNext()) {
    std::string_view key = tokenizer.token_piece();
    current = current->GetChild(key);
    if (!current)
      return nullptr;
  }
  return current;
}

Node::Node(Process* dump_graph, Node* parent)
    : dump_graph_(dump_graph), parent_(parent), owns_edge_(nullptr) {}
Node::~Node() {}

Node* Node::GetChild(std::string_view name) {
  DCHECK(!name.empty());
  DCHECK_EQ(std::string::npos, name.find('/'));

  auto child = children_.find(std::string(name));
  return child == children_.end() ? nullptr : child->second;
}

void Node::InsertChild(std::string_view name, Node* node) {
  DCHECK(!name.empty());
  DCHECK_EQ(std::string::npos, name.find('/'));

  children_.emplace(std::string(name), node);
}

Node* Node::CreateChild(std::string_view name) {
  Node* new_child = dump_graph_->global_graph()->CreateNode(dump_graph_, this);
  InsertChild(name, new_child);
  return new_child;
}

bool Node::IsDescendentOf(const Node& possible_parent) const {
  const Node* current = this;
  while (current != nullptr) {
    if (current == &possible_parent)
      return true;
    current = current->parent();
  }
  return false;
}

void Node::AddOwnedByEdge(Edge* edge) {
  owned_by_edges_.push_back(edge);
}

void Node::SetOwnsEdge(Edge* owns_edge) {
  owns_edge_ = owns_edge;
}

void Node::AddEntry(std::string name,
                    Node::Entry::ScalarUnits units,
                    uint64_t value) {
  entries_.emplace(name, Node::Entry(units, value));
}

void Node::AddEntry(std::string name, std::string value) {
  entries_.emplace(name, Node::Entry(value));
}

Node::Entry::Entry(Entry::ScalarUnits units, uint64_t value)
    : type(Node::Entry::Type::kUInt64), units(units), value_uint64(value) {}

Node::Entry::Entry(std::string value)
    : type(Node::Entry::Type::kString),
      units(Node::Entry::ScalarUnits::kObjects),
      value_string(value),
      value_uint64(0) {}

Edge::Edge(Node* source, Node* target, int priority)
    : source_(source), target_(target), priority_(priority) {}

PreOrderIterator::PreOrderIterator(
    std::vector<raw_ptr<Node, VectorExperimental>> roots)
    : to_visit_(std::move(roots)) {}
PreOrderIterator::PreOrderIterator(PreOrderIterator&& other) = default;
PreOrderIterator::~PreOrderIterator() {}

// Yields the next node in the DFS post-order traversal.
Node* PreOrderIterator::next() {
  while (!to_visit_.empty()) {
    // Retain a pointer to the node at the top and remove it from stack.
    Node* node = to_visit_.back();
    to_visit_.pop_back();

    // If the node has already been visited, don't visit it again.
    if (visited_.count(node) != 0)
      continue;

    // If we haven't visited the node which this node owns then wait for that.
    if (node->owns_edge() && visited_.count(node->owns_edge()->target()) == 0)
      continue;

    // If we haven't visited the node's parent then wait for that.
    if (node->parent() && visited_.count(node->parent()) == 0)
      continue;

    // Visit all children of this node.
    for (const auto& [name, child] : base::Reversed(*node->children())) {
      to_visit_.push_back(child);
    }

    // Visit all owners of this node.
    for (memory_instrumentation::GlobalDumpGraph::Edge* edge :
         base::Reversed(*node->owned_by_edges())) {
      to_visit_.push_back(edge->source());
    }

    // Add this node to the visited set.
    visited_.insert(node);
    return node;
  }
  return nullptr;
}

PostOrderIterator::PostOrderIterator(
    std::vector<raw_ptr<Node, VectorExperimental>> roots)
    : to_visit_(std::move(roots)) {}
PostOrderIterator::PostOrderIterator(PostOrderIterator&& other) = default;
PostOrderIterator::~PostOrderIterator() = default;

// Yields the next node in the DFS post-order traversal.
Node* PostOrderIterator::next() {
  while (!to_visit_.empty()) {
    // Retain a pointer to the node at the top and remove it from stack.
    Node* node = to_visit_.back();
    to_visit_.pop_back();

    // If the node has already been visited, don't visit it again.
    if (visited_.count(node) != 0)
      continue;

    // If the node is at the top of the path, we have already looked
    // at its children and owners.
    if (!path_.empty() && path_.back() == node) {
      // Mark the current node as visited so we don't visit again.
      visited_.insert(node);

      // The current node is no longer on the path.
      path_.pop_back();

      return node;
    }

    // If the node is not at the front, it should also certainly not be
    // anywhere else in the path. If it is, there is a cycle in the graph.
    DCHECK(!base::Contains(path_, node));
    path_.push_back(node);

    // Add this node back to the queue of nodes to visit.
    to_visit_.push_back(node);

    // Visit all children of this node.
    for (const auto& [name, child] : base::Reversed(*node->children())) {
      to_visit_.push_back(child);
    }

    // Visit all owners of this node.
    for (memory_instrumentation::GlobalDumpGraph::Edge* edge :
         base::Reversed(*node->owned_by_edges())) {
      to_visit_.push_back(edge->source());
    }
  }
  return nullptr;
}

}  // namespace memory_instrumentation
