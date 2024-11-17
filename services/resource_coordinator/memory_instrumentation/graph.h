// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_H_

#include <forward_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/trace_event/memory_allocator_dump_guid.h"

namespace memory_instrumentation {

// Contains processed dump graphs for each process and in the global space.
// This class is also the arena which owns the nodes of the graph.
class GlobalDumpGraph {
 public:
  class Node;
  class Edge;
  class PreOrderIterator;
  class PostOrderIterator;

  // Graph of dumps either associated with a process or with
  // the shared space.
  class Process {
   public:
    Process(base::ProcessId pid, GlobalDumpGraph* global_graph);

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    ~Process();

    // Creates a node in the dump graph which is associated with the
    // given |guid|, |path| and |weak|ness and returns it.
    GlobalDumpGraph::Node* CreateNode(
        base::trace_event::MemoryAllocatorDumpGuid guid,
        std::string_view path,
        bool weak);

    // Returns the node in the graph at the given |path| or nullptr
    // if no such node exists in the provided |graph|.
    GlobalDumpGraph::Node* FindNode(std::string_view path);

    base::ProcessId pid() const { return pid_; }
    GlobalDumpGraph* global_graph() const { return global_graph_; }
    GlobalDumpGraph::Node* root() const { return root_; }

   private:
    base::ProcessId pid_;
    raw_ptr<GlobalDumpGraph> global_graph_;
    raw_ptr<GlobalDumpGraph::Node> root_;
  };

  // A single node in the graph of allocator dumps associated with a
  // certain path and containing the entries for this path.
  class Node {
   public:
    // Auxilary data (a scalar number or a string) about this node each
    // associated with a key.
    struct Entry {
      enum Type {
        kUInt64,
        kString,
      };

      // The units of the entry if the entry is a scalar. The scalar
      // refers to either a number of objects or a size in bytes.
      enum ScalarUnits {
        kObjects,
        kBytes,
      };

      // Creates the entry with the appropriate type.
      Entry(ScalarUnits units, uint64_t value);
      Entry(std::string value);

      const Type type;
      const ScalarUnits units;

      // The value of the entry if this entry has a string type.
      const std::string value_string;

      // The value of the entry if this entry has a integer type.
      const uint64_t value_uint64;
    };

    explicit Node(GlobalDumpGraph::Process* dump_graph, Node* parent);

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    ~Node();

    // Gets the direct child of a node for the given |subpath|.
    Node* GetChild(std::string_view name);

    // Inserts the given |node| as a child of the current node
    // with the given |subpath| as the key.
    void InsertChild(std::string_view name, Node* node);

    // Creates a child for this node with the given |name| as the key.
    Node* CreateChild(std::string_view name);

    // Checks if the current node is a descendent (i.e. exists as a child,
    // child of a child, etc.) of the given node |possible_parent|.
    bool IsDescendentOf(const Node& possible_parent) const;

    // Adds an entry for this dump node with the given |name|, |units| and
    // type.
    void AddEntry(std::string name, Entry::ScalarUnits units, uint64_t value);
    void AddEntry(std::string name, std::string value);

    // Adds an edge which indicates that this node is owned by
    // another node.
    void AddOwnedByEdge(Edge* edge);

    // Sets the edge indicates that this node owns another node.
    void SetOwnsEdge(Edge* edge);

    bool is_weak() const { return weak_; }
    void set_weak(bool weak) { weak_ = weak; }
    bool is_explicit() const { return explicit_; }
    void set_explicit(bool explicit_node) { explicit_ = explicit_node; }
    uint64_t not_owned_sub_size() const { return not_owned_sub_size_; }
    void add_not_owned_sub_size(uint64_t addition) {
      not_owned_sub_size_ += addition;
    }
    uint64_t not_owning_sub_size() const { return not_owning_sub_size_; }
    void add_not_owning_sub_size(uint64_t addition) {
      not_owning_sub_size_ += addition;
    }
    double owned_coefficient() const { return owned_coefficient_; }
    void set_owned_coefficient(double owned_coefficient) {
      owned_coefficient_ = owned_coefficient;
    }
    double owning_coefficient() const { return owning_coefficient_; }
    void set_owning_coefficient(double owning_coefficient) {
      owning_coefficient_ = owning_coefficient;
    }
    double cumulative_owned_coefficient() const {
      return cumulative_owned_coefficient_;
    }
    void set_cumulative_owned_coefficient(double cumulative_owned_coefficient) {
      cumulative_owned_coefficient_ = cumulative_owned_coefficient;
    }
    double cumulative_owning_coefficient() const {
      return cumulative_owning_coefficient_;
    }
    void set_cumulative_owning_coefficient(
        double cumulative_owning_coefficient) {
      cumulative_owning_coefficient_ = cumulative_owning_coefficient;
    }
    base::trace_event::MemoryAllocatorDumpGuid guid() const { return guid_; }
    void set_guid(base::trace_event::MemoryAllocatorDumpGuid guid) {
      guid_ = guid;
    }
    GlobalDumpGraph::Edge* owns_edge() const { return owns_edge_; }
    std::map<std::string, raw_ptr<Node, CtnExperimental>>* children() {
      return &children_;
    }
    const std::map<std::string, raw_ptr<Node, CtnExperimental>>&
    const_children() const {
      return children_;
    }
    std::vector<raw_ptr<GlobalDumpGraph::Edge, VectorExperimental>>*
    owned_by_edges() {
      return &owned_by_edges_;
    }
    const Node* parent() const { return parent_; }
    const GlobalDumpGraph::Process* dump_graph() const { return dump_graph_; }
    std::map<std::string, Entry>* entries() { return &entries_; }
    const std::map<std::string, Entry>& const_entries() const {
      return entries_;
    }

