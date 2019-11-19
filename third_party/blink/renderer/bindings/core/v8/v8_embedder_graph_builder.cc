// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_embedder_graph_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/unified_heap_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

using Traceable = const void*;
using Graph = v8::EmbedderGraph;

// Information about whether a node is attached to the main DOM tree
// or not. It is computed as follows:
// 1) A Document with IsContextDestroyed() = true is detached.
// 2) A Document with IsContextDestroyed() = false is attached.
// 3) A Node that is not connected to any Document is detached.
// 4) A Node that is connected to a detached Document is detached.
// 5) A Node that is connected to an attached Document is attached.
// 6) A ScriptWrappable that is reachable from an attached Node is
//    attached.
// 7) A ScriptWrappable that is reachable from a detached Node is
//    detached.
// 8) A ScriptWrappable that is not reachable from any Node is
//    considered (conservatively) as attached.
// The unknown state applies to ScriptWrappables during graph
// traversal when we don't have reachability information yet.
enum class DomTreeState { kAttached, kDetached, kUnknown };

DomTreeState DomTreeStateFromWrapper(v8::Isolate* isolate,
                                     uint16_t class_id,
                                     v8::Local<v8::Object> v8_value) {
  if (class_id != WrapperTypeInfo::kNodeClassId)
    return DomTreeState::kUnknown;
  Node* node = V8Node::ToImpl(v8_value);
  Node* root = V8GCController::OpaqueRootForGC(isolate, node);
  if (root->isConnected() &&
      !node->GetDocument().MasterDocument().IsContextDestroyed()) {
    return DomTreeState::kAttached;
  }
  return DomTreeState::kDetached;
}

class EmbedderNode : public Graph::Node {
 public:
  EmbedderNode(const char* name,
               Graph::Node* wrapper,
               DomTreeState dom_tree_state)
      : name_(name), wrapper_(wrapper), dom_tree_state_(dom_tree_state) {}

  DomTreeState GetDomTreeState() { return dom_tree_state_; }
  void UpdateDomTreeState(DomTreeState parent_dom_tree_state) {
    // If the child's state is unknown, then take the parent's state.
    // If the parent is attached, then the child is also attached.
    if (dom_tree_state_ == DomTreeState::kUnknown ||
        parent_dom_tree_state == DomTreeState::kAttached) {
      dom_tree_state_ = parent_dom_tree_state;
    }
  }
  // Graph::Node overrides.
  const char* Name() override { return name_; }
  const char* NamePrefix() override {
    return dom_tree_state_ == DomTreeState::kDetached ? "Detached" : nullptr;
  }
  size_t SizeInBytes() override { return 0; }
  Graph::Node* WrapperNode() override { return wrapper_; }

 private:
  const char* name_;
  Graph::Node* wrapper_;
  DomTreeState dom_tree_state_;
};

class EmbedderRootNode : public EmbedderNode {
 public:
  explicit EmbedderRootNode(const char* name)
      : EmbedderNode(name, nullptr, DomTreeState::kUnknown) {}
  // Graph::Node override.
  bool IsRootNode() override { return true; }

  void AddEdgeName(std::unique_ptr<const char> edge_name) {
    edge_names_.insert(std::move(edge_name));
  }

 private:
  // Storage to hold edge names until they have been internalized by V8.
  HashSet<std::unique_ptr<const char>> edge_names_;
};

class NodeBuilder final {
  USING_FAST_MALLOC(NodeBuilder);

 public:
  explicit NodeBuilder(Graph* graph) : graph_(graph) {}

  Graph::Node* GraphNode(const v8::Local<v8::Value>&);
  EmbedderNode* GraphNode(Traceable,
                          const char* name,
                          Graph::Node* wrapper,
                          DomTreeState);
  bool Contains(Traceable traceable) const {
    return graph_nodes_.Contains(traceable);
  }

