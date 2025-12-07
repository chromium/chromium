// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"

#include <memory>

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_type_builder_helper.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"

namespace blink {

namespace {

// Send node updates to the frontend not more often than once within the
// `kNodeSyncThrottlePeriod` time frame.
static const base::TimeDelta kNodeSyncThrottlePeriod = base::Milliseconds(250);
// Interval at which we check if there is a need to schedule visual updates.
static const base::TimeDelta kVisualUpdateCheckInterval =
    kNodeSyncThrottlePeriod + base::Milliseconds(10);

void AddChildren(AXObject& ax_object,
                 bool follow_ignored,
                 std::unique_ptr<protocol::Array<AXNode>>& nodes,
                 AXObjectCacheImpl& cache) {
  HeapVector<Member<AXObject>> reachable;
  reachable.AppendRange(ax_object.ChildrenIncludingIgnored().rbegin(),
                        ax_object.ChildrenIncludingIgnored().rend());

  while (!reachable.empty()) {
    AXObject* descendant = reachable.back();
    reachable.pop_back();
    if (descendant->IsDetached()) {
      continue;
    }

    // If the node is ignored or has no corresponding DOM node, we include
    // another layer of children.
    if (follow_ignored &&
        (descendant->IsIgnoredButIncludedInTree() || !descendant->GetNode())) {
      reachable.AppendRange(descendant->ChildrenIncludingIgnored().rbegin(),
                            descendant->ChildrenIncludingIgnored().rend());
    }
    auto child_node = BuildProtocolAXNodeForAXObject(*descendant);
    nodes->emplace_back(std::move(child_node));
  }
}

void AddAncestors(AXObject& first_ancestor,
                  AXObject* inspected_ax_object,
                  std::unique_ptr<protocol::Array<AXNode>>& nodes,
                  AXObjectCacheImpl& cache) {
  std::unique_ptr<AXNode> first_parent_node_object =
      BuildProtocolAXNodeForAXObject(first_ancestor);
  // Since the inspected node is ignored it is missing from the first ancestors
  // childIds. We therefore add it to maintain the tree structure:
  if (!inspected_ax_object || inspected_ax_object->IsIgnored()) {
    auto child_ids = std::make_unique<protocol::Array<AXNodeId>>();
    auto* existing_child_ids = first_parent_node_object->getChildIds(nullptr);

    // put the ignored node first regardless of DOM structure.
    child_ids->insert(
        child_ids->begin(),
        String::Number(inspected_ax_object ? inspected_ax_object->AXObjectID()
                                           : kIDForInspectedNodeWithNoAXNode));
    if (existing_child_ids) {
      for (auto id : *existing_child_ids) {
        child_ids->push_back(id);
      }
    }
    first_parent_node_object->setChildIds(std::move(child_ids));
  }
  nodes->emplace_back(std::move(first_parent_node_object));
  AXObject* ancestor = first_ancestor.ParentObjectIncludedInTree();
  while (ancestor) {
    std::unique_ptr<AXNode> parent_node_object =
        BuildProtocolAXNodeForAXObject(*ancestor);
    nodes->emplace_back(std::move(parent_node_object));
    ancestor = ancestor->ParentObjectIncludedInTree();
  }
}

std::unique_ptr<protocol::Array<AXNode>> WalkAXNodesToDepth(
    AXObjectCacheImpl& cache,
    int max_depth) {
  std::unique_ptr<protocol::Array<AXNode>> nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  Deque<std::pair<AXID, int>> id_depths;
  id_depths.emplace_back(cache.Root()->AXObjectID(), 1);
  nodes->emplace_back(BuildProtocolAXNodeForAXObject(*cache.Root()));

  while (!id_depths.empty()) {
    std::pair<AXID, int> id_depth = id_depths.front();
    id_depths.pop_front();
    AXObject* ax_object = cache.ObjectFromAXID(id_depth.first);
    if (!ax_object)
      continue;
    AddChildren(*ax_object, true, nodes, cache);

    const AXObject::AXObjectVector& children = ax_object->UnignoredChildrenSlow();

    for (auto& child_ax_object : children) {
      int depth = id_depth.second;
      if (max_depth == -1 || depth < max_depth) {
        id_depths.emplace_back(child_ax_object->AXObjectID(), depth + 1);
      }
    }
  }

  return nodes;
}

}  // namespace

using EnabledAgentsMultimap =
    HeapHashMap<WeakMember<LocalFrame>,
                Member<GCedHeapHashSet<Member<InspectorAccessibilityAgent>>>>;

EnabledAgentsMultimap& EnabledAgents() {
  using EnabledAgentsMultimapHolder = DisallowNewWrapper<EnabledAgentsMultimap>;
  DEFINE_STATIC_LOCAL(Persistent<EnabledAgentsMultimapHolder>, holder,
                      (MakeGarbageCollected<EnabledAgentsMultimapHolder>()));
  return holder->Value();
}

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMAgent* dom_agent)
    : inspected_frames_(inspected_frames),
      dom_agent_(dom_agent),
      enabled_(&agent_state_, /*default_value=*/false) {}