   private:
    raw_ptr<GlobalDumpGraph::Process, DanglingUntriaged> dump_graph_;
    const raw_ptr<Node> parent_;
    base::trace_event::MemoryAllocatorDumpGuid guid_;
    std::map<std::string, Entry> entries_;
    std::map<std::string, raw_ptr<Node, CtnExperimental>> children_;
    bool explicit_ = false;
    bool weak_ = false;
    uint64_t not_owning_sub_size_ = 0;
    uint64_t not_owned_sub_size_ = 0;
    double owned_coefficient_ = 1;
    double owning_coefficient_ = 1;
    double cumulative_owned_coefficient_ = 1;
    double cumulative_owning_coefficient_ = 1;

    raw_ptr<GlobalDumpGraph::Edge, DanglingUntriaged> owns_edge_;
    std::vector<raw_ptr<GlobalDumpGraph::Edge, VectorExperimental>>
        owned_by_edges_;
  };

  // An edge in the dump graph which indicates ownership between the
  // source and target nodes.
  class Edge {
   public:
    Edge(GlobalDumpGraph::Node* source,
         GlobalDumpGraph::Node* target,
         int priority);

    GlobalDumpGraph::Node* source() const { return source_; }
    GlobalDumpGraph::Node* target() const { return target_; }
    int priority() const { return priority_; }

   private:
    const raw_ptr<GlobalDumpGraph::Node> source_;
    const raw_ptr<GlobalDumpGraph::Node> target_;
    const int priority_;
  };

  // An iterator-esque class which yields nodes in a depth-first pre order.
  class PreOrderIterator {
   public:
    PreOrderIterator(std::vector<raw_ptr<Node, VectorExperimental>> root_nodes);
    PreOrderIterator(PreOrderIterator&& other);
    ~PreOrderIterator();

    // Yields the next node in the DFS post-order traversal.
    Node* next();

   private:
    std::vector<raw_ptr<Node, VectorExperimental>> to_visit_;
    std::set<raw_ptr<const Node, SetExperimental>> visited_;
  };

  // An iterator-esque class which yields nodes in a depth-first post order.
  class PostOrderIterator {
   public:
    PostOrderIterator(
        std::vector<raw_ptr<Node, VectorExperimental>> root_nodes);
    PostOrderIterator(PostOrderIterator&& other);
    ~PostOrderIterator();

    // Yields the next node in the DFS post-order traversal.
    Node* next();

   private:
    std::vector<raw_ptr<Node, VectorExperimental>> to_visit_;
    std::set<raw_ptr<Node, SetExperimental>> visited_;
    std::vector<raw_ptr<Node, VectorExperimental>> path_;
  };

  using ProcessDumpGraphMap =
      std::map<base::ProcessId, std::unique_ptr<GlobalDumpGraph::Process>>;
  using GuidNodeMap = std::map<base::trace_event::MemoryAllocatorDumpGuid,
                               raw_ptr<Node, CtnExperimental>>;

  GlobalDumpGraph();

  GlobalDumpGraph(const GlobalDumpGraph&) = delete;
  GlobalDumpGraph& operator=(const GlobalDumpGraph&) = delete;

  ~GlobalDumpGraph();

  // Creates a container for all the dump graphs for the process given
  // by the given |process_id|.
  GlobalDumpGraph::Process* CreateGraphForProcess(base::ProcessId process_id);

  // Adds an edge in the dump graph with the given source and target nodes
  // and edge priority.
  void AddNodeOwnershipEdge(Node* owner, Node* owned, int priority);

  // Returns an iterator which yields nodes in the nodes in this graph in
  // pre-order. That is, children and owners of nodes are returned after the
  // node itself.
  PreOrderIterator VisitInDepthFirstPreOrder();

  // Returns an iterator which yields nodes in the nodes in this graph in
  // post-order. That is, children and owners of nodes are returned before the
  // node itself.
  PostOrderIterator VisitInDepthFirstPostOrder();

  const GuidNodeMap& nodes_by_guid() const { return nodes_by_guid_; }
  GlobalDumpGraph::Process* shared_memory_graph() const {
    return shared_memory_graph_.get();
  }
  const ProcessDumpGraphMap& process_dump_graphs() const {
    return process_dump_graphs_;
  }
  const std::forward_list<Edge> edges() const { return all_edges_; }

 private:
  // Creates a node in the arena which is associated with the given
  // |dump_graph| and for the given |parent|.
  Node* CreateNode(GlobalDumpGraph::Process* dump_graph,
                   GlobalDumpGraph::Node* parent);

  std::forward_list<Node> all_nodes_;
  std::forward_list<Edge> all_edges_;
  GuidNodeMap nodes_by_guid_;
  std::unique_ptr<GlobalDumpGraph::Process> shared_memory_graph_;
  ProcessDumpGraphMap process_dump_graphs_;
};

}  // namespace memory_instrumentation
#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_GRAPH_H_