 private:
  Graph* const graph_;
  HashMap<Traceable, EmbedderNode*> graph_nodes_;
};

v8::EmbedderGraph::Node* NodeBuilder::GraphNode(
    const v8::Local<v8::Value>& value) {
  return graph_->V8Node(value);
}

EmbedderNode* NodeBuilder::GraphNode(Traceable traceable,
                                     const char* name,
                                     v8::EmbedderGraph::Node* wrapper,
                                     DomTreeState dom_tree_state) {
  auto iter = graph_nodes_.find(traceable);
  if (iter != graph_nodes_.end()) {
    iter->value->UpdateDomTreeState(dom_tree_state);
    return iter->value;
  }
  // Ownership of the new node is transferred to the graph_.
  // graph_node_.at(tracable) is valid for all BuildEmbedderGraph execution.
  auto* raw_node = new EmbedderNode(name, wrapper, dom_tree_state);
  EmbedderNode* node = static_cast<EmbedderNode*>(
      graph_->AddNode(std::unique_ptr<Graph::Node>(raw_node)));
  graph_nodes_.insert(traceable, node);
  return node;
}

// V8EmbedderGraphBuilder is used to build heap snapshots of Blink's managed
// object graph. On a high level, the following operations are performed:
// - Objects are classified as attached, detached, or unknown.
// - Depending an implicit mode, objects are classified as relevant or internal.
//   This classification happens based on NameTrait and the fact that all
//   ScriptWrappable objects (those that can have JS properties) are using that
//   trait.
// - Not relevant objects are filtered where possible, e.g., sub graphs of
//   internals are filtered and not reported.
//
// The algorithm performs a single pass on the graph, starting from V8-to-Blink
// references that identify attached nodes. Each object then starts a recursion
// into its own subgraph to identify and filter subgraphs that only consist of
// internals. Roots, which are potentially Blink only, are transitively
// traversed after handling JavaScript related objects.
class GC_PLUGIN_IGNORE(
    "This class is not managed by Oilpan but GC plugin recognizes it as such "
    "due to Trace methods.") V8EmbedderGraphBuilder
    : public Visitor,
      public v8::PersistentHandleVisitor,
      public v8::EmbedderHeapTracer::TracedGlobalHandleVisitor {
 public:
  V8EmbedderGraphBuilder(v8::Isolate*, Graph*, NodeBuilder*);
  ~V8EmbedderGraphBuilder() override;

  void BuildEmbedderGraph();

  // v8::PersistentHandleVisitor override.
  void VisitPersistentHandle(v8::Persistent<v8::Value>*,
                             uint16_t class_id) override;

  // v8::EmbedderHeapTracer::TracedGlobalHandleVisitor override.
  void VisitTracedReference(
      const v8::TracedReference<v8::Value>& value) override;
  void VisitTracedGlobalHandle(const v8::TracedGlobal<v8::Value>&) override;

  // Visitor overrides.
  void VisitRoot(void*, TraceDescriptor, const base::Location&) final;
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final;
  void Visit(void*, TraceDescriptor) final;
  void VisitBackingStoreStrongly(void*, void**, TraceDescriptor) final;
  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final;
  bool VisitEphemeronKeyValuePair(void*,
                                  void*,
                                  EphemeronTracingCallback,
                                  EphemeronTracingCallback) final;

  // Unused Visitor overrides.
  void VisitWeak(void* object,
                 void* object_weak_ref,
                 TraceDescriptor desc,
                 WeakCallback callback) final {}
  void VisitBackingStoreOnly(void*, void**) final {}
  void RegisterBackingStoreCallback(void*, MovingObjectCallback) final {}
  void RegisterWeakCallback(WeakCallback, void*) final {}

 private:
  class ParentScope {
    STACK_ALLOCATED();

   public:
    ParentScope(V8EmbedderGraphBuilder* visitor, Traceable traceable)
        : visitor_(visitor) {
      visitor->current_parent_ = traceable;
    }
    ~ParentScope() { visitor_->current_parent_ = nullptr; }

   private:
    V8EmbedderGraphBuilder* const visitor_;
  };

  class State final {
    USING_FAST_MALLOC(State);

   public:
    State(Traceable traceable, const char* name, DomTreeState dom_tree_state)
        : traceable_(traceable), name_(name), dom_tree_state_(dom_tree_state) {}
    explicit State(EmbedderNode* node)
        : node_(node), dom_tree_state_(node->GetDomTreeState()) {}

    bool IsVisited() const { return visited_; }
    void MarkVisited() { visited_ = true; }

    bool IsPending() const { return pending_; }
    void MarkPending() { pending_ = true; }
    void UnmarkPending() { pending_ = false; }

    bool HasNode() const { return node_; }
    EmbedderNode* GetOrCreateNode(NodeBuilder* builder) {
      if (!node_) {
        DCHECK(name_);
        node_ = builder->GraphNode(traceable_, name_, nullptr, dom_tree_state_);
      }
      return node_;
    }

    DomTreeState GetDomTreeState() const { return dom_tree_state_; }
    void UpdateDomTreeState(DomTreeState parent_dom_tree_state) {
      // If the child's state is unknown, then take the parent's state.
      // If the parent is attached, then the child is also attached.
      if (dom_tree_state_ == DomTreeState::kUnknown ||
          parent_dom_tree_state == DomTreeState::kAttached) {
        dom_tree_state_ = parent_dom_tree_state;
      }
      if (node_)
        node_->UpdateDomTreeState(dom_tree_state_);
    }

    void AddEdge(State* destination, std::string edge_name) {
      auto result = named_edges_.insert(destination, std::move(edge_name));
      DCHECK(result.is_new_entry);
    }

    std::string EdgeName(State* destination) {
      auto it = named_edges_.find(destination);
      if (it != named_edges_.end())
        return it->value;
      return std::string();
    }

   private:
    EmbedderNode* node_ = nullptr;
    Traceable traceable_ = nullptr;
    const char* name_ = nullptr;
    DomTreeState dom_tree_state_;
    HashMap<State* /*destination*/, std::string> named_edges_;
    bool visited_ = false;
    bool pending_ = false;
  };

  // WorklistItemBase is used for different kinds of items that require
  // processing the regular worklist.
  class WorklistItemBase {
    USING_FAST_MALLOC(WorklistItemBase);

   public:
    explicit WorklistItemBase(State* parent, State* to_process)
        : parent_(parent), to_process_(to_process) {}
    virtual ~WorklistItemBase() = default;
    virtual void Process(V8EmbedderGraphBuilder*) = 0;

    State* to_process() const { return to_process_; }
    State* parent() const { return parent_; }

   private:
    State* const parent_;
    State* const to_process_;
  };

  // A VisitationItem processes a given object and visits all its children.
  class VisitationItem final : public WorklistItemBase {
   public:
    VisitationItem(State* parent,
                   State* to_process,
                   Traceable traceable,
                   TraceCallback trace_callback)
        : WorklistItemBase(parent, to_process),
          traceable_(traceable),
          trace_callback_(trace_callback) {}

    void Process(V8EmbedderGraphBuilder* builder) final {
      // Post-order traversal as parent needs the full information on child.
      builder->worklist_.push_back(std::unique_ptr<WorklistItemBase>{
          new VisitationDoneItem(parent(), to_process())});
      to_process()->MarkPending();
      DCHECK(to_process()->IsPending());
      DCHECK(to_process()->IsVisited());
      ParentScope parent_scope(builder, traceable_);
      trace_callback_(builder, const_cast<void*>(traceable_));
    }

   private:
    Traceable traceable_;
    TraceCallback trace_callback_;
  };

  // A VisitationDoneItem unmarks the pending state of an object and creates
  // an edge from a parent in case there is one.
  class VisitationDoneItem final : public WorklistItemBase {
   public:
    VisitationDoneItem(State* parent, State* to_process)
        : WorklistItemBase(parent, to_process) {}

    void Process(V8EmbedderGraphBuilder* builder) final {
      if (parent() && to_process()->HasNode()) {
        builder->AddEdge(parent(), to_process());
      }
      to_process()->UnmarkPending();
    }
  };

  class EphemeronItem final {
   public:
    EphemeronItem(Traceable key,
                  Traceable value,
                  Visitor::EphemeronTracingCallback key_tracing_callback,
                  Visitor::EphemeronTracingCallback value_tracing_callback)
        : key_(key),
          value_(value),
          key_tracing_callback_(key_tracing_callback),
          value_tracing_callback_(value_tracing_callback) {}

    bool Process(V8EmbedderGraphBuilder* builder) {
      Traceable key = nullptr;
      {
        TraceKeysScope scope(builder, &key);
        key_tracing_callback_(builder, const_cast<void*>(key_));
      }
      if (!key) {
        // Don't trace the value if the key is nullptr.
        return true;
      }
      if (!builder->StateExists(key))
        return false;
      {
        TraceValuesScope scope(builder, key);
        value_tracing_callback_(builder, const_cast<void*>(value_));
      }
      return true;
    }

   private:
    Traceable key_;
    Traceable value_;
    Visitor::EphemeronTracingCallback key_tracing_callback_;
    Visitor::EphemeronTracingCallback value_tracing_callback_;
  };

  State* GetOrCreateState(Traceable traceable,
                          const char* name,
                          DomTreeState dom_tree_state) {
    if (!states_.Contains(traceable)) {
      states_.insert(traceable, new State(traceable, name, dom_tree_state));
    }
    return states_.at(traceable);
  }

  bool StateExists(Traceable traceable) const {
    return states_.Contains(traceable);
  }

  State* GetStateNotNull(Traceable traceable) {
    CHECK(states_.Contains(traceable));
    return states_.at(traceable);
  }

  State* EnsureState(Traceable traceable, EmbedderNode* node) {
    if (!states_.Contains(traceable)) {
      states_.insert(traceable, new State(node));
    }
    return states_.at(traceable);
  }

  void EnsureRootState(EmbedderNode* node) {
    CHECK(!states_.Contains(node));
    states_.insert(node, new State(node));
  }

  void AddEdge(State*, State*);

  void VisitPersistentHandleInternal(v8::Local<v8::Object>, uint16_t);
  void VisitPendingActivities();
  void VisitBlinkRoots();
  void VisitTransitiveClosure();

  // Push a VisitatonItem to the main worklist in case the State has not been
  // already visited.
  void CreateAndPushVisitationItem(State* parent,
                                   State* to_process,
                                   Traceable traceable,
                                   TraceCallback trace_callback) {
    DCHECK(!to_process->IsVisited());
    to_process->MarkVisited();
    worklist_.push_back(std::unique_ptr<WorklistItemBase>{
        new VisitationItem(parent, to_process, traceable, trace_callback)});
  }

  void PushVisitationItem(std::unique_ptr<VisitationItem> item) {
    if (!item->to_process()->IsVisited()) {
      item->to_process()->MarkVisited();
      worklist_.push_back(std::move(item));
    }
  }

  class TraceKeysScope final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(TraceKeysScope);

   public:
    explicit TraceKeysScope(V8EmbedderGraphBuilder* graph_builder,
                            Traceable* key_holder)
        : graph_builder_(graph_builder), key_holder_(key_holder) {
      DCHECK(!graph_builder_->trace_keys_scope_);
      graph_builder_->trace_keys_scope_ = this;
    }
    ~TraceKeysScope() {
      DCHECK(graph_builder_->trace_keys_scope_);
      graph_builder_->trace_keys_scope_ = nullptr;
    }

    void SetKey(Traceable key) {
      DCHECK(!*key_holder_);
      *key_holder_ = key;
    }

   private:
    V8EmbedderGraphBuilder* const graph_builder_;
    Traceable* key_holder_;
  };

  class TraceValuesScope final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(TraceValuesScope);

   public:
    explicit TraceValuesScope(V8EmbedderGraphBuilder* graph_builder,
                              Traceable key)
        : graph_builder_(graph_builder) {
      graph_builder_->current_parent_ = key;
    }
    ~TraceValuesScope() { graph_builder_->current_parent_ = nullptr; }

   private:
    V8EmbedderGraphBuilder* const graph_builder_;
  };

  TraceKeysScope* trace_keys_scope_ = nullptr;

  v8::Isolate* const isolate_;
  Graph* const graph_;
  NodeBuilder* const node_builder_;

  Traceable current_parent_ = nullptr;
  HashMap<Traceable, State*> states_;
  // The default worklist that is used to visit transitive closure.
  Deque<std::unique_ptr<WorklistItemBase>> worklist_;
  // The worklist that collects detached Nodes during persistent handle
  // iteration.
  Deque<std::unique_ptr<VisitationItem>> detached_worklist_;
  // The worklist that collects ScriptWrappables with unknown information
  // about attached/detached state during persistent handle iteration.
  Deque<std::unique_ptr<VisitationItem>> unknown_worklist_;
  // The worklist that collects Ephemeron entries for later processing.
  Deque<std::unique_ptr<EphemeronItem>> ephemeron_worklist_;
};