protocol::Response InspectorAccessibilityAgent::getPartialAXTree(
    std::optional<int> dom_node_id,
    std::optional<int> backend_node_id,
    std::optional<String> object_id,
    std::optional<bool> fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  Node* dom_node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(dom_node_id, backend_node_id, object_id, dom_node);
  if (!response.IsSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return protocol::Response::ServerError("Frame is detached.");

  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(cache);

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  AXObject* inspected_ax_object = cache.Get(dom_node);
  *nodes = std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  if (inspected_ax_object) {
    (*nodes)->emplace_back(
        BuildProtocolAXNodeForAXObject(*inspected_ax_object));
  } else {
    (*nodes)->emplace_back(BuildProtocolAXNodeForDOMNodeWithNoAXNode(
        IdentifiersFactory::IntIdForNode(dom_node)));
  }

  if (!fetch_relatives.value_or(true)) {
    return protocol::Response::Success();
  }

  if (inspected_ax_object && !inspected_ax_object->IsIgnored())
    AddChildren(*inspected_ax_object, true, *nodes, cache);

  AXObject* parent_ax_object;
  if (inspected_ax_object) {
    parent_ax_object = inspected_ax_object->ParentObjectIncludedInTree();
  } else {
    // Walk up parents until an AXObject can be found.
    auto* shadow_root = DynamicTo<ShadowRoot>(dom_node);
    Node* parent_node = shadow_root ? &shadow_root->host()
                                    : FlatTreeTraversal::Parent(*dom_node);
    parent_ax_object = cache.Get(parent_node);
    while (parent_node && !parent_ax_object) {
      shadow_root = DynamicTo<ShadowRoot>(parent_node);
      parent_node = shadow_root ? &shadow_root->host()
                                : FlatTreeTraversal::Parent(*parent_node);
      parent_ax_object = cache.Get(parent_node);
    }
  }
  if (!parent_ax_object)
    return protocol::Response::Success();
  AddAncestors(*parent_ax_object, inspected_ax_object, *nodes, cache);

  return protocol::Response::Success();
}

LocalFrame* InspectorAccessibilityAgent::FrameFromIdOrRoot(
    const std::optional<String>& frame_id) {
  if (frame_id.has_value()) {
    return IdentifiersFactory::FrameById(inspected_frames_.Get(),
                                         frame_id.value());
  }
  return inspected_frames_->Root();
}

protocol::Response InspectorAccessibilityAgent::getFullAXTree(
    std::optional<int> depth,
    std::optional<String> frame_id,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();
  if (document->View()->NeedsLayout() || document->NeedsLayoutTreeUpdate())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();
  ScopedFreezeAXCache freeze(cache);

  *nodes = WalkAXNodesToDepth(cache, depth.value_or(-1));

  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::getRootAXNode(
    std::optional<String> frame_id,
    std::unique_ptr<AXNode>* node) {
  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();
  auto& root = *cache.Root();

  ScopedFreezeAXCache freeze(cache);

  *node = BuildProtocolAXNodeForAXObject(root);
  nodes_requested_.insert(root.AXObjectID());

  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::getAXNodeAndAncestors(
    std::optional<int> dom_node_id,
    std::optional<int> backend_node_id,
    std::optional<String> object_id,
    std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
        out_nodes) {
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  Node* dom_node = nullptr;
  protocol::Response response =
      dom_agent_->AssertNode(dom_node_id, backend_node_id, object_id, dom_node);
  if (!response.IsSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return protocol::Response::ServerError("Frame is detached.");

  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  AXObject* ax_object = cache.Get(dom_node);

  ScopedFreezeAXCache freeze(cache);

  *out_nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  if (!ax_object) {
    (*out_nodes)
        ->emplace_back(BuildProtocolAXNodeForDOMNodeWithNoAXNode(
            IdentifiersFactory::IntIdForNode(dom_node)));
    return protocol::Response::Success();
  }

  do {
    nodes_requested_.insert(ax_object->AXObjectID());
    std::unique_ptr<AXNode> ancestor =
        BuildProtocolAXNodeForAXObject(*ax_object);
    (*out_nodes)->emplace_back(std::move(ancestor));
    ax_object = ax_object->ParentObjectIncludedInTree();
  } while (ax_object);

  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::getChildAXNodes(
    const String& in_id,
    std::optional<String> frame_id,
    std::unique_ptr<protocol::Array<protocol::Accessibility::AXNode>>*
        out_nodes) {
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "Accessibility has not been enabled.");
  }

  LocalFrame* frame = FrameFromIdOrRoot(frame_id);
  if (!frame) {
    return protocol::Response::InvalidParams(
        "Frame with the given frameId is not found.");
  }

  Document* document = frame->GetDocument();
  if (!document)
    return protocol::Response::InternalError();

  auto& cache = AttachToAXObjectCache(document);
  cache.UpdateAXForAllDocuments();

  ScopedFreezeAXCache freeze(cache);

  AXID ax_id = in_id.ToInt();
  AXObject* ax_object = cache.ObjectFromAXID(ax_id);

  if (!ax_object || ax_object->IsDetached())
    return protocol::Response::InvalidParams("Invalid ID");

  *out_nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  AddChildren(*ax_object, /* follow_ignored */ true, *out_nodes, cache);

  for (const auto& child : **out_nodes)
    nodes_requested_.insert(child->getNodeId().ToInt());

  return protocol::Response::Success();
}

void InspectorAccessibilityAgent::queryAXTree(
    std::optional<int> dom_node_id,
    std::optional<int> backend_node_id,
    std::optional<String> object_id,
    std::optional<String> accessible_name,
    std::optional<String> role,
    std::unique_ptr<QueryAXTreeCallback> callback) {
  Node* root_dom_node = nullptr;
  protocol::Response response = dom_agent_->AssertNode(
      dom_node_id, backend_node_id, object_id, root_dom_node);
  if (!response.IsSuccess()) {
    callback->sendFailure(response);
    return;
  }

  // Shadow roots are missing from a11y tree.
  // We start searching the host element instead as a11y tree does not
  // care about shadow roots.
  if (root_dom_node->IsShadowRoot()) {
    root_dom_node = root_dom_node->OwnerShadowHost();
  }
  if (!root_dom_node) {
    callback->sendFailure(
        protocol::Response::InvalidParams("Root DOM node could not be found"));
    return;
  }

  Document& document = root_dom_node->GetDocument();
  auto& cache = AttachToAXObjectCache(&document);
  cache.UpdateAXForAllDocuments();

  // ScheduleAXUpdateWithCallback() ensures the lifecycle doesn't get stalled,
  // and therefore ensures we get the callback as soon as a11y is clean again.
  cache.ScheduleAXUpdateWithCallback(BindOnce(
      &InspectorAccessibilityAgent::CompleteQuery, WrapWeakPersistent(this),
      WrapWeakPersistent(root_dom_node), std::move(accessible_name),
      std::move(role), std::move(callback)));
}

void InspectorAccessibilityAgent::CompleteQuery(
    Node* root_dom_node,
    std::optional<String> accessible_name,
    std::optional<String> role,
    std::unique_ptr<QueryAXTreeCallback> callback) {
  if (!root_dom_node) {
    return callback->sendFailure(protocol::Response::ServerError(
        "Root DOM node was GC'ed while the query was in-flight."));
  }
  // Shadow roots are missing from a11y tree.
  // We start searching the host element instead as a11y tree does not
  // care about shadow roots.
  if (root_dom_node->IsShadowRoot())
    root_dom_node = root_dom_node->OwnerShadowHost();
  if (!root_dom_node) {
    callback->sendFailure(
        protocol::Response::InvalidParams("Root DOM node could not be found"));
    return;
  }
  Document& document = root_dom_node->GetDocument();

  document.UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());
  auto& cache = AttachToAXObjectCache(&document);
  ScopedFreezeAXCache freeze(cache);

  std::unique_ptr<protocol::Array<AXNode>> nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();
  AXObject* root_ax_node = cache.Get(root_dom_node);

  HeapVector<Member<AXObject>> reachable;
  if (root_ax_node)
    reachable.push_back(root_ax_node);

  while (!reachable.empty()) {
    AXObject* ax_object = reachable.back();
    if (ax_object->IsDetached() ||
        !ax_object->IsIncludedInTree()) {
      reachable.pop_back();
      continue;
    }
    ui::AXNodeData node_data;
    ax_object->Serialize(&node_data, ui::kAXModeInspector);
    reachable.pop_back();
    const AXObject::AXObjectVector& children =
        ax_object->ChildrenIncludingIgnored();
    reachable.AppendRange(children.rbegin(), children.rend());

    const bool ignored = ax_object->IsIgnored();
    // if querying by name: skip if name of current object does not match.
    // For now, we need to handle names of ignored nodes separately, since they
    // do not get a name assigned when serializing to AXNodeData.
    if (ignored && accessible_name.has_value() &&
        accessible_name.value() != ax_object->ComputedName()) {
      continue;
    }
    if (!ignored && accessible_name.has_value() &&
        accessible_name.value().Utf8() !=
            node_data.GetStringAttribute(
                ax::mojom::blink::StringAttribute::kName)) {
      continue;
    }

    // if querying by role: skip if role of current object does not match.
    if (role.has_value() &&
        role.value() != AXObject::RoleName(node_data.role)) {
      continue;
    }

    // both name and role are OK, so we can add current object to the result.
    nodes->push_back(BuildProtocolAXNodeForAXObject(
        *ax_object, /* force_name_and_role */ true));
  }

  callback->sendSuccess(std::move(nodes));
}

void InspectorAccessibilityAgent::AXReadyCallback(Document& document) {
  ProcessPendingDirtyNodes(document);
  if (load_complete_needs_processing_.Contains(&document) &&
      document.IsLoadCompleted()) {
    load_complete_needs_processing_.erase(&document);
    AXObjectCache* cache = document.ExistingAXObjectCache();
    CHECK(cache);
    AXObject* root = cache->Root();
    CHECK(root);
    dirty_nodes_.clear();
    nodes_requested_.clear();
    nodes_requested_.insert(root->AXObjectID());
    ScopedFreezeAXCache freeze(*cache);
    GetFrontend()->loadComplete(BuildProtocolAXNodeForAXObject(*root));
  }
}

void InspectorAccessibilityAgent::ProcessPendingDirtyNodes(Document& document) {
  auto now = base::Time::Now();

  if (!last_sync_times_.Contains(&document))
    last_sync_times_.insert(&document, now);
  else if (now - last_sync_times_.at(&document) < kNodeSyncThrottlePeriod)
    return;
  else
    last_sync_times_.at(&document) = now;

  if (!dirty_nodes_.Contains(&document))
    return;
  // Sometimes, computing properties for an object while serializing will
  // mark other objects dirty. This makes us re-enter this function.
  // To make this benign, we use a copy of dirty_nodes_ when iterating.
  Member<GCedHeapHashSet<WeakMember<AXObject>>> dirty_nodes =
      dirty_nodes_.Take(&document);
  auto nodes =
      std::make_unique<protocol::Array<protocol::Accessibility::AXNode>>();

  CHECK(document.ExistingAXObjectCache());
  ScopedFreezeAXCache freeze(*document.ExistingAXObjectCache());
  for (AXObject* changed_node : *dirty_nodes) {
    if (!changed_node->IsDetached())
      nodes->push_back(BuildProtocolAXNodeForAXObject(*changed_node));
  }
  GetFrontend()->nodesUpdated(std::move(nodes));
}

void InspectorAccessibilityAgent::ScheduleAXUpdateIfNeeded(TimerBase*,
                                                           Document* document) {
  DCHECK(document);

  if (!dirty_nodes_.Contains(document))
    return;

  // Scheduling an AX update for the cache will schedule it for both the main
  // document, and the popup document (if present).
  if (auto* cache = document->ExistingAXObjectCache()) {
    cache->ScheduleAXUpdate();
  }
}

void InspectorAccessibilityAgent::ScheduleAXChangeNotification(
    Document* document) {
  DCHECK(document);
  if (!timers_.Contains(document)) {
    timers_.insert(document,
                   MakeGarbageCollected<DisallowNewWrapper<DocumentTimer>>(
                       document, this,
                       &InspectorAccessibilityAgent::ScheduleAXUpdateIfNeeded));
  }
  DisallowNewWrapper<DocumentTimer>* timer = timers_.at(document);
  if (!timer->Value().IsActive())
    timer->Value().StartOneShot(kVisualUpdateCheckInterval, FROM_HERE);
}

void InspectorAccessibilityAgent::AXEventFired(AXObject* ax_object,
                                               ax::mojom::blink::Event event) {
  if (!enabled_.Get())
    return;
  DCHECK(ax_object->IsIncludedInTree());

  switch (event) {
    case ax::mojom::blink::Event::kLoadComplete: {
      // Will be handled in AXReadyCallback().
      load_complete_needs_processing_.insert(ax_object->GetDocument());
    } break;
    case ax::mojom::blink::Event::kLocationChanged:
      // Since we do not serialize location data we can ignore changes to this.
      break;
    default:
      MarkAXObjectDirty(ax_object);
      ScheduleAXChangeNotification(ax_object->GetDocument());
      break;
  }
}

bool InspectorAccessibilityAgent::MarkAXObjectDirty(AXObject* ax_object) {
  if (!nodes_requested_.Contains(ax_object->AXObjectID()))
    return false;
  Document* document = ax_object->GetDocument();
  auto inserted = dirty_nodes_.insert(document, nullptr);
  if (inserted.is_new_entry) {
    inserted.stored_value->value =
        MakeGarbageCollected<GCedHeapHashSet<WeakMember<AXObject>>>();
  }
  return inserted.stored_value->value->insert(ax_object).is_new_entry;
}

void InspectorAccessibilityAgent::AXObjectModified(AXObject* ax_object,
                                                   bool subtree) {
  if (!enabled_.Get())
    return;
  DCHECK(ax_object->IsIncludedInTree());
  if (subtree) {
    HeapVector<Member<AXObject>> reachable;
    reachable.push_back(ax_object);
    while (!reachable.empty()) {
      AXObject* descendant = reachable.back();
      reachable.pop_back();
      DCHECK(descendant->IsIncludedInTree());
      if (!MarkAXObjectDirty(descendant))
        continue;
      const AXObject::AXObjectVector& children =
          descendant->ChildrenIncludingIgnored();
      reachable.AppendRange(children.rbegin(), children.rend());
    }
  } else {
    MarkAXObjectDirty(ax_object);
  }
  ScheduleAXChangeNotification(ax_object->GetDocument());
}

void InspectorAccessibilityAgent::EnableAndReset() {
  enabled_.Set(true);
  LocalFrame* frame = inspected_frames_->Root();
  if (!EnabledAgents().Contains(frame)) {
    EnabledAgents().Set(
        frame, MakeGarbageCollected<
                   GCedHeapHashSet<Member<InspectorAccessibilityAgent>>>());
  }
  EnabledAgents().find(frame)->value->insert(this);
  for (auto& context : document_to_context_map_.Values()) {
    auto& cache = To<AXObjectCacheImpl>(context->GetAXObjectCache());
    cache.AddInspectorAgent(this);
  }
}

protocol::Response InspectorAccessibilityAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return protocol::Response::Success();
}

protocol::Response InspectorAccessibilityAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(false);
  for (auto& document : document_to_context_map_.Keys()) {
    DCHECK(document);
    // We do not rely on AXContext::GetAXObjectCache here, since it might
    // dereference nullptrs and requires several preconditions to be checked.
    // Instead, we remove the agent from any document that has an existing
    // AXObjectCache.
    AXObjectCache* existing_cache = document->ExistingAXObjectCache();
    if (!existing_cache) {
      continue;
    }
    auto& cache = To<AXObjectCacheImpl>(*existing_cache);
    cache.RemoveInspectorAgent(this);
  }
  document_to_context_map_.clear();
  nodes_requested_.clear();
  dirty_nodes_.clear();
  LocalFrame* frame = inspected_frames_->Root();
  DCHECK(EnabledAgents().Contains(frame));
  auto it = EnabledAgents().find(frame);
  it->value->erase(this);
  if (it->value->empty())
    EnabledAgents().erase(frame);
  return protocol::Response::Success();
}

void InspectorAccessibilityAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

void InspectorAccessibilityAgent::ProvideTo(LocalFrame* frame) {
  if (!EnabledAgents().Contains(frame))
    return;
  for (InspectorAccessibilityAgent* agent :
       *EnabledAgents().find(frame)->value) {
    agent->AttachToAXObjectCache(frame->GetDocument());
  }
}

AXObjectCacheImpl& InspectorAccessibilityAgent::AttachToAXObjectCache(
    Document* document) {
  DCHECK(document);
  DCHECK(document->IsActive());
  if (!document_to_context_map_.Contains(document)) {
    auto context = std::make_unique<AXContext>(*document, ui::kAXModeComplete);
    document_to_context_map_.insert(document, std::move(context));
  }
  AXObjectCacheImpl* cache =
      To<AXObjectCacheImpl>(document->ExistingAXObjectCache());
  cache->AddInspectorAgent(this);
  return *cache;
}

void InspectorAccessibilityAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_agent_);
  visitor->Trace(document_to_context_map_);
  visitor->Trace(dirty_nodes_);
  visitor->Trace(timers_);
  visitor->Trace(last_sync_times_);
  visitor->Trace(load_complete_needs_processing_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