V8EmbedderGraphBuilder::V8EmbedderGraphBuilder(v8::Isolate* isolate,
                                               Graph* graph,
                                               NodeBuilder* node_builder)
    : Visitor(ThreadState::Current()),
      isolate_(isolate),
      graph_(graph),
      node_builder_(node_builder) {
  CHECK(isolate);
  CHECK(graph);
  CHECK_EQ(isolate, ThreadState::Current()->GetIsolate());
}

V8EmbedderGraphBuilder::~V8EmbedderGraphBuilder() {
  for (const auto& kvp : states_) {
    delete kvp.value;
  }
}

void V8EmbedderGraphBuilder::BuildEmbedderGraph() {
  isolate_->VisitHandlesWithClassIds(this);
  v8::EmbedderHeapTracer* const tracer = static_cast<v8::EmbedderHeapTracer*>(
      ThreadState::Current()->unified_heap_controller());
  tracer->IterateTracedGlobalHandles(this);
// At this point we collected ScriptWrappables in three groups:
// attached, detached, and unknown.
#if DCHECK_IS_ON()
  for (auto const& item : worklist_) {
    DCHECK_EQ(DomTreeState::kAttached, item->to_process()->GetDomTreeState());
  }
  for (auto const& item : detached_worklist_) {
    DCHECK_EQ(DomTreeState::kDetached, item->to_process()->GetDomTreeState());
  }
  for (auto const& item : unknown_worklist_) {
    DCHECK_EQ(DomTreeState::kUnknown, item->to_process()->GetDomTreeState());
  }
#endif
  // We need to propagate attached/detached information to ScriptWrappables
  // with the unknown state. The information propagates from a parent to
  // a child as follows:
  // - if the parent is attached, then the child is considered attached.
  // - if the parent is detached and the child is unknown, then the child is
  //   considered detached.
  // - if the parent is unknown, then the state of the child does not change.
  //
  // We need to organize DOM traversal in three stages to ensure correct
  // propagation:
  // 1) Traverse from the attached nodes. All nodes discovered in this stage
  //    will be marked as kAttached.
  // 2) Traverse from the detached nodes. All nodes discovered in this stage
  //    will be marked as kDetached if they are not already marked as kAttached.
  // 3) Traverse from the unknown nodes. This is needed only for edge recording.
  // Stage 1: find transitive closure of the attached nodes.
  VisitTransitiveClosure();
  // Stage 2: find transitive closure of the detached nodes.
  while (!detached_worklist_.empty()) {
    auto item = std::move(detached_worklist_.back());
    detached_worklist_.pop_back();
    PushVisitationItem(std::move(item));
  }
  VisitTransitiveClosure();
  // Stage 3: find transitive closure of the unknown nodes.
  // Nodes reachable only via pending activities are treated as unknown.
  VisitPendingActivities();
  VisitBlinkRoots();
  while (!unknown_worklist_.empty()) {
    auto item = std::move(unknown_worklist_.back());
    unknown_worklist_.pop_back();
    PushVisitationItem(std::move(item));
  }
  VisitTransitiveClosure();
  DCHECK(worklist_.empty());
  DCHECK(detached_worklist_.empty());
  DCHECK(unknown_worklist_.empty());
  // ephemeron_worklist_ might not be empty. We might have an ephemeron whose
  // key is alive but was never observed by the snapshot (e.g. objects pointed
  // to by the stack). Such entries will remain in the worklist.
  //
  // TODO(omerkatz): add DCHECK(ephemeron_worklist_.empty()) when heap snapshot
  // covers all live objects.
}

void V8EmbedderGraphBuilder::VisitPersistentHandleInternal(
    v8::Local<v8::Object> v8_value,
    uint16_t class_id) {
  ScriptWrappable* traceable = ToScriptWrappable(v8_value);
  if (!traceable)
    return;
  Graph::Node* wrapper = node_builder_->GraphNode(v8_value);
  DomTreeState dom_tree_state =
      DomTreeStateFromWrapper(isolate_, class_id, v8_value);
  EmbedderNode* graph_node = node_builder_->GraphNode(
      traceable, traceable->NameInHeapSnapshot(), wrapper, dom_tree_state);
  State* const to_process_state = EnsureState(traceable, graph_node);
  const TraceDescriptor& descriptor =
      TraceDescriptorFor<ScriptWrappable>(traceable);
  switch (graph_node->GetDomTreeState()) {
    case DomTreeState::kAttached:
      CreateAndPushVisitationItem(nullptr, to_process_state, traceable,
                                  descriptor.callback);
      break;
    case DomTreeState::kDetached:
      detached_worklist_.push_back(
          std::unique_ptr<VisitationItem>{new VisitationItem(
              nullptr, to_process_state, traceable, descriptor.callback)});
      break;
    case DomTreeState::kUnknown:
      unknown_worklist_.push_back(
          std::unique_ptr<VisitationItem>{new VisitationItem(
              nullptr, to_process_state, traceable, descriptor.callback)});
      break;
  }
}

void V8EmbedderGraphBuilder::VisitTracedReference(
    const v8::TracedReference<v8::Value>& value) {
  const uint16_t class_id = value.WrapperClassId();
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;
  VisitPersistentHandleInternal(value.As<v8::Object>().Get(isolate_), class_id);
}

void V8EmbedderGraphBuilder::VisitTracedGlobalHandle(
    const v8::TracedGlobal<v8::Value>&) {
  CHECK(false) << "Blink does not use v8::TracedGlobal.";
}

void V8EmbedderGraphBuilder::VisitPersistentHandle(
    v8::Persistent<v8::Value>* value,
    uint16_t class_id) {
  if (class_id != WrapperTypeInfo::kNodeClassId &&
      class_id != WrapperTypeInfo::kObjectClassId)
    return;
  v8::Local<v8::Object> v8_value = v8::Local<v8::Object>::New(
      isolate_, v8::Persistent<v8::Object>::Cast(*value));
  VisitPersistentHandleInternal(v8_value, class_id);
}

void V8EmbedderGraphBuilder::Visit(
    const TraceWrapperV8Reference<v8::Value>& traced_wrapper) {
  // Add an edge from the current parent to the V8 object.
  v8::Local<v8::Value> v8_value = traced_wrapper.NewLocal(isolate_);
  if (!v8_value.IsEmpty()) {
    State* parent = GetStateNotNull(current_parent_);
    graph_->AddEdge(parent->GetOrCreateNode(node_builder_),
                    node_builder_->GraphNode(v8_value));
  }
}

void V8EmbedderGraphBuilder::VisitRoot(void* object,
                                       TraceDescriptor wrapper_descriptor,
                                       const base::Location& location) {
  // Extract edge name if |location| is set.
  if (location.has_source_info()) {
    const void* traceable = wrapper_descriptor.base_object_payload;
    State* const parent = GetStateNotNull(current_parent_);
    State* const current = GetOrCreateState(
        traceable, HeapObjectHeader::FromPayload(traceable)->Name(),
        parent->GetDomTreeState());
    parent->AddEdge(current, location.ToString());
  }
  Visit(object, wrapper_descriptor);
}

void V8EmbedderGraphBuilder::Visit(void* object,
                                   TraceDescriptor wrapper_descriptor) {
  if (trace_keys_scope_) {
    trace_keys_scope_->SetKey(object);
    return;
  }
  const void* traceable = wrapper_descriptor.base_object_payload;
  const GCInfo* info = GCInfoTable::Get().GCInfoFromIndex(
      HeapObjectHeader::FromPayload(traceable)->GcInfoIndex());
  HeapObjectName name = info->name(traceable);

  State* const parent = GetStateNotNull(current_parent_);
  State* const current =
      GetOrCreateState(traceable, name.value, parent->GetDomTreeState());
  if (current->IsPending()) {
    if (parent->HasNode()) {
      // Backedge in currently processed graph.
      AddEdge(parent, current);
    }
    return;
  }

  // Immediately materialize the node if it is not hidden.
  if (!name.name_is_hidden) {
    current->GetOrCreateNode(node_builder_);
  }

  // Propagate the parent's DomTreeState down to the current state.
  current->UpdateDomTreeState(parent->GetDomTreeState());

  if (!current->IsVisited()) {
    CreateAndPushVisitationItem(parent, current, traceable, info->trace);
  } else {
    // Edge into an already processed subgraph.
    if (current->HasNode()) {
      // Create an edge in case the current node has already been visited.
      AddEdge(parent, current);
    }
  }
}

void V8EmbedderGraphBuilder::AddEdge(State* parent, State* current) {
  EmbedderNode* parent_node = parent->GetOrCreateNode(node_builder_);
  EmbedderNode* current_node = current->GetOrCreateNode(node_builder_);
  if (parent_node->IsRootNode()) {
    const std::string edge_name = parent->EdgeName(current);
    if (!edge_name.empty()) {
      // V8's API is based on raw C strings. Allocate and temporarily keep the
      // edge name alive from the corresponding node.
      const size_t len = edge_name.length();
      char* raw_location_string = new char[len + 1];
      strncpy(raw_location_string, edge_name.c_str(), len);
      raw_location_string[len] = 0;
      std::unique_ptr<const char> holder(raw_location_string);
      graph_->AddEdge(parent_node, current_node, holder.get());
      static_cast<EmbedderRootNode*>(parent_node)
          ->AddEdgeName(std::move(holder));
      return;
    }
  }
  graph_->AddEdge(parent_node, current_node);
}

void V8EmbedderGraphBuilder::VisitBackingStoreStrongly(void* object,
                                                       void** object_slot,
                                                       TraceDescriptor desc) {
  if (!object)
    return;
  desc.callback(this, desc.base_object_payload);
}

void V8EmbedderGraphBuilder::VisitBackingStoreWeakly(
    void* object,
    void** object_slot,
    TraceDescriptor strong_desc,
    TraceDescriptor weak_desc,
    WeakCallback,
    void*) {
  // Only ephemerons have weak callbacks.
  if (weak_desc.callback) {
    // Heap snapshot is always run after a GC so we know there are no dead
    // entries in the backing store, thus it safe to trace it strongly.
    VisitBackingStoreStrongly(object, object_slot, strong_desc);
  }
}

bool V8EmbedderGraphBuilder::VisitEphemeronKeyValuePair(
    void* key,
    void* value,
    EphemeronTracingCallback key_trace_callback,
    EphemeronTracingCallback value_trace_callback) {
  ephemeron_worklist_.push_back(std::unique_ptr<EphemeronItem>{
      new EphemeronItem(key, value, key_trace_callback, value_trace_callback)});
  return true;
}

void V8EmbedderGraphBuilder::VisitPendingActivities() {
  // Ownership of the new node is transferred to the graph_.
  EmbedderNode* root =
      static_cast<EmbedderNode*>(graph_->AddNode(std::unique_ptr<Graph::Node>(
          new EmbedderRootNode("Pending activities"))));
  EnsureRootState(root);
  ParentScope parent(this, root);
  ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, this);
}

void V8EmbedderGraphBuilder::VisitBlinkRoots() {
  {
    EmbedderNode* root = static_cast<EmbedderNode*>(graph_->AddNode(
        std::unique_ptr<Graph::Node>(new EmbedderRootNode("Blink roots"))));
    EnsureRootState(root);
    ParentScope parent(this, root);
    ThreadState::Current()->GetPersistentRegion()->TraceNodes(this);
  }
  {
    EmbedderNode* root =
        static_cast<EmbedderNode*>(graph_->AddNode(std::unique_ptr<Graph::Node>(
            new EmbedderRootNode("Blink cross-thread roots"))));
    EnsureRootState(root);
    ParentScope parent(this, root);
    MutexLocker persistent_lock(ProcessHeap::CrossThreadPersistentMutex());
    ProcessHeap::GetCrossThreadPersistentRegion().TraceNodes(this);
  }
}

void V8EmbedderGraphBuilder::VisitTransitiveClosure() {
  // The following loop process the worklist and ephemerons. Since regular
  // tracing can record new ephemerons, and tracing an ephemeron can add
  // items to the regular worklist, we need to repeatedly process the worklist
  // until a fixed point is reached.

  // Because snapshots are processed in stages, there may be ephemerons that
  // where key's do not have yet a state associated with them which prohibits
  // them from being processed. Such ephemerons are stashed for later
  // processing.
  bool processed_ephemerons;
  do {
    // Step 1: Go through all items in the worklist using depth-first search.
    while (!worklist_.empty()) {
      std::unique_ptr<WorklistItemBase> item = std::move(worklist_.back());
      worklist_.pop_back();
      item->Process(this);
    }

    // Step 2: Go through ephemeron items.
    processed_ephemerons = false;
    Deque<std::unique_ptr<EphemeronItem>> unprocessed_ephemerons_;
    //  Only process an ephemeron item if its key was already observed.
    while (!ephemeron_worklist_.empty()) {
      std::unique_ptr<EphemeronItem> item =
          std::move(ephemeron_worklist_.front());
      ephemeron_worklist_.pop_front();
      if (!item->Process(this)) {
        unprocessed_ephemerons_.push_back(std::move(item));
      } else {
        processed_ephemerons = true;
      }
    }
    ephemeron_worklist_.Swap(unprocessed_ephemerons_);
  } while (!worklist_.empty() || processed_ephemerons);
}

}  // namespace

void EmbedderGraphBuilder::BuildEmbedderGraphCallback(v8::Isolate* isolate,
                                                      v8::EmbedderGraph* graph,
                                                      void*) {
  // Synchronize with concurrent sweepers before taking a snapshot.
  ThreadState::Current()->CompleteSweep();

  NodeBuilder node_builder(graph);
  V8EmbedderGraphBuilder builder(isolate, graph, &node_builder);
  builder.BuildEmbedderGraph();
}

}  // namespace blink
