/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/node.h"

#include <algorithm>

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/node_or_string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_root_node_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/child_node_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/template_content_document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"
#include "third_party/blink/renderer/core/dom/user_action_element_set.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/mutation_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/events/text_event.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/ui_event.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_customization_callbacks.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state_callback.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// We need to retain the scroll customization callbacks until the element
// they're associated with is destroyed. It would be simplest if the callbacks
// could be stored in ElementRareData, but we can't afford the space increase.
// Instead, keep the scroll customization callbacks here. The other option would
// be to store these callbacks on the Page or document, but that necessitates a
// bunch more logic for transferring the callbacks between Pages when elements
// are moved around.
ScrollCustomizationCallbacks& GetScrollCustomizationCallbacks() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollCustomizationCallbacks>,
                      scroll_customization_callbacks,
                      (MakeGarbageCollected<ScrollCustomizationCallbacks>()));
  return *scroll_customization_callbacks;
}

// TODO(crbug.com/545926): Unsafe hack to avoid triggering the
// ThreadRestrictionVerifier on StringImpl. This should be fixed completely, and
// we should always avoid accessing these strings from the impl thread.
// Currently code that calls into this method from the impl thread tries to make
// sure that the main thread is not running at this time.
void AppendUnsafe(StringBuilder& builder, const String& off_thread_string) {
  StringImpl* impl = off_thread_string.Impl();
  if (impl) {
    WTF::VisitCharacters(*impl, [&](const auto* chars, unsigned length) {
      builder.Append(chars, length);
    });
  }
}

}  // namespace

using ReattachHookScope = LayoutShiftTracker::ReattachHookScope;

struct SameSizeAsNode : EventTarget {
  uint32_t node_flags_;
  Member<void*> willbe_member_[4];
  Member<NodeData> member_;
#if !DCHECK_IS_ON()
  // Increasing size of Member increases size of Node.
  ASSERT_SIZE(Member<NodeData>, void*);
#endif  // !DCHECK_IS_ON()
};

ASSERT_SIZE(Node, SameSizeAsNode);

#if DUMP_NODE_STATISTICS
using WeakNodeSet = HeapHashSet<WeakMember<Node>>;
static WeakNodeSet& liveNodeSet() {
  DEFINE_STATIC_LOCAL(WeakNodeSet, set, (new WeakNodeSet));
  return set;
}
#endif

void Node::DumpStatistics() {
#if DUMP_NODE_STATISTICS
  size_t nodesWithRareData = 0;

  size_t elementNodes = 0;
  size_t attrNodes = 0;
  size_t textNodes = 0;
  size_t cdataNodes = 0;
  size_t commentNodes = 0;
  size_t piNodes = 0;
  size_t documentNodes = 0;
  size_t docTypeNodes = 0;
  size_t fragmentNodes = 0;
  size_t shadowRootNodes = 0;

  HashMap<String, size_t> perTagCount;

  size_t attributes = 0;
  size_t elementsWithAttributeStorage = 0;
  size_t elementsWithRareData = 0;
  size_t elementsWithNamedNodeMap = 0;

  {
    ScriptForbiddenScope forbidScriptDuringRawIteration;
    for (Node* node : liveNodeSet()) {
      if (node->hasRareData()) {
        ++nodesWithRareData;
        if (auto* element = DynamicTo<Element>(node)) {
          ++elementsWithRareData;
          if (element->hasNamedNodeMap())
            ++elementsWithNamedNodeMap;
        }
      }

      switch (node->getNodeType()) {
        case kElementNode: {
          ++elementNodes;

          // Tag stats
          auto* element = To<Element>(node);
          HashMap<String, size_t>::AddResult result =
              perTagCount.add(element->tagName(), 1);
          if (!result.isNewEntry)
            result.storedValue->value++;

          size_t attributeCount = element->attributesWithoutUpdate().size();
          if (attributeCount) {
            attributes += attributeCount;
            ++elementsWithAttributeStorage;
          }
          break;
        }
        case kAttributeNode: {
          ++attrNodes;
          break;
        }
        case kTextNode: {
          ++textNodes;
          break;
        }
        case kCdataSectionNode: {
          ++cdataNodes;
          break;
        }
        case kCommentNode: {
          ++commentNodes;
          break;
        }
        case kProcessingInstructionNode: {
          ++piNodes;
          break;
        }
        case kDocumentNode: {
          ++documentNodes;
          break;
        }
        case kDocumentTypeNode: {
          ++docTypeNodes;
          break;
        }
        case kDocumentFragmentNode: {
          if (node->isShadowRoot())
            ++shadowRootNodes;
          else
            ++fragmentNodes;
          break;
        }
      }
    }
  }

  std::stringstream perTagStream;
  for (const auto& entry : perTagCount)
    perTagStream << "  Number of <" << entry.key.utf8().data()
                 << "> tags: " << entry.value << "\n";

  LOG(INFO) << "\n"
            << "Number of Nodes: " << liveNodeSet().size() << "\n"
            << "Number of Nodes with RareData: " << nodesWithRareData << "\n\n"

            << "NodeType distribution:\n"
            << "  Number of Element nodes: " << elementNodes << "\n"
            << "  Number of Attribute nodes: " << attrNodes << "\n"
            << "  Number of Text nodes: " << textNodes << "\n"
            << "  Number of CDATASection nodes: " << cdataNodes << "\n"
            << "  Number of Comment nodes: " << commentNodes << "\n"
            << "  Number of ProcessingInstruction nodes: " << piNodes << "\n"
            << "  Number of Document nodes: " << documentNodes << "\n"
            << "  Number of DocumentType nodes: " << docTypeNodes << "\n"
            << "  Number of DocumentFragment nodes: " << fragmentNodes << "\n"
            << "  Number of ShadowRoot nodes: " << shadowRootNodes << "\n"

            << "Element tag name distibution:\n"
            << perTagStream.str()

            << "Attributes:\n"
            << "  Number of Attributes (non-Node and Node): " << attributes
            << " x " << sizeof(Attribute) << "Bytes\n"
            << "  Number of Elements with attribute storage: "
            << elementsWithAttributeStorage << " x " << sizeof(ElementData)
            << "Bytes\n"
            << "  Number of Elements with RareData: " << elementsWithRareData
            << "\n"
            << "  Number of Elements with NamedNodeMap: "
            << elementsWithNamedNodeMap << " x " << sizeof(NamedNodeMap)
            << "Bytes";
#endif
}

void Node::TrackForDebugging() {
#if DUMP_NODE_STATISTICS
  liveNodeSet().add(this);
#endif
}

Node::Node(TreeScope* tree_scope, ConstructionType type)
    : node_flags_(type),
      parent_or_shadow_host_node_(nullptr),
      tree_scope_(tree_scope),
      previous_(nullptr),
      next_(nullptr),
      data_(&NodeRenderingData::SharedEmptyData()) {
  DCHECK(tree_scope_ || type == kCreateDocument || type == kCreateShadowRoot);
#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
  TrackForDebugging();
#endif
  InstanceCounters::IncrementCounter(InstanceCounters::kNodeCounter);
  // Document is required for probe sink.
  if (tree_scope_)
    probe::NodeCreated(this);
}

Node::~Node() {
  InstanceCounters::DecrementCounter(InstanceCounters::kNodeCounter);
}

NodeRareData& Node::CreateRareData() {
  if (IsElementNode()) {
    data_ = MakeGarbageCollected<ElementRareData>(DataAsNodeRenderingData());
  } else {
    data_ = MakeGarbageCollected<NodeRareData>(DataAsNodeRenderingData());
  }

  DCHECK(data_);
  SetFlag(kHasRareDataFlag);
  return *RareData();
}

Node* Node::ToNode() {
  return this;
}

String Node::nodeValue() const {
  return String();
}

void Node::setNodeValue(const String&) {
  // By default, setting nodeValue has no effect.
}

ContainerNode* Node::parentNode() const {
  return IsShadowRoot() ? nullptr : ParentOrShadowHostNode();
}

NodeList* Node::childNodes() {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return EnsureRareData().EnsureNodeLists().EnsureChildNodeList(*this_node);
  return EnsureRareData().EnsureNodeLists().EnsureEmptyChildNodeList(*this);
}

Node* Node::PseudoAwarePreviousSibling() const {
  Element* parent = parentElement();
  if (!parent || previousSibling())
    return previousSibling();
  switch (GetPseudoId()) {
    case kPseudoIdAfter:
      if (Node* previous = parent->lastChild())
        return previous;
      FALLTHROUGH;
    case kPseudoIdNone:
      if (Node* previous = parent->GetPseudoElement(kPseudoIdBefore))
        return previous;
      FALLTHROUGH;
    case kPseudoIdBefore:
      if (Node* previous = parent->GetPseudoElement(kPseudoIdMarker))
        return previous;
      FALLTHROUGH;
    case kPseudoIdMarker:
      break;
    default:
      NOTREACHED();
  }
  return nullptr;
}

Node* Node::PseudoAwareNextSibling() const {
  Element* parent = parentElement();
  if (!parent || nextSibling())
    return nextSibling();
  switch (GetPseudoId()) {
    case kPseudoIdMarker:
      if (Node* next = parent->GetPseudoElement(kPseudoIdBefore))
        return next;
      FALLTHROUGH;
    case kPseudoIdBefore:
      if (parent->HasChildren())
        return parent->firstChild();
      FALLTHROUGH;
    case kPseudoIdNone:
      if (Node* next = parent->GetPseudoElement(kPseudoIdAfter))
        return next;
      FALLTHROUGH;
    case kPseudoIdAfter:
      break;
    default:
      NOTREACHED();
  }
  return nullptr;
}

Node* Node::PseudoAwareFirstChild() const {
  if (const auto* current_element = DynamicTo<Element>(this)) {
    if (Node* first = current_element->GetPseudoElement(kPseudoIdMarker))
      return first;
    if (Node* first = current_element->GetPseudoElement(kPseudoIdBefore))
      return first;
    if (Node* first = current_element->firstChild())
      return first;
    return current_element->GetPseudoElement(kPseudoIdAfter);
  }

  return firstChild();
}

Node* Node::PseudoAwareLastChild() const {
  if (const auto* current_element = DynamicTo<Element>(this)) {
    if (Node* last = current_element->GetPseudoElement(kPseudoIdAfter))
      return last;
    if (Node* last = current_element->lastChild())
      return last;
    if (Node* last = current_element->GetPseudoElement(kPseudoIdBefore))
      return last;
    return current_element->GetPseudoElement(kPseudoIdMarker);
  }

  return lastChild();
}

Node& Node::TreeRoot() const {
  if (IsInTreeScope())
    return ContainingTreeScope().RootNode();
  const Node* node = this;
  while (node->parentNode())
    node = node->parentNode();
  return const_cast<Node&>(*node);
}

Node* Node::getRootNode(const GetRootNodeOptions* options) const {
  return (options->hasComposed() && options->composed())
             ? &ShadowIncludingRoot()
             : &TreeRoot();
}

void Node::setDistributeScroll(V8ScrollStateCallback* scroll_state_callback,
                               const String& native_scroll_behavior) {
  GetScrollCustomizationCallbacks().SetDistributeScroll(
      this, ScrollStateCallbackV8Impl::Create(scroll_state_callback,
                                              native_scroll_behavior));
}

void Node::setApplyScroll(V8ScrollStateCallback* scroll_state_callback,
                          const String& native_scroll_behavior) {
  SetApplyScroll(ScrollStateCallbackV8Impl::Create(scroll_state_callback,
                                                   native_scroll_behavior));
}

void Node::SetApplyScroll(ScrollStateCallback* scroll_state_callback) {
  GetScrollCustomizationCallbacks().SetApplyScroll(this, scroll_state_callback);
}

void Node::RemoveApplyScroll() {
  GetScrollCustomizationCallbacks().RemoveApplyScroll(this);
}

ScrollStateCallback* Node::GetApplyScroll() {
  return GetScrollCustomizationCallbacks().GetApplyScroll(this);
}

void Node::NativeDistributeScroll(ScrollState& scroll_state) {
  if (scroll_state.FullyConsumed())
    return;

  scroll_state.distributeToScrollChainDescendant();

  // The scroll doesn't propagate, and we're currently scrolling an element
  // other than this one, prevent the scroll from propagating to this element.
  if (scroll_state.DeltaConsumedForScrollSequence() &&
      scroll_state.CurrentNativeScrollingNode() != this) {
    return;
  }

  const double delta_x = scroll_state.deltaX();
  const double delta_y = scroll_state.deltaY();

  CallApplyScroll(scroll_state);

  if (delta_x != scroll_state.deltaX() || delta_y != scroll_state.deltaY())
    scroll_state.SetCurrentNativeScrollingNode(this);
}

void Node::NativeApplyScroll(ScrollState& scroll_state) {
  if (!GetLayoutObject())
    return;

  // All elements in the scroll chain should be boxes. However, in a scroll
  // gesture sequence, the scroll chain is only computed on GestureScrollBegin.
  // The type of layout object of the nodes in the scroll chain can change
  // between GestureScrollUpdate and GestureScrollBegin (e.g. from script
  // setting one of the nodes to display:inline). If there is no box there will
  // not be a scrollable area to scroll, so just return.
  if (!GetLayoutObject()->IsBox())
    return;

  if (scroll_state.FullyConsumed())
    return;

  FloatSize delta(scroll_state.deltaX(), scroll_state.deltaY());

  if (delta.IsZero())
    return;

  // TODO: This should use updateStyleAndLayoutForNode.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kScroll);

  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(ToLayoutBox(GetLayoutObject()));
  if (!scrollable_area)
    return;
  LayoutBox* box_to_scroll = scrollable_area->GetLayoutBox();

  // TODO(bokan): This is a hack to fix https://crbug.com/977954. If we have a
  // non-default root scroller, scrolling from one of its siblings or a fixed
  // element will chain up to the root node without passing through the root
  // scroller. This should scroll the visual viewport (so we can still pan
  // while zoomed) but not by using the RootFrameViewport, which would cause
  // scrolling in the root scroller element. Implementing this on the main
  // thread is awkward since we assume only Nodes are scrollable but the
  // VisualViewport isn't a Node. See LTHI::ApplyScroll for the equivalent
  // behavior in CC.
  bool also_scroll_visual_viewport = GetDocument().GetFrame() &&
                                     GetDocument().GetFrame()->IsMainFrame() &&
                                     IsA<LayoutView>(box_to_scroll);
  DCHECK(!also_scroll_visual_viewport ||
         !box_to_scroll->IsGlobalRootScroller());

  ScrollResult result =
      scrollable_area->UserScroll(scroll_state.delta_granularity(), delta,
                                  ScrollableArea::ScrollCallback());

  // Also try scrolling the visual viewport if we're at the end of the scroll
  // chain.
  if (!result.DidScroll() && also_scroll_visual_viewport) {
    result = GetDocument().GetPage()->GetVisualViewport().UserScroll(
        scroll_state.delta_granularity(), delta,
        ScrollableArea::ScrollCallback());
  }

  if (!result.DidScroll())
    return;

  // FIXME: Native scrollers should only consume the scroll they
  // apply. See crbug.com/457765.
  scroll_state.ConsumeDeltaNative(delta.Width(), delta.Height());

  // We need to setCurrentNativeScrollingElement in both the
  // distributeScroll and applyScroll default implementations so
  // that if JS overrides one of these methods, but not the
  // other, this bookkeeping remains accurate.
  scroll_state.SetCurrentNativeScrollingNode(this);
}

void Node::CallDistributeScroll(ScrollState& scroll_state) {
  TRACE_EVENT0("input", "Node::CallDistributeScroll");
  ScrollStateCallback* callback =
      GetScrollCustomizationCallbacks().GetDistributeScroll(this);

  // TODO(bokan): Need to add tests before we allow calling custom callbacks
  // for non-touch modalities. For now, just call into the native callback but
  // allow the viewport scroll callback so we don't disable overscroll.
  // crbug.com/623079.
  bool disable_custom_callbacks = !scroll_state.isDirectManipulation() &&
                                  !GetDocument()
                                       .GetPage()
                                       ->GlobalRootScrollerController()
                                       .IsViewportScrollCallback(callback);

  bool is_global_root_scroller =
      GetLayoutObject() && GetLayoutObject()->IsGlobalRootScroller();

  disable_custom_callbacks |=
      !is_global_root_scroller &&
      RuntimeEnabledFeatures::ScrollCustomizationEnabled() &&
      !GetScrollCustomizationCallbacks().InScrollPhase(this);

  if (!callback || disable_custom_callbacks) {
    NativeDistributeScroll(scroll_state);
    return;
  }
  if (callback->GetNativeScrollBehavior() !=
      NativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
  if (callback->GetNativeScrollBehavior() !=
      NativeScrollBehavior::kDisableNativeScroll)
    NativeDistributeScroll(scroll_state);
  if (callback->GetNativeScrollBehavior() ==
      NativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
}

void Node::CallApplyScroll(ScrollState& scroll_state) {
  TRACE_EVENT0("input", "Node::CallApplyScroll");

  if (!GetDocument().GetPage()) {
    // We should always have a Page if we're scrolling. See
    // crbug.com/689074 for details.
    NOTREACHED();
    return;
  }

  ScrollStateCallback* callback =
      GetScrollCustomizationCallbacks().GetApplyScroll(this);

  // TODO(bokan): Need to add tests before we allow calling custom callbacks
  // for non-touch modalities. For now, just call into the native callback but
  // allow the viewport scroll callback so we don't disable overscroll.
  // crbug.com/623079.
  bool disable_custom_callbacks = !scroll_state.isDirectManipulation() &&
                                  !GetDocument()
                                       .GetPage()
                                       ->GlobalRootScrollerController()
                                       .IsViewportScrollCallback(callback);

  bool is_global_root_scroller =
      GetLayoutObject() && GetLayoutObject()->IsGlobalRootScroller();

  disable_custom_callbacks |=
      !is_global_root_scroller &&
      RuntimeEnabledFeatures::ScrollCustomizationEnabled() &&
      !GetScrollCustomizationCallbacks().InScrollPhase(this);

  if (!callback || disable_custom_callbacks) {
    NativeApplyScroll(scroll_state);
    return;
  }
  if (callback->GetNativeScrollBehavior() !=
      NativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
  if (callback->GetNativeScrollBehavior() !=
      NativeScrollBehavior::kDisableNativeScroll)
    NativeApplyScroll(scroll_state);
  if (callback->GetNativeScrollBehavior() ==
      NativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
}

void Node::WillBeginCustomizedScrollPhase(
    scroll_customization::ScrollDirection direction) {
  DCHECK(!GetScrollCustomizationCallbacks().InScrollPhase(this));
  LayoutBox* box = GetLayoutBox();
  if (!box)
    return;

  scroll_customization::ScrollDirection scroll_customization =
      box->Style()->ScrollCustomization();

  GetScrollCustomizationCallbacks().SetInScrollPhase(
      this, direction & scroll_customization);
}

void Node::DidEndCustomizedScrollPhase() {
  GetScrollCustomizationCallbacks().SetInScrollPhase(this, false);
}

Node* Node::insertBefore(Node* new_child,
                         Node* ref_child,
                         ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return this_node->InsertBefore(new_child, ref_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kHierarchyRequestError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::insertBefore(Node* new_child, Node* ref_child) {
  return insertBefore(new_child, ref_child, ASSERT_NO_EXCEPTION);
}

Node* Node::replaceChild(Node* new_child,
                         Node* old_child,
                         ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return this_node->ReplaceChild(new_child, old_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kHierarchyRequestError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::replaceChild(Node* new_child, Node* old_child) {
  return replaceChild(new_child, old_child, ASSERT_NO_EXCEPTION);
}

Node* Node::removeChild(Node* old_child, ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return this_node->RemoveChild(old_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotFoundError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::removeChild(Node* old_child) {
  return removeChild(old_child, ASSERT_NO_EXCEPTION);
}

Node* Node::appendChild(Node* new_child, ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return this_node->AppendChild(new_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kHierarchyRequestError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::appendChild(Node* new_child) {
  return appendChild(new_child, ASSERT_NO_EXCEPTION);
}

static bool IsNodeInNodes(
    const Node* const node,
    const HeapVector<NodeOrStringOrTrustedScript>& nodes) {
  for (const NodeOrStringOrTrustedScript& node_or_string : nodes) {
    if (node_or_string.IsNode() && node_or_string.GetAsNode() == node)
      return true;
  }
  return false;
}

static Node* FindViablePreviousSibling(
    const Node& node,
    const HeapVector<NodeOrStringOrTrustedScript>& nodes) {
  for (Node* sibling = node.previousSibling(); sibling;
       sibling = sibling->previousSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* FindViableNextSibling(
    const Node& node,
    const HeapVector<NodeOrStringOrTrustedScript>& nodes) {
  for (Node* sibling = node.nextSibling(); sibling;
       sibling = sibling->nextSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* NodeOrStringToNode(
    const NodeOrStringOrTrustedScript& node_or_string,
    Document& document,
    bool needs_trusted_types_check,
    ExceptionState& exception_state) {
  if (!needs_trusted_types_check) {
    // Without trusted type checks, we simply extract the string from whatever
    // constituent type we find.
    if (node_or_string.IsNode())
      return node_or_string.GetAsNode();
    if (node_or_string.IsTrustedScript()) {
      return Text::Create(document,
                          node_or_string.GetAsTrustedScript()->toString());
    }
    return Text::Create(document, node_or_string.GetAsString());
  }

  // With trusted type checks, we can process trusted script or non-text nodes
  // directly. Strings or text nodes need to be checked.
  if (node_or_string.IsNode() && !node_or_string.GetAsNode()->IsTextNode())
    return node_or_string.GetAsNode();

  if (node_or_string.IsTrustedScript()) {
    return Text::Create(document,
                        node_or_string.GetAsTrustedScript()->toString());
  }

  String string_value = node_or_string.IsString()
                            ? node_or_string.GetAsString()
                            : node_or_string.GetAsNode()->textContent();

  string_value = TrustedTypesCheckForScript(
      string_value, document.GetExecutionContext(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  return Text::Create(document, string_value);
}

// Returns nullptr if an exception was thrown.
static Node* ConvertNodesIntoNode(
    const Node* parent,
    const HeapVector<NodeOrStringOrTrustedScript>& nodes,
    Document& document,
    ExceptionState& exception_state) {
  bool needs_check = IsA<HTMLScriptElement>(parent) &&
                     document.GetExecutionContext() &&
                     document.GetExecutionContext()->RequireTrustedTypes();

  if (nodes.size() == 1)
    return NodeOrStringToNode(nodes[0], document, needs_check, exception_state);

  Node* fragment = DocumentFragment::Create(document);
  for (const NodeOrStringOrTrustedScript& node_or_string_or_trusted_script :
       nodes) {
    Node* node = NodeOrStringToNode(node_or_string_or_trusted_script, document,
                                    needs_check, exception_state);
    if (node)
      fragment->appendChild(node, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  return fragment;
}

void Node::Prepend(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                   ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  if (Node* node =
          ConvertNodesIntoNode(this, nodes, GetDocument(), exception_state))
    this_node->InsertBefore(node, firstChild(), exception_state);
}

void Node::Append(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                  ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  if (Node* node =
          ConvertNodesIntoNode(this, nodes, GetDocument(), exception_state))
    this_node->AppendChild(node, exception_state);
}

void Node::Before(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                  ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  auto* parent_node = DynamicTo<ContainerNode>(parent);
  if (!parent_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }
  Node* viable_previous_sibling = FindViablePreviousSibling(*this, nodes);
  if (Node* node =
          ConvertNodesIntoNode(parent, nodes, GetDocument(), exception_state)) {
    parent_node->InsertBefore(node,
                              viable_previous_sibling
                                  ? viable_previous_sibling->nextSibling()
                                  : parent->firstChild(),
                              exception_state);
  }
}

void Node::After(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                 ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  auto* parent_node = DynamicTo<ContainerNode>(parent);
  if (!parent_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }
  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  if (Node* node =
          ConvertNodesIntoNode(parent, nodes, GetDocument(), exception_state))
    parent_node->InsertBefore(node, viable_next_sibling, exception_state);
}

void Node::ReplaceWith(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                       ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  auto* parent_node = DynamicTo<ContainerNode>(parent);
  if (!parent_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  Node* node =
      ConvertNodesIntoNode(parent, nodes, GetDocument(), exception_state);
  if (exception_state.HadException())
    return;
  if (parent_node == parentNode())
    parent_node->ReplaceChild(node, this, exception_state);
  else
    parent_node->InsertBefore(node, viable_next_sibling, exception_state);
}

// https://dom.spec.whatwg.org/#dom-parentnode-replacechildren
void Node::ReplaceChildren(const HeapVector<NodeOrStringOrTrustedScript>& nodes,
                           ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  // 1. Let node be the result of converting nodes into a node given nodes and
  // this’s node document.
  Node* node =
      ConvertNodesIntoNode(this, nodes, GetDocument(), exception_state);
  if (exception_state.HadException())
    return;

  // 2. Ensure pre-insertion validity of node into this before null.
  if (!this_node->EnsurePreInsertionValidity(*node, nullptr, nullptr,
                                             exception_state))
    return;

  // 3. Replace all with node within this.
  ChildListMutationScope mutation(*this);
  while (Node* first_child = firstChild()) {
    removeChild(first_child, exception_state);
    if (exception_state.HadException())
      return;
  }

  appendChild(node, exception_state);
}

void Node::remove(ExceptionState& exception_state) {
  if (ContainerNode* parent = parentNode())
    parent->RemoveChild(this, exception_state);
}

void Node::remove() {
  remove(ASSERT_NO_EXCEPTION);
}

Node* Node::cloneNode(bool deep, ExceptionState& exception_state) const {
  // https://dom.spec.whatwg.org/#dom-node-clonenode

  // 1. If this is a shadow root, then throw a "NotSupportedError" DOMException.
  if (IsShadowRoot()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "ShadowRoot nodes are not clonable.");
    return nullptr;
  }

  // 2. Return a clone of this, with the clone children flag set if deep is
  // true, and the clone shadows flag set if this is a DocumentFragment whose
  // host is an HTML template element.
  auto* fragment = DynamicTo<DocumentFragment>(this);
  bool clone_shadows_flag = fragment && fragment->IsTemplateContent() &&
                            RuntimeEnabledFeatures::DeclarativeShadowDOMEnabled(
                                GetExecutionContext());
  return Clone(GetDocument(),
               deep ? (clone_shadows_flag ? CloneChildrenFlag::kCloneWithShadows
                                          : CloneChildrenFlag::kClone)
                    : CloneChildrenFlag::kSkip);
}

Node* Node::cloneNode(bool deep) const {
  return cloneNode(deep, ASSERT_NO_EXCEPTION);
}

void Node::normalize() {
  UpdateDistributionForFlatTreeTraversal();

  // Go through the subtree beneath us, normalizing all nodes. This means that
  // any two adjacent text nodes are merged and any empty text nodes are
  // removed.

  Node* node = this;
  while (Node* first_child = node->firstChild())
    node = first_child;
  while (node) {
    if (node == this)
      break;

    if (node->getNodeType() == kTextNode)
      node = To<Text>(node)->MergeNextSiblingNodesIfPossible();
    else
      node = NodeTraversal::NextPostOrder(*node);
  }
}

LayoutBox* Node::GetLayoutBox() const {
  LayoutObject* layout_object = GetLayoutObject();
  return layout_object && layout_object->IsBox() ? ToLayoutBox(layout_object)
                                                 : nullptr;
}

void Node::SetLayoutObject(LayoutObject* layout_object) {
  NodeRenderingData* node_layout_data =
      HasRareData() ? DataAsNodeRareData()->GetNodeRenderingData()
                    : DataAsNodeRenderingData();

  // Already pointing to a non empty NodeRenderingData so just set the pointer
  // to the new LayoutObject.
  if (!node_layout_data->IsSharedEmptyData()) {
    node_layout_data->SetLayoutObject(layout_object);
    return;
  }

  if (!layout_object)
    return;

  // Swap the NodeRenderingData to point to a new NodeRenderingData instead of
  // the static SharedEmptyData instance.
  DCHECK(!node_layout_data->GetComputedStyle());
  node_layout_data =
      MakeGarbageCollected<NodeRenderingData>(layout_object, nullptr);
  if (HasRareData()) {
    DataAsNodeRareData()->SetNodeRenderingData(node_layout_data);
  } else {
    data_ = node_layout_data;
  }
}

void Node::SetComputedStyle(scoped_refptr<const ComputedStyle> computed_style) {
  // We don't set computed style for text nodes.
  DCHECK(IsElementNode());

  NodeRenderingData* node_layout_data =
      HasRareData() ? DataAsNodeRareData()->GetNodeRenderingData()
                    : DataAsNodeRenderingData();

  // Already pointing to a non empty NodeRenderingData so just set the pointer
  // to the new LayoutObject.
  if (!node_layout_data->IsSharedEmptyData()) {
    node_layout_data->SetComputedStyle(computed_style);
    return;
  }

  if (!computed_style)
    return;

  // Ensure we only set computed style for elements which are not part of the
  // flat tree unless it's enforced for getComputedStyle().
  DCHECK(computed_style->IsEnsuredInDisplayNone() ||
         LayoutTreeBuilderTraversal::Parent(*this));

  // Swap the NodeRenderingData to point to a new NodeRenderingData instead of
  // the static SharedEmptyData instance.
  DCHECK(!node_layout_data->GetLayoutObject());
  node_layout_data =
      MakeGarbageCollected<NodeRenderingData>(nullptr, computed_style);
  if (HasRareData()) {
    DataAsNodeRareData()->SetNodeRenderingData(node_layout_data);
  } else {
    data_ = node_layout_data;
  }
}

LayoutBoxModelObject* Node::GetLayoutBoxModelObject() const {
  LayoutObject* layout_object = GetLayoutObject();
  return layout_object && layout_object->IsBoxModelObject()
             ? ToLayoutBoxModelObject(layout_object)
             : nullptr;
}

PhysicalRect Node::BoundingBox() const {
  if (GetLayoutObject())
    return PhysicalRect(GetLayoutObject()->AbsoluteBoundingBoxRect());
  return PhysicalRect();
}

IntRect Node::PixelSnappedBoundingBox() const {
  return PixelSnappedIntRect(BoundingBox());
}

PhysicalRect Node::BoundingBoxForScrollIntoView() const {
  if (GetLayoutObject()) {
    return GetLayoutObject()->AbsoluteBoundingBoxRectForScrollIntoView();
  }

  return PhysicalRect();
}

Node& Node::ShadowIncludingRoot() const {
  if (isConnected())
    return GetDocument();
  Node* root = const_cast<Node*>(this);
  while (Node* host = root->OwnerShadowHost())
    root = host;
  while (Node* ancestor = root->parentNode())
    root = ancestor;
  DCHECK(!root->OwnerShadowHost());
  return *root;
}

bool Node::IsClosedShadowHiddenFrom(const Node& other) const {
  if (!IsInShadowTree() || GetTreeScope() == other.GetTreeScope())
    return false;

  const TreeScope* scope = &GetTreeScope();
  for (; scope->ParentTreeScope(); scope = scope->ParentTreeScope()) {
    const ContainerNode& root = scope->RootNode();
    auto* shadow_root = DynamicTo<ShadowRoot>(root);
    if (shadow_root && !shadow_root->IsOpenOrV0())
      break;
  }

  for (TreeScope* other_scope = &other.GetTreeScope(); other_scope;
       other_scope = other_scope->ParentTreeScope()) {
    if (other_scope == scope)
      return false;
  }
  return true;
}

bool Node::NeedsDistributionRecalc() const {
  return ShadowIncludingRoot().ChildNeedsDistributionRecalc();
}

bool Node::MayContainLegacyNodeTreeWhereDistributionShouldBeSupported() const {
  if (isConnected() && !GetDocument().MayContainV0Shadow()) {
    // TODO(crbug.com/787717): Some built-in elements still use <content>
    // elements in their user-agent shadow roots. DCHECK() fails if such an
    // element is used.
    DCHECK(!GetDocument().ChildNeedsDistributionRecalc());
    return false;
  }
  return true;
}

void Node::UpdateDistributionForUnknownReasons() {
  UpdateDistributionInternal();
  // For the sake of safety, call RecalcSlotAssignments as well as
  // UpdateDistribution().
  if (isConnected())
    GetDocument().GetSlotAssignmentEngine().RecalcSlotAssignments();
}

void Node::UpdateDistributionInternal() {
  if (!MayContainLegacyNodeTreeWhereDistributionShouldBeSupported())
    return;
  // Extra early out to avoid spamming traces.
  if (isConnected() && !GetDocument().ChildNeedsDistributionRecalc())
    return;
  TRACE_EVENT0("blink", "Node::updateDistribution");
  ScriptForbiddenScope forbid_script;
  Node& root = ShadowIncludingRoot();
  if (root.ChildNeedsDistributionRecalc())
    root.RecalcDistribution();
}

void Node::RecalcDistribution() {
  DCHECK(ChildNeedsDistributionRecalc());

  if (GetShadowRoot())
    GetShadowRoot()->DistributeIfNeeded();

  DCHECK(ScriptForbiddenScope::IsScriptForbidden());
  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    if (child->ChildNeedsDistributionRecalc())
      child->RecalcDistribution();
  }

  if (ShadowRoot* root = GetShadowRoot()) {
    if (root->ChildNeedsDistributionRecalc())
      root->RecalcDistribution();
  }

  ClearChildNeedsDistributionRecalc();
}

void Node::SetIsLink(bool is_link) {
  SetFlag(is_link && !SVGImage::IsInSVGImage(To<Element>(this)), kIsLinkFlag);
}

void Node::SetNeedsStyleInvalidation() {
  DCHECK(IsContainerNode());
  SetFlag(kNeedsStyleInvalidationFlag);
  MarkAncestorsWithChildNeedsStyleInvalidation();
}

void Node::MarkAncestorsWithChildNeedsStyleInvalidation() {
  ScriptForbiddenScope forbid_script_during_raw_iteration;
  ContainerNode* ancestor = ParentOrShadowHostNode();
  bool parent_dirty = ancestor && ancestor->NeedsStyleInvalidation();
  for (; ancestor && !ancestor->ChildNeedsStyleInvalidation();
       ancestor = ancestor->ParentOrShadowHostNode()) {
    if (!ancestor->isConnected())
      return;
    ancestor->SetChildNeedsStyleInvalidation();
    if (ancestor->NeedsStyleInvalidation())
      break;
  }
  if (!isConnected())
    return;
  // If the parent node is already dirty, we can keep the same invalidation
  // root. The early return here is a performance optimization.
  if (parent_dirty)
    return;
  GetDocument().GetStyleEngine().UpdateStyleInvalidationRoot(ancestor, this);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void Node::MarkAncestorsWithChildNeedsDistributionRecalc() {
  ScriptForbiddenScope forbid_script_during_raw_iteration;
  for (Node* node = this; node && !node->ChildNeedsDistributionRecalc();
       node = node->ParentOrShadowHostNode()) {
    node->SetChildNeedsDistributionRecalc();
  }
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void Node::MarkSubtreeNeedsStyleRecalcForFontUpdates() {
  if (GetStyleChangeType() == kSubtreeStyleChange)
    return;

  if (IsElementNode()) {
    const ComputedStyle* style = GetComputedStyle();
    if (!style)
      return;

    // We require font-specific metrics to resolve length units 'ex' and 'ch',
    // and to compute the adjusted font size when 'font-size-adjust' is set. All
    // other style computations are unaffected by font loading.
    if (!NeedsStyleRecalc()) {
      if (style->DependsOnFontMetrics() ||
          To<Element>(this)->PseudoElementStylesDependOnFontMetrics()) {
        SetNeedsStyleRecalc(
            kLocalStyleChange,
            StyleChangeReasonForTracing::Create(style_change_reason::kFonts));
      }
    }

    if (Node* shadow_root = GetShadowRoot())
      shadow_root->MarkSubtreeNeedsStyleRecalcForFontUpdates();
  }

  for (Node* child = firstChild(); child; child = child->nextSibling())
    child->MarkSubtreeNeedsStyleRecalcForFontUpdates();
}

#if DCHECK_IS_ON()
namespace {
class AllowDirtyShadowV0TraversalScope {
  STACK_ALLOCATED();

 public:
  explicit AllowDirtyShadowV0TraversalScope(Document& document)
      : document_(&document),
        old_value_(document.AllowDirtyShadowV0Traversal()) {
    document.SetAllowDirtyShadowV0Traversal(true);
  }
  ~AllowDirtyShadowV0TraversalScope() {
    document_->SetAllowDirtyShadowV0Traversal(old_value_);
  }

 private:
  Document* document_;
  bool old_value_;
};
}  // namespace
#define ALLOW_DIRTY_SHADOW_V0_TRAVERSAL_SCOPE(document) \
  AllowDirtyShadowV0TraversalScope traversal_scope(document)
#else  // DCHECK_IS_ON()
#define ALLOW_DIRTY_SHADOW_V0_TRAVERSAL_SCOPE(document)
#endif  // DCHECK_IS_ON()

bool Node::ShouldSkipMarkingStyleDirty() const {
  if (GetComputedStyle())
    return false;

  ALLOW_DIRTY_SHADOW_V0_TRAVERSAL_SCOPE(GetDocument());

  // If we don't have a computed style, and our parent element does not have a
  // computed style it's not necessary to mark this node for style recalc.
  if (auto* parent = GetStyleRecalcParent()) {
    while (parent && !parent->CanParticipateInFlatTree())
      parent = parent->GetStyleRecalcParent();
    return !parent || !parent->GetComputedStyle();
  }
  // If this is the root element, and it does not have a computed style, we
  // still need to mark it for style recalc since it may change from
  // display:none. Otherwise, the node is not in the flat tree, and we can
  // skip marking it dirty.
  auto* root_element = GetDocument().documentElement();
  return root_element && root_element != this;
}

void Node::MarkAncestorsWithChildNeedsStyleRecalc() {
  ALLOW_DIRTY_SHADOW_V0_TRAVERSAL_SCOPE(GetDocument());
  ContainerNode* style_parent = GetStyleRecalcParent();
  bool parent_dirty = style_parent && style_parent->IsDirtyForStyleRecalc();
  ContainerNode* ancestor = style_parent;
  for (; ancestor && !ancestor->ChildNeedsStyleRecalc();
       ancestor = ancestor->GetStyleRecalcParent()) {
    if (!ancestor->isConnected())
      return;
    ancestor->SetChildNeedsStyleRecalc();
    if (ancestor->IsDirtyForStyleRecalc())
      break;
    // If we reach a locked ancestor, we should abort since the ancestor marking
    // will be done when the lock is committed.
    if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled()) {
      auto* ancestor_element = DynamicTo<Element>(ancestor);
      if (ancestor_element &&
          ancestor_element->ChildStyleRecalcBlockedByDisplayLock()) {
        break;
      }
    }
  }
  if (!isConnected())
    return;
  // If the parent node is already dirty, we can keep the same recalc root. The
  // early return here is a performance optimization.
  if (parent_dirty)
    return;
  // If we are outside the flat tree we should not update the recalc root
  // because we should not traverse those nodes from StyleEngine::RecalcStyle().
  if (const ComputedStyle* current_style = GetComputedStyle()) {
    if (current_style->IsEnsuredOutsideFlatTree())
      return;
  } else {
    while (style_parent && !style_parent->CanParticipateInFlatTree())
      style_parent = style_parent->GetStyleRecalcParent();
    if (style_parent) {
      if (const auto* parent_style = style_parent->GetComputedStyle()) {
        if (parent_style->IsEnsuredOutsideFlatTree())
          return;
      }
    }
  }
  // If we're in a locked subtree, then we should not update the style recalc
  // roots. These would be updated when we commit the lock. If we have locked
  // display locks somewhere in the document, we iterate up the ancestor chain
  // to check if we're in one such subtree.
  if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled() &&
      GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() >
          0) {
    for (auto* ancestor_copy = ancestor; ancestor_copy;
         ancestor_copy = ancestor_copy->GetStyleRecalcParent()) {
      auto* ancestor_copy_element = DynamicTo<Element>(ancestor_copy);
      if (ancestor_copy_element &&
          ancestor_copy_element->ChildStyleRecalcBlockedByDisplayLock()) {
        return;
      }
    }
  }

  GetDocument().GetStyleEngine().UpdateStyleRecalcRoot(ancestor, this);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

Element* Node::FlatTreeParentForChildDirty() const {
  if (IsPseudoElement())
    return ParentOrShadowHostElement();
  if (IsChildOfV1ShadowHost()) {
    if (auto* data = GetFlatTreeNodeData())
      return data->AssignedSlot();
    return nullptr;
  }
  if (IsInV0ShadowTree() || IsChildOfV0ShadowHost()) {
    if (ShadowRootWhereNodeCanBeDistributedForV0(*this)) {
      if (V0InsertionPoint* insertion_point =
              const_cast<V0InsertionPoint*>(ResolveReprojection(this))) {
        return insertion_point;
      }
      return nullptr;
    }
  }
  return ParentOrShadowHostElement();
}

void Node::MarkAncestorsWithChildNeedsReattachLayoutTree() {
  DCHECK(isConnected());
  Element* ancestor = GetReattachParent();
  bool parent_dirty = ancestor && ancestor->NeedsReattachLayoutTree();
  for (; ancestor && !ancestor->ChildNeedsReattachLayoutTree();
       ancestor = ancestor->GetReattachParent()) {
    ancestor->SetChildNeedsReattachLayoutTree();
    if (ancestor->NeedsReattachLayoutTree())
      break;
  }
  // If the parent node is already dirty, we can keep the same rebuild root. The
  // early return here is a performance optimization.
  if (parent_dirty)
    return;
  GetDocument().GetStyleEngine().UpdateLayoutTreeRebuildRoot(ancestor, this);
}

void Node::SetNeedsReattachLayoutTree() {
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(!GetDocument().ChildNeedsDistributionRecalc());
  DCHECK(IsElementNode() || IsTextNode());
  DCHECK(InActiveDocument());
  SetFlag(kNeedsReattachLayoutTree);
  MarkAncestorsWithChildNeedsReattachLayoutTree();
}

void Node::SetNeedsStyleRecalc(StyleChangeType change_type,
                               const StyleChangeReasonForTracing& reason) {
  DCHECK(!GetDocument().GetStyleEngine().InRebuildLayoutTree());
  DCHECK(change_type != kNoStyleChange);
  DCHECK(IsElementNode() || IsTextNode());

  if (!InActiveDocument())
    return;
  if (ShouldSkipMarkingStyleDirty())
    return;

  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "StyleRecalcInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
      inspector_style_recalc_invalidation_tracking_event::Data(
          this, change_type, reason));

  StyleChangeType existing_change_type = GetStyleChangeType();
  if (change_type > existing_change_type)
    SetStyleChange(change_type);

  auto* this_element = DynamicTo<Element>(this);
  if (existing_change_type == kNoStyleChange)
    MarkAncestorsWithChildNeedsStyleRecalc();

  if (this_element && HasRareData())
    this_element->SetAnimationStyleChange(false);

  if (auto* svg_element = DynamicTo<SVGElement>(this))
    svg_element->SetNeedsStyleRecalcForInstances(change_type, reason);
}

void Node::ClearNeedsStyleRecalc() {
  node_flags_ &= ~kStyleChangeMask;
  ClearFlag(kForceReattachLayoutTree);

  auto* element = DynamicTo<Element>(this);
  if (element && HasRareData())
    element->SetAnimationStyleChange(false);
}

bool Node::InActiveDocument() const {
  return isConnected() && GetDocument().IsActive();
}

bool Node::ShouldHaveFocusAppearance() const {
  DCHECK(IsFocused());
  return true;
}

bool Node::IsInert() const {
  if (!isConnected() || !CanParticipateInFlatTree())
    return true;

  DCHECK(!ChildNeedsDistributionRecalc());

  if (this != GetDocument() && this != GetDocument().documentElement()) {
    const Element* modal_element = GetDocument().ActiveModalDialog();
    if (!modal_element)
      modal_element = Fullscreen::FullscreenElementFrom(GetDocument());
    if (modal_element && !FlatTreeTraversal::ContainsIncludingPseudoElement(
                             *modal_element, *this)) {
      return true;
    }
  }

  if (RuntimeEnabledFeatures::InertAttributeEnabled()) {
    const auto* element = DynamicTo<Element>(this);
    if (!element)
      element = FlatTreeTraversal::ParentElement(*this);

    while (element) {
      if (element->FastHasAttribute(html_names::kInertAttr))
        return true;
      element = FlatTreeTraversal::ParentElement(*element);
    }
  }
  return GetDocument().GetFrame() && GetDocument().GetFrame()->IsInert();
}

unsigned Node::NodeIndex() const {
  const Node* temp_node = previousSibling();
  unsigned count = 0;
  for (count = 0; temp_node; count++)
    temp_node = temp_node->previousSibling();
  return count;
}

NodeListsNodeData* Node::NodeLists() {
  return HasRareData() ? RareData()->NodeLists() : nullptr;
}

void Node::ClearNodeLists() {
  RareData()->ClearNodeLists();
}

FlatTreeNodeData& Node::EnsureFlatTreeNodeData() {
  return EnsureRareData().EnsureFlatTreeNodeData();
}

FlatTreeNodeData* Node::GetFlatTreeNodeData() const {
  if (!HasRareData())
    return nullptr;
  return RareData()->GetFlatTreeNodeData();
}

void Node::ClearFlatTreeNodeData() {
  if (FlatTreeNodeData* data = GetFlatTreeNodeData())
    data->Clear();
}

void Node::ClearFlatTreeNodeDataIfHostChanged(const ContainerNode& parent) {
  if (FlatTreeNodeData* data = GetFlatTreeNodeData()) {
    if (data->AssignedSlot() &&
        data->AssignedSlot()->OwnerShadowHost() != &parent) {
      data->Clear();
    }
  }
}

bool Node::IsDescendantOf(const Node* other) const {
  DCHECK(this);  // Necessary for clusterfuzz tooling to get a useful backtrace

  // Return true if other is an ancestor of this, otherwise false
  if (!other || isConnected() != other->isConnected())
    return false;
  if (other->GetTreeScope() != GetTreeScope())
    return false;
  if (other->IsTreeScope())
    return !IsTreeScope();
  for (const ContainerNode* n = parentNode(); n; n = n->parentNode()) {
    if (n == other)
      return true;
  }
  return false;
}

bool Node::contains(const Node* node) const {
  if (!node)
    return false;
  return this == node || node->IsDescendantOf(this);
}

bool Node::IsShadowIncludingInclusiveAncestorOf(const Node& node) const {
  return this == &node || IsShadowIncludingAncestorOf(node);
}

bool Node::IsShadowIncludingAncestorOf(const Node& node) const {
  // In the following case, contains(host) below returns true.
  if (this == &node)
    return false;

  if (GetDocument() != node.GetDocument())
    return false;

  if (isConnected() != node.isConnected())
    return false;

  auto* this_node = DynamicTo<ContainerNode>(this);
  bool has_children = this_node ? this_node->HasChildren() : false;
  bool has_shadow = IsShadowHost(this);
  if (!has_children && !has_shadow)
    return false;

  for (const Node* host = &node; host; host = host->OwnerShadowHost()) {
    if (GetTreeScope() == host->GetTreeScope())
      return contains(host);
  }

  return false;
}

bool Node::ContainsIncludingHostElements(const Node& node) const {
  const Node* current = &node;
  do {
    if (current == this)
      return true;
    auto* curr_fragment = DynamicTo<DocumentFragment>(current);
    if (curr_fragment && curr_fragment->IsTemplateContent())
      current =
          static_cast<const TemplateContentDocumentFragment*>(current)->Host();
    else
      current = current->ParentOrShadowHostNode();
  } while (current);
  return false;
}

Node* Node::CommonAncestor(const Node& other,
                           ContainerNode* (*parent)(const Node&)) const {
  if (this == other)
    return const_cast<Node*>(this);
  if (GetDocument() != other.GetDocument())
    return nullptr;
  int this_depth = 0;
  for (const Node* node = this; node; node = parent(*node)) {
    if (node == &other)
      return const_cast<Node*>(node);
    this_depth++;
  }
  int other_depth = 0;
  for (const Node* node = &other; node; node = parent(*node)) {
    if (node == this)
      return const_cast<Node*>(this);
    other_depth++;
  }
  const Node* this_iterator = this;
  const Node* other_iterator = &other;
  if (this_depth > other_depth) {
    for (int i = this_depth; i > other_depth; --i)
      this_iterator = parent(*this_iterator);
  } else if (other_depth > this_depth) {
    for (int i = other_depth; i > this_depth; --i)
      other_iterator = parent(*other_iterator);
  }
  while (this_iterator) {
    if (this_iterator == other_iterator)
      return const_cast<Node*>(this_iterator);
    this_iterator = parent(*this_iterator);
    other_iterator = parent(*other_iterator);
  }
  DCHECK(!other_iterator);
  return nullptr;
}

void Node::ReattachLayoutTree(AttachContext& context) {
  context.performing_reattach = true;
  ReattachHookScope reattach_scope(*this);

  DetachLayoutTree(context.performing_reattach);
  AttachLayoutTree(context);
  DCHECK(!NeedsReattachLayoutTree());
}

void Node::AttachLayoutTree(AttachContext& context) {
  DCHECK(GetDocument().InStyleRecalc() || IsDocumentNode());
  DCHECK(!GetDocument().Lifecycle().InDetach());
  DCHECK(!context.performing_reattach ||
         GetDocument().GetStyleEngine().InRebuildLayoutTree());

  LayoutObject* layout_object = GetLayoutObject();
  DCHECK(!layout_object ||
         (layout_object->Style() &&
          (layout_object->Parent() || IsA<LayoutView>(layout_object))));

  ClearNeedsReattachLayoutTree();

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->UpdateCacheAfterNodeIsAttached(this);

  if (context.performing_reattach)
    ReattachHookScope::NotifyAttach(*this);
}

void Node::DetachLayoutTree(bool performing_reattach) {
  DCHECK(GetDocument().Lifecycle().StateAllowsDetach());
  DCHECK(!performing_reattach ||
         GetDocument().GetStyleEngine().InRebuildLayoutTree());
  DocumentLifecycle::DetachScope will_detach(GetDocument().Lifecycle());

  if (performing_reattach)
    ReattachHookScope::NotifyDetach(*this);

  if (GetLayoutObject())
    GetLayoutObject()->DestroyAndCleanupAnonymousWrappers();
  SetLayoutObject(nullptr);
  if (!performing_reattach) {
    // We are clearing the ComputedStyle for elements, which means we should not
    // need to recalc style. Also, this way we can detect if we need to remove
    // this Node as a StyleRecalcRoot if this detach is because the node is
    // removed from the flat tree. That is necessary because we are not allowed
    // to have a style recalc root outside the flat tree when traversing the
    // flat tree for style recalc (see StyleRecalcRoot::RemovedFromFlatTree()).
    ClearNeedsStyleRecalc();
    ClearChildNeedsStyleRecalc();
  }
}

const ComputedStyle* Node::VirtualEnsureComputedStyle(
    PseudoId pseudo_element_specifier) {
  return ParentOrShadowHostNode()
             ? ParentOrShadowHostNode()->EnsureComputedStyle(
                   pseudo_element_specifier)
             : nullptr;
}

void Node::SetForceReattachLayoutTree() {
  DCHECK(!GetDocument().GetStyleEngine().InRebuildLayoutTree());
  DCHECK(IsElementNode() || IsTextNode());
  if (GetForceReattachLayoutTree())
    return;
  if (!InActiveDocument())
    return;
  if (IsElementNode()) {
    if (!GetComputedStyle()) {
      DCHECK(!GetLayoutObject());
      return;
    }
  } else {
    DCHECK(IsTextNode());
    if (!GetLayoutObject() && ShouldSkipMarkingStyleDirty())
      return;
  }
  SetFlag(kForceReattachLayoutTree);
  if (!NeedsStyleRecalc()) {
    // Make sure we traverse down to this node during style recalc.
    MarkAncestorsWithChildNeedsStyleRecalc();
  }
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks
// questions about HTML in the core DOM class is obviously misplaced.
bool Node::CanStartSelection() const {
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*this))
    GetDocument().UpdateStyleAndLayoutTreeForNode(this);
  if (HasEditableStyle(*this))
    return true;

  if (GetLayoutObject()) {
    const ComputedStyle& style = GetLayoutObject()->StyleRef();
    if (style.UserSelect() == EUserSelect::kNone)
      return false;
    // We allow selections to begin within |user-select: text/all| sub trees
    // but not if the element is draggable.
    if (style.UserDrag() != EUserDrag::kElement &&
        (style.UserSelect() == EUserSelect::kText ||
         style.UserSelect() == EUserSelect::kAll))
      return true;
  }
  ContainerNode* parent = FlatTreeTraversal::Parent(*this);
  return parent ? parent->CanStartSelection() : true;
}

void Node::NotifyPriorityScrollAnchorStatusChanged() {
  auto* node = this;
  while (node && !node->GetLayoutObject())
    node = FlatTreeTraversal::Parent(*node);
  if (node) {
    DCHECK(node->GetLayoutObject());
    node->GetLayoutObject()->NotifyPriorityScrollAnchorStatusChanged();
  }
}

// StyledElements allow inline style (style="border: 1px"), presentational
// attributes (ex. color), class names (ex. class="foo bar") and other non-basic
// styling features. They also control if this element can participate in style
// sharing.
//
// FIXME: The only things that ever go through StyleResolver that aren't
// StyledElements are PseudoElements and VTTElements. It's possible we can just
// eliminate all the checks since those elements will never have class names,
// inline style, or other things that this apparently guards against.
bool Node::IsStyledElement() const {
  auto* this_element = DynamicTo<Element>(this);
  return IsHTMLElement() || IsSVGElement() || IsMathMLElement() ||
         (!RuntimeEnabledFeatures::MathMLCoreEnabled() && this_element &&
          this_element->namespaceURI() == mathml_names::kNamespaceURI);
}

bool Node::CanParticipateInFlatTree() const {
  // TODO(hayato): Return false for pseudo elements.
  return !IsShadowRoot() && !IsActiveV0InsertionPoint(*this);
}

bool Node::IsActiveSlotOrActiveV0InsertionPoint() const {
  return ToHTMLSlotElementIfSupportsAssignmentOrNull(*this) ||
         IsActiveV0InsertionPoint(*this);
}

AtomicString Node::SlotName() const {
  DCHECK(IsSlotable());
  if (IsElementNode()) {
    return HTMLSlotElement::NormalizeSlotName(
        To<Element>(*this).FastGetAttribute(html_names::kSlotAttr));
  }
  DCHECK(IsTextNode());
  return g_empty_atom;
}

bool Node::IsInV1ShadowTree() const {
  ShadowRoot* shadow_root = ContainingShadowRoot();
  return shadow_root && shadow_root->IsV1();
}

bool Node::IsInV0ShadowTree() const {
  ShadowRoot* shadow_root = ContainingShadowRoot();
  return shadow_root && !shadow_root->IsV1();
}

ShadowRoot* Node::ParentElementShadowRoot() const {
  Element* parent = parentElement();
  return parent ? parent->GetShadowRoot() : nullptr;
}

bool Node::IsChildOfV1ShadowHost() const {
  ShadowRoot* parent_shadow_root = ParentElementShadowRoot();
  return parent_shadow_root && parent_shadow_root->IsV1();
}

bool Node::IsChildOfV0ShadowHost() const {
  ShadowRoot* parent_shadow_root = ParentElementShadowRoot();
  return parent_shadow_root && !parent_shadow_root->IsV1();
}

ShadowRoot* Node::V1ShadowRootOfParent() const {
  if (Element* parent = parentElement())
    return parent->ShadowRootIfV1();
  return nullptr;
}

Element* Node::OwnerShadowHost() const {
  if (ShadowRoot* root = ContainingShadowRoot())
    return &root->host();
  return nullptr;
}

ShadowRoot* Node::ContainingShadowRoot() const {
  Node& root = GetTreeScope().RootNode();
  return DynamicTo<ShadowRoot>(root);
}

Node* Node::NonBoundaryShadowTreeRootNode() {
  DCHECK(!IsShadowRoot());
  Node* root = this;
  while (root) {
    if (root->IsShadowRoot())
      return root;
    Node* parent = root->ParentOrShadowHostNode();
    if (parent && parent->IsShadowRoot())
      return root;
    root = parent;
  }
  return nullptr;
}

ContainerNode* Node::NonShadowBoundaryParentNode() const {
  ContainerNode* parent = parentNode();
  return parent && !parent->IsShadowRoot() ? parent : nullptr;
}

Element* Node::ParentOrShadowHostElement() const {
  ContainerNode* parent = ParentOrShadowHostNode();
  if (!parent)
    return nullptr;

  if (auto* shadow_root = DynamicTo<ShadowRoot>(parent))
    return &shadow_root->host();

  return DynamicTo<Element>(parent);
}

ContainerNode* Node::ParentOrShadowHostOrTemplateHostNode() const {
  auto* this_fragment = DynamicTo<DocumentFragment>(this);
  if (this_fragment && this_fragment->IsTemplateContent())
    return static_cast<const TemplateContentDocumentFragment*>(this)->Host();
  return ParentOrShadowHostNode();
}

TreeScope& Node::OriginatingTreeScope() const {
  if (const SVGElement* svg_element = DynamicTo<SVGElement>(this)) {
    if (const SVGElement* corr_element = svg_element->CorrespondingElement()) {
      DCHECK(!corr_element->CorrespondingElement());
      return corr_element->GetTreeScope();
    }
  }
  return GetTreeScope();
}

Document* Node::ownerDocument() const {
  Document* doc = &GetDocument();
  return doc == this ? nullptr : doc;
}

const KURL& Node::baseURI() const {
  return GetDocument().BaseURL();
}

bool Node::isEqualNode(Node* other) const {
  if (!other)
    return false;

  NodeType node_type = getNodeType();
  if (node_type != other->getNodeType())
    return false;

  if (nodeValue() != other->nodeValue())
    return false;

  if (auto* this_attr = DynamicTo<Attr>(this)) {
    auto* other_attr = To<Attr>(other);
    if (this_attr->localName() != other_attr->localName())
      return false;

    if (this_attr->namespaceURI() != other_attr->namespaceURI())
      return false;
  } else if (auto* this_element = DynamicTo<Element>(this)) {
    auto* other_element = DynamicTo<Element>(other);
    if (this_element->TagQName() != other_element->TagQName())
      return false;

    if (!this_element->HasEquivalentAttributes(*other_element))
      return false;
  } else if (nodeName() != other->nodeName()) {
    return false;
  }

  Node* child = firstChild();
  Node* other_child = other->firstChild();

  while (child) {
    if (!child->isEqualNode(other_child))
      return false;

    child = child->nextSibling();
    other_child = other_child->nextSibling();
  }

  if (other_child)
    return false;

  if (const auto* document_type_this = DynamicTo<DocumentType>(this)) {
    const auto* document_type_other = To<DocumentType>(other);

    if (document_type_this->publicId() != document_type_other->publicId())
      return false;

    if (document_type_this->systemId() != document_type_other->systemId())
      return false;
  }

  return true;
}

bool Node::isDefaultNamespace(
    const AtomicString& namespace_uri_maybe_empty) const {
  // https://dom.spec.whatwg.org/#dom-node-isdefaultnamespace

  // 1. If namespace is the empty string, then set it to null.
  const AtomicString& namespace_uri = namespace_uri_maybe_empty.IsEmpty()
                                          ? g_null_atom
                                          : namespace_uri_maybe_empty;

  // 2. Let defaultNamespace be the result of running locate a namespace for
  // context object using null.
  const AtomicString& default_namespace = lookupNamespaceURI(String());

  // 3. Return true if defaultNamespace is the same as namespace, and false
  // otherwise.
  return namespace_uri == default_namespace;
}

const AtomicString& Node::lookupPrefix(
    const AtomicString& namespace_uri) const {
  // Implemented according to
  // https://dom.spec.whatwg.org/#dom-node-lookupprefix

  if (namespace_uri.IsEmpty() || namespace_uri.IsNull())
    return g_null_atom;

  const Element* context;

  switch (getNodeType()) {
    case kElementNode:
      context = To<Element>(this);
      break;
    case kDocumentNode:
      context = To<Document>(this)->documentElement();
      break;
    case kDocumentFragmentNode:
    case kDocumentTypeNode:
      context = nullptr;
      break;
    case kAttributeNode:
      context = To<Attr>(this)->ownerElement();
      break;
    default:
      context = parentElement();
      break;
  }

  if (!context)
    return g_null_atom;

  return context->LocateNamespacePrefix(namespace_uri);
}

const AtomicString& Node::lookupNamespaceURI(
    const String& specified_prefix) const {
  // Implemented according to
  // https://dom.spec.whatwg.org/#dom-node-lookupnamespaceuri

  // 1. If prefix is the empty string, then set it to null.
  String prefix = specified_prefix;
  if (!specified_prefix.IsNull() && specified_prefix.IsEmpty())
    prefix = String();

  // 2. Return the result of running locate a namespace for the context object
  // using prefix.

  // https://dom.spec.whatwg.org/#locate-a-namespace
  switch (getNodeType()) {
    case kElementNode: {
      const auto& element = To<Element>(*this);

      // 1. If its namespace is not null and its namespace prefix is prefix,
      // then return namespace.
      if (!element.namespaceURI().IsNull() && element.prefix() == prefix)
        return element.namespaceURI();

      // 2. If it has an attribute whose namespace is the XMLNS namespace,
      // namespace prefix is "xmlns", and local name is prefix, or if prefix is
      // null and it has an attribute whose namespace is the XMLNS namespace,
      // namespace prefix is null, and local name is "xmlns", then return its
      // value if it is not the empty string, and null otherwise.
      AttributeCollection attributes = element.Attributes();
      for (const Attribute& attr : attributes) {
        if (attr.Prefix() == g_xmlns_atom && attr.LocalName() == prefix) {
          if (!attr.Value().IsEmpty())
            return attr.Value();
          return g_null_atom;
        }
        if (attr.LocalName() == g_xmlns_atom && prefix.IsNull()) {
          if (!attr.Value().IsEmpty())
            return attr.Value();
          return g_null_atom;
        }
      }

      // 3. If its parent element is null, then return null.
      // 4. Return the result of running locate a namespace on its parent
      // element using prefix.
      if (Element* parent = parentElement())
        return parent->lookupNamespaceURI(prefix);
      return g_null_atom;
    }
    case kDocumentNode:
      if (Element* de = To<Document>(this)->documentElement())
        return de->lookupNamespaceURI(prefix);
      return g_null_atom;
    case kDocumentTypeNode:
    case kDocumentFragmentNode:
      return g_null_atom;
    case kAttributeNode: {
      const auto* attr = To<Attr>(this);
      if (attr->ownerElement())
        return attr->ownerElement()->lookupNamespaceURI(prefix);
      return g_null_atom;
    }
    default:
      if (Element* parent = parentElement())
        return parent->lookupNamespaceURI(prefix);
      return g_null_atom;
  }
}

String Node::textContent(bool convert_brs_to_newlines) const {
  // This covers ProcessingInstruction and Comment that should return their
  // value when .textContent is accessed on them, but should be ignored when
  // iterated over as a descendant of a ContainerNode.
  if (auto* character_data = DynamicTo<CharacterData>(this))
    return character_data->data();

  // Attribute nodes have their attribute values as textContent.
  if (auto* attr = DynamicTo<Attr>(this))
    return attr->value();

  // Documents and non-container nodes (that are not CharacterData)
  // have null textContent.
  if (IsDocumentNode() || !IsContainerNode())
    return String();

  StringBuilder content;
  for (const Node& node : NodeTraversal::InclusiveDescendantsOf(*this)) {
    if (IsA<HTMLBRElement>(node) && convert_brs_to_newlines) {
      content.Append('\n');
    } else if (auto* text_node = DynamicTo<Text>(node)) {
      content.Append(text_node->data());
    }
  }
  return content.ToString();
}

void Node::setTextContent(const StringOrTrustedScript& string_or_trusted_script,
                          ExceptionState& exception_state) {
  String value =
      string_or_trusted_script.IsString()
          ? string_or_trusted_script.GetAsString()
          : string_or_trusted_script.IsTrustedScript()
                ? string_or_trusted_script.GetAsTrustedScript()->toString()
                : g_empty_string;
  setTextContent(value);
}

void Node::textContent(StringOrTrustedScript& result) {
  String value = textContent();
  if (!value.IsNull())
    result.SetString(value);
}

void Node::setTextContent(const String& text) {
  switch (getNodeType()) {
    case kAttributeNode:
    case kTextNode:
    case kCdataSectionNode:
    case kCommentNode:
    case kProcessingInstructionNode:
      setNodeValue(text);
      return;
    case kElementNode:
    case kDocumentFragmentNode: {
      // FIXME: Merge this logic into replaceChildrenWithText.
      auto* container = To<ContainerNode>(this);

      // Note: This is an intentional optimization.
      // See crbug.com/352836 also.
      // No need to do anything if the text is identical.
      if (container->HasOneTextChild() &&
          To<Text>(container->firstChild())->data() == text && !text.IsEmpty())
        return;

      ChildListMutationScope mutation(*this);
      // Note: This API will not insert empty text nodes:
      // https://dom.spec.whatwg.org/#dom-node-textcontent
      if (text.IsEmpty()) {
        container->RemoveChildren(kDispatchSubtreeModifiedEvent);
      } else {
        container->RemoveChildren(kOmitSubtreeModifiedEvent);
        container->AppendChild(GetDocument().createTextNode(text),
                               ASSERT_NO_EXCEPTION);
      }
      return;
    }
    case kDocumentNode:
    case kDocumentTypeNode:
      // Do nothing.
      return;
  }
  NOTREACHED();
}

uint16_t Node::compareDocumentPosition(const Node* other_node,
                                       ShadowTreesTreatment treatment) const {
  if (other_node == this)
    return kDocumentPositionEquivalent;

  const auto* attr1 = DynamicTo<Attr>(this);
  const Attr* attr2 = DynamicTo<Attr>(other_node);

  const Node* start1 = attr1 ? attr1->ownerElement() : this;
  const Node* start2 = attr2 ? attr2->ownerElement() : other_node;

  // If either of start1 or start2 is null, then we are disconnected, since one
  // of the nodes is an orphaned attribute node.
  if (!start1 || !start2) {
    uint16_t direction = (this > other_node) ? kDocumentPositionPreceding
                                             : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  HeapVector<Member<const Node>, 16> chain1;
  HeapVector<Member<const Node>, 16> chain2;
  if (attr1)
    chain1.push_back(attr1);
  if (attr2)
    chain2.push_back(attr2);

  if (attr1 && attr2 && start1 == start2 && start1) {
    // We are comparing two attributes on the same node. Crawl our attribute map
    // and see which one we hit first.
    const Element* owner1 = attr1->ownerElement();
    AttributeCollection attributes = owner1->Attributes();
    for (const Attribute& attr : attributes) {
      // If neither of the two determining nodes is a child node and nodeType is
      // the same for both determining nodes, then an implementation-dependent
      // order between the determining nodes is returned. This order is stable
      // as long as no nodes of the same nodeType are inserted into or removed
      // from the direct container. This would be the case, for example, when
      // comparing two attributes of the same element, and inserting or removing
      // additional attributes might change the order between existing
      // attributes.
      if (attr1->GetQualifiedName() == attr.GetName())
        return kDocumentPositionImplementationSpecific |
               kDocumentPositionFollowing;
      if (attr2->GetQualifiedName() == attr.GetName())
        return kDocumentPositionImplementationSpecific |
               kDocumentPositionPreceding;
    }

    NOTREACHED();
    return kDocumentPositionDisconnected;
  }

  // If one node is in the document and the other is not, we must be
  // disconnected.  If the nodes have different owning documents, they must be
  // disconnected.  Note that we avoid comparing Attr nodes here, since they
  // return false from isConnected() all the time (which seems like a bug).
  if (start1->isConnected() != start2->isConnected() ||
      (treatment == kTreatShadowTreesAsDisconnected &&
       start1->GetTreeScope() != start2->GetTreeScope())) {
    uint16_t direction = (this > other_node) ? kDocumentPositionPreceding
                                             : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  // We need to find a common ancestor container, and then compare the indices
  // of the two immediate children.
  const Node* current;
  for (current = start1; current; current = current->ParentOrShadowHostNode())
    chain1.push_back(current);
  for (current = start2; current; current = current->ParentOrShadowHostNode())
    chain2.push_back(current);

  unsigned index1 = chain1.size();
  unsigned index2 = chain2.size();

  // If the two elements don't have a common root, they're not in the same tree.
  if (chain1[index1 - 1] != chain2[index2 - 1]) {
    uint16_t direction = (this > other_node) ? kDocumentPositionPreceding
                                             : kDocumentPositionFollowing;
    return kDocumentPositionDisconnected |
           kDocumentPositionImplementationSpecific | direction;
  }

  unsigned connection = start1->GetTreeScope() != start2->GetTreeScope()
                            ? kDocumentPositionDisconnected |
                                  kDocumentPositionImplementationSpecific
                            : 0;

  // Walk the two chains backwards and look for the first difference.
  for (unsigned i = std::min(index1, index2); i; --i) {
    const Node* child1 = chain1[--index1];
    const Node* child2 = chain2[--index2];
    if (child1 != child2) {
      // If one of the children is an attribute, it wins.
      if (child1->getNodeType() == kAttributeNode)
        return kDocumentPositionFollowing | connection;
      if (child2->getNodeType() == kAttributeNode)
        return kDocumentPositionPreceding | connection;

      // If one of the children is a shadow root,
      if (child1->IsShadowRoot() || child2->IsShadowRoot()) {
        if (!child2->IsShadowRoot())
          return Node::kDocumentPositionFollowing | connection;
        if (!child1->IsShadowRoot())
          return Node::kDocumentPositionPreceding | connection;

        return Node::kDocumentPositionPreceding | connection;
      }

      if (!child2->nextSibling())
        return kDocumentPositionFollowing | connection;
      if (!child1->nextSibling())
        return kDocumentPositionPreceding | connection;

      // Otherwise we need to see which node occurs first.  Crawl backwards from
      // child2 looking for child1.
      for (const Node* child = child2->previousSibling(); child;
           child = child->previousSibling()) {
        if (child == child1)
          return kDocumentPositionFollowing | connection;
      }
      return kDocumentPositionPreceding | connection;
    }
  }

  // There was no difference between the two parent chains, i.e., one was a
  // subset of the other.  The shorter chain is the ancestor.
  return index1 < index2 ? kDocumentPositionFollowing |
                               kDocumentPositionContainedBy | connection
                         : kDocumentPositionPreceding |
                               kDocumentPositionContains | connection;
}

String Node::DebugName() const {
  StringBuilder name;
  AppendUnsafe(name, DebugNodeName());
  if (const auto* this_element = DynamicTo<Element>(this)) {
    if (this_element->HasID()) {
      name.Append(" id=\'");
      AppendUnsafe(name, this_element->GetIdAttribute());
      name.Append('\'');
    }

    if (this_element->HasClass()) {
      name.Append(" class=\'");
      for (wtf_size_t i = 0; i < this_element->ClassNames().size(); ++i) {
        if (i > 0)
          name.Append(' ');
        AppendUnsafe(name, this_element->ClassNames()[i]);
      }
      name.Append('\'');
    }
  }
  return name.ToString();
}

String Node::DebugNodeName() const {
  return nodeName();
}

static void DumpAttributeDesc(const Node& node,
                              const QualifiedName& name,
                              StringBuilder& builder) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return;
  const AtomicString& value = element->getAttribute(name);
  if (value.IsEmpty())
    return;
  builder.Append(' ');
  builder.Append(name.ToString());
  builder.Append("=");
  builder.Append(String(value).EncodeForDebugging());
}

std::ostream& operator<<(std::ostream& ostream, const Node& node) {
  return ostream << node.ToString().Utf8();
}

std::ostream& operator<<(std::ostream& ostream, const Node* node) {
  if (!node)
    return ostream << "null";
  return ostream << *node;
}

String Node::ToString() const {
  if (getNodeType() == Node::kProcessingInstructionNode)
    return "?" + nodeName();
  if (auto* shadow_root = DynamicTo<ShadowRoot>(this)) {
    // nodeName of ShadowRoot is #document-fragment.  It's confused with
    // DocumentFragment.
    std::stringstream shadow_root_type;
    shadow_root_type << shadow_root->GetType();
    String shadow_root_type_str(shadow_root_type.str().c_str());
    return "#shadow-root(" + shadow_root_type_str + ")";
  }
  if (IsDocumentTypeNode())
    return "DOCTYPE " + nodeName();

  StringBuilder builder;
  builder.Append(nodeName());
  if (IsTextNode()) {
    builder.Append(" ");
    builder.Append(nodeValue().EncodeForDebugging());
    return builder.ToString();
  }
  DumpAttributeDesc(*this, html_names::kIdAttr, builder);
  DumpAttributeDesc(*this, html_names::kClassAttr, builder);
  DumpAttributeDesc(*this, html_names::kStyleAttr, builder);
  if (HasEditableStyle(*this))
    builder.Append(" (editable)");
  if (GetDocument().FocusedElement() == this)
    builder.Append(" (focused)");
  return builder.ToString();
}

#if DCHECK_IS_ON()

String Node::ToTreeStringForThis() const {
  return ToMarkedTreeString(this, "*");
}

String Node::ToFlatTreeStringForThis() const {
  return ToMarkedFlatTreeString(this, "*");
}

void Node::PrintNodePathTo(std::ostream& stream) const {
  HeapVector<Member<const Node>, 16> chain;
  const Node* parent_node = this;
  while (parent_node->ParentOrShadowHostNode()) {
    chain.push_back(parent_node);
    parent_node = parent_node->ParentOrShadowHostNode();
  }
  for (unsigned index = chain.size(); index > 0; --index) {
    const Node* node = chain[index - 1];
    if (node->IsShadowRoot()) {
      stream << "/#shadow-root";
      continue;
    }

    switch (node->getNodeType()) {
      case kElementNode: {
        stream << "/" << node->nodeName().Utf8();

        const auto* element = To<Element>(node);
        const AtomicString& idattr = element->GetIdAttribute();
        bool has_id_attr = !idattr.IsNull() && !idattr.IsEmpty();
        if (node->previousSibling() || node->nextSibling()) {
          int count = 0;
          for (const Node* previous = node->previousSibling(); previous;
               previous = previous->previousSibling()) {
            if (previous->nodeName() == node->nodeName()) {
              ++count;
            }
          }
          if (has_id_attr)
            stream << "[@id=\"" << idattr.Utf8()
                   << "\" and position()=" << count << "]";
          else
            stream << "[" << count << "]";
        } else if (has_id_attr) {
          stream << "[@id=\"" << idattr.Utf8() << "\"]";
        }
        break;
      }
      case kTextNode:
        stream << "/text()";
        break;
      case kAttributeNode:
        stream << "/@" << node->nodeName().Utf8();
        break;
      default:
        break;
    }
  }
}

static void AppendMarkedTree(const String& base_indent,
                             const Node* root_node,
                             const Node* marked_node1,
                             const char* marked_label1,
                             const Node* marked_node2,
                             const char* marked_label2,
                             StringBuilder& builder) {
  for (const Node& node : NodeTraversal::InclusiveDescendantsOf(*root_node)) {
    StringBuilder indent;
    if (node == marked_node1)
      indent.Append(marked_label1);
    if (node == marked_node2)
      indent.Append(marked_label2);
    indent.Append(base_indent);
    for (const Node* tmp_node = &node; tmp_node && tmp_node != root_node;
         tmp_node = tmp_node->ParentOrShadowHostNode())
      indent.Append('\t');
    builder.Append(indent);
    builder.Append(node.ToString());
    builder.Append("\n");
    indent.Append('\t');

    if (const auto* element = DynamicTo<Element>(node)) {
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdMarker)) {
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      }
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdBefore))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdAfter))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdFirstLetter))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdBackdrop))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
    }

    if (ShadowRoot* shadow_root = node.GetShadowRoot()) {
      AppendMarkedTree(indent.ToString(), shadow_root, marked_node1,
                       marked_label1, marked_node2, marked_label2, builder);
    }
  }
}

static void AppendMarkedFlatTree(const String& base_indent,
                                 const Node* root_node,
                                 const Node* marked_node1,
                                 const char* marked_label1,
                                 const Node* marked_node2,
                                 const char* marked_label2,
                                 StringBuilder& builder) {
  for (const Node* node = root_node; node;
       node = FlatTreeTraversal::NextSibling(*node)) {
    StringBuilder indent;
    if (node == marked_node1)
      indent.Append(marked_label1);
    if (node == marked_node2)
      indent.Append(marked_label2);
    indent.Append(base_indent);
    builder.Append(indent);
    builder.Append(node->ToString());
    builder.Append("\n");
    indent.Append('\t');

    if (Node* child = FlatTreeTraversal::FirstChild(*node))
      AppendMarkedFlatTree(indent.ToString(), child, marked_node1,
                           marked_label1, marked_node2, marked_label2, builder);
  }
}

String Node::ToMarkedTreeString(const Node* marked_node1,
                                const char* marked_label1,
                                const Node* marked_node2,
                                const char* marked_label2) const {
  const Node* root_node;
  const Node* node = this;
  while (node->ParentOrShadowHostNode() && !IsA<HTMLBodyElement>(*node))
    node = node->ParentOrShadowHostNode();
  root_node = node;

  StringBuilder builder;
  String starting_indent;
  AppendMarkedTree(starting_indent, root_node, marked_node1, marked_label1,
                   marked_node2, marked_label2, builder);
  return builder.ToString();
}

String Node::ToMarkedFlatTreeString(const Node* marked_node1,
                                    const char* marked_label1,
                                    const Node* marked_node2,
                                    const char* marked_label2) const {
  const Node* root_node;
  const Node* node = this;
  while (node->ParentOrShadowHostNode() && !IsA<HTMLBodyElement>(*node))
    node = node->ParentOrShadowHostNode();
  root_node = node;

  StringBuilder builder;
  String starting_indent;
  AppendMarkedFlatTree(starting_indent, root_node, marked_node1, marked_label1,
                       marked_node2, marked_label2, builder);
  return builder.ToString();
}

static ContainerNode* ParentOrShadowHostOrFrameOwner(const Node* node) {
  ContainerNode* parent = node->ParentOrShadowHostNode();
  if (!parent && node->GetDocument().GetFrame())
    parent = node->GetDocument().GetFrame()->DeprecatedLocalOwner();
  return parent;
}

static void PrintSubTreeAcrossFrame(const Node* node,
                                    const Node* marked_node,
                                    const String& indent,
                                    std::ostream& stream) {
  if (node == marked_node)
    stream << "*";
  stream << indent.Utf8() << *node << "\n";
  if (auto* frame_owner_element = DynamicTo<HTMLFrameOwnerElement>(node)) {
    PrintSubTreeAcrossFrame(frame_owner_element->contentDocument(), marked_node,
                            indent + "\t", stream);
  }
  if (ShadowRoot* shadow_root = node->GetShadowRoot())
    PrintSubTreeAcrossFrame(shadow_root, marked_node, indent + "\t", stream);
  for (const Node* child = node->firstChild(); child;
       child = child->nextSibling())
    PrintSubTreeAcrossFrame(child, marked_node, indent + "\t", stream);
}

void Node::ShowTreeForThisAcrossFrame() const {
  const Node* root_node = this;
  while (ParentOrShadowHostOrFrameOwner(root_node))
    root_node = ParentOrShadowHostOrFrameOwner(root_node);
  std::stringstream stream;
  PrintSubTreeAcrossFrame(root_node, this, "", stream);
  LOG(INFO) << "\n" << stream.str();
}

#endif

// --------

Element* Node::EnclosingLinkEventParentOrSelf() const {
  // https://crbug.com/784492
  DCHECK(this);

  for (const Node* node = this; node; node = FlatTreeTraversal::Parent(*node)) {
    // For imagemaps, the enclosing link node is the associated area element not
    // the image itself.  So we don't let images be the enclosingLinkNode, even
    // though isLink sometimes returns true for them.
    if (node->IsLink() && !IsA<HTMLImageElement>(*node)) {
      // Casting to Element is safe because only HTMLAnchorElement,
      // HTMLImageElement and SVGAElement can return true for isLink().
      return To<Element>(const_cast<Node*>(node));
    }
  }

  return nullptr;
}

const AtomicString& Node::InterfaceName() const {
  return event_target_names::kNode;
}

ExecutionContext* Node::GetExecutionContext() const {
  return GetDocument().GetExecutionContext();
}

void Node::WillMoveToNewDocument(Document& old_document,
                                 Document& new_document) {
  if (!old_document.GetPage() ||
      old_document.GetPage() == new_document.GetPage())
    return;

  old_document.GetFrame()->GetEventHandlerRegistry().DidMoveOutOfPage(*this);

  if (auto* this_element = DynamicTo<Element>(this)) {
    StylePropertyMapReadOnly* computed_style_map_item =
        old_document.RemoveComputedStyleMapItem(this_element);
    if (computed_style_map_item) {
      new_document.AddComputedStyleMapItem(this_element,
                                           computed_style_map_item);
    }
  }
}

void Node::DidMoveToNewDocument(Document& old_document) {
  TreeScopeAdopter::EnsureDidMoveToNewDocumentWasCalled(old_document);

  if (const EventTargetData* event_target_data = GetEventTargetData()) {
    const EventListenerMap& listener_map =
        event_target_data->event_listener_map;
    if (!listener_map.IsEmpty()) {
      for (const auto& type : listener_map.EventTypes())
        GetDocument().AddListenerTypeIfNeeded(type, *this);
    }
  }
  if (auto* text_node = DynamicTo<Text>(this))
    old_document.Markers().RemoveMarkersForNode(*text_node);
  if (GetDocument().GetPage() &&
      GetDocument().GetPage() != old_document.GetPage()) {
    GetDocument().GetFrame()->GetEventHandlerRegistry().DidMoveIntoPage(*this);
  }

  if (const HeapVector<Member<MutationObserverRegistration>>* registry =
          MutationObserverRegistry()) {
    for (const auto& registration : *registry) {
      GetDocument().AddMutationObserverTypes(registration->MutationTypes());
    }
  }

  if (TransientMutationObserverRegistry()) {
    for (MutationObserverRegistration* registration :
         *TransientMutationObserverRegistry())
      GetDocument().AddMutationObserverTypes(registration->MutationTypes());
  }
}

void Node::AddedEventListener(const AtomicString& event_type,
                              RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  GetDocument().AddListenerTypeIfNeeded(event_type, *this);
  if (auto* frame = GetDocument().GetFrame()) {
    frame->GetEventHandlerRegistry().DidAddEventHandler(
        *this, event_type, registered_listener.Options());
    // We need to track the existence of the visibilitychange event listeners to
    // enable/disable sudden terminations.
    if (IsDocumentNode() && event_type == event_type_names::kVisibilitychange) {
      frame->AddedSuddenTerminationDisablerListener(*this, event_type);
    }
  }
}

void Node::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  // FIXME: Notify Document that the listener has vanished. We need to keep
  // track of a number of listeners for each type, not just a bool - see
  // https://bugs.webkit.org/show_bug.cgi?id=33861
  if (auto* frame = GetDocument().GetFrame()) {
    frame->GetEventHandlerRegistry().DidRemoveEventHandler(
        *this, event_type, registered_listener.Options());
    // We need to track the existence of the visibilitychange event listeners to
    // enable/disable sudden terminations.
    if (IsDocumentNode() && event_type == event_type_names::kVisibilitychange) {
      frame->RemovedSuddenTerminationDisablerListener(*this, event_type);
    }
  }
}

void Node::RemoveAllEventListeners() {
  if (HasEventListeners() && GetDocument().GetPage())
    GetDocument()
        .GetFrame()
        ->GetEventHandlerRegistry()
        .DidRemoveAllEventHandlers(*this);
  EventTarget::RemoveAllEventListeners();
}

void Node::RemoveAllEventListenersRecursively() {
  ScriptForbiddenScope forbid_script_during_raw_iteration;
  for (Node& node : NodeTraversal::StartsAt(*this)) {
    node.RemoveAllEventListeners();
    if (ShadowRoot* root = node.GetShadowRoot())
      root->RemoveAllEventListenersRecursively();
  }
}

using EventTargetDataMap =
    HeapHashMap<WeakMember<Node>, Member<EventTargetData>>;
static EventTargetDataMap& GetEventTargetDataMap() {
  DEFINE_STATIC_LOCAL(Persistent<EventTargetDataMap>, map,
                      (MakeGarbageCollected<EventTargetDataMap>()));
  return *map;
}

EventTargetData* Node::GetEventTargetData() {
  return HasEventTargetData() ? GetEventTargetDataMap().at(this) : nullptr;
}

EventTargetData& Node::EnsureEventTargetData() {
  if (HasEventTargetData())
    return *GetEventTargetDataMap().at(this);
  DCHECK(!GetEventTargetDataMap().Contains(this));
  EventTargetData* data = MakeGarbageCollected<EventTargetData>();
  GetEventTargetDataMap().Set(this, data);
  SetHasEventTargetData(true);
  return *data;
}

const HeapVector<Member<MutationObserverRegistration>>*
Node::MutationObserverRegistry() {
  if (!HasRareData())
    return nullptr;
  NodeMutationObserverData* data = RareData()->MutationObserverData();
  if (!data)
    return nullptr;
  return &data->Registry();
}

const HeapHashSet<Member<MutationObserverRegistration>>*
Node::TransientMutationObserverRegistry() {
  if (!HasRareData())
    return nullptr;
  NodeMutationObserverData* data = RareData()->MutationObserverData();
  if (!data)
    return nullptr;
  return &data->TransientRegistry();
}

template <typename Registry>
static inline void CollectMatchingObserversForMutation(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    Registry* registry,
    Node& target,
    MutationType type,
    const QualifiedName* attribute_name) {
  if (!registry)
    return;

  for (const auto& registration : *registry) {
    if (registration->ShouldReceiveMutationFrom(target, type, attribute_name)) {
      MutationRecordDeliveryOptions delivery_options =
          registration->DeliveryOptions();
      HeapHashMap<Member<MutationObserver>,
                  MutationRecordDeliveryOptions>::AddResult result =
          observers.insert(&registration->Observer(), delivery_options);
      if (!result.is_new_entry)
        result.stored_value->value |= delivery_options;
    }
  }
}

void Node::GetRegisteredMutationObserversOfType(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    MutationType type,
    const QualifiedName* attribute_name) {
  DCHECK((type == kMutationTypeAttributes && attribute_name) ||
         !attribute_name);
  CollectMatchingObserversForMutation(observers, MutationObserverRegistry(),
                                      *this, type, attribute_name);
  CollectMatchingObserversForMutation(observers,
                                      TransientMutationObserverRegistry(),
                                      *this, type, attribute_name);
  ScriptForbiddenScope forbid_script_during_raw_iteration;
  for (Node* node = parentNode(); node; node = node->parentNode()) {
    CollectMatchingObserversForMutation(observers,
                                        node->MutationObserverRegistry(), *this,
                                        type, attribute_name);
    CollectMatchingObserversForMutation(
        observers, node->TransientMutationObserverRegistry(), *this, type,
        attribute_name);
  }
}

void Node::RegisterMutationObserver(
    MutationObserver& observer,
    MutationObserverOptions options,
    const HashSet<AtomicString>& attribute_filter) {
  MutationObserverRegistration* registration = nullptr;
  for (const auto& item :
       EnsureRareData().EnsureMutationObserverData().Registry()) {
    if (&item->Observer() == &observer) {
      registration = item.Get();
      registration->ResetObservation(options, attribute_filter);
    }
  }

  if (!registration) {
    registration = MakeGarbageCollected<MutationObserverRegistration>(
        observer, this, options, attribute_filter);
    EnsureRareData().EnsureMutationObserverData().AddRegistration(registration);
  }

  GetDocument().AddMutationObserverTypes(registration->MutationTypes());
}

void Node::UnregisterMutationObserver(
    MutationObserverRegistration* registration) {
  const HeapVector<Member<MutationObserverRegistration>>* registry =
      MutationObserverRegistry();
  DCHECK(registry);
  if (!registry)
    return;

  // FIXME: Simplify the registration/transient registration logic to make this
  // understandable by humans.  The explicit dispose() is needed to have the
  // registration object unregister itself promptly.
  registration->Dispose();
  EnsureRareData().EnsureMutationObserverData().RemoveRegistration(
      registration);
}

void Node::RegisterTransientMutationObserver(
    MutationObserverRegistration* registration) {
  EnsureRareData().EnsureMutationObserverData().AddTransientRegistration(
      registration);
}

void Node::UnregisterTransientMutationObserver(
    MutationObserverRegistration* registration) {
  const HeapHashSet<Member<MutationObserverRegistration>>* transient_registry =
      TransientMutationObserverRegistry();
  DCHECK(transient_registry);
  if (!transient_registry)
    return;

  EnsureRareData().EnsureMutationObserverData().RemoveTransientRegistration(
      registration);
}

void Node::NotifyMutationObserversNodeWillDetach() {
  if (!GetDocument().HasMutationObservers())
    return;

  ScriptForbiddenScope forbid_script_during_raw_iteration;
  for (Node* node = parentNode(); node; node = node->parentNode()) {
    if (const HeapVector<Member<MutationObserverRegistration>>* registry =
            node->MutationObserverRegistry()) {
      for (const auto& registration : *registry)
        registration->ObservedSubtreeNodeWillDetach(*this);
    }

    if (const HeapHashSet<Member<MutationObserverRegistration>>*
            transient_registry = node->TransientMutationObserverRegistry()) {
      for (auto& registration : *transient_registry)
        registration->ObservedSubtreeNodeWillDetach(*this);
    }
  }
}

void Node::HandleLocalEvents(Event& event) {
  if (!HasEventTargetData())
    return;

  if (IsDisabledFormControl(this) && IsA<MouseEvent>(event) &&
      !RuntimeEnabledFeatures::SendMouseEventsDisabledFormControlsEnabled()) {
    if (HasEventListeners(event.type())) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDispatchMouseEventOnDisabledFormControl);
      if (event.type() == event_type_names::kMousedown ||
          event.type() == event_type_names::kMouseup) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::kDispatchMouseUpDownEventOnDisabledFormControl);
      }
    }
    return;
  }

  FireEventListeners(event);
}

void Node::DispatchScopedEvent(Event& event) {
  event.SetTrusted(true);
  EventDispatcher::DispatchScopedEvent(*this, event);
}

DispatchEventResult Node::DispatchEventInternal(Event& event) {
  return EventDispatcher::DispatchEvent(*this, event);
}

void Node::DispatchSubtreeModifiedEvent() {
  if (IsInShadowTree())
    return;

#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  if (!GetDocument().HasListenerType(Document::kDOMSubtreeModifiedListener))
    return;

  DispatchScopedEvent(*MutationEvent::Create(
      event_type_names::kDOMSubtreeModified, Event::Bubbles::kYes));
}

DispatchEventResult Node::DispatchDOMActivateEvent(int detail,
                                                   Event& underlying_event) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  UIEvent& event = *UIEvent::Create();
  event.initUIEvent(event_type_names::kDOMActivate, true, true,
                    GetDocument().domWindow(), detail);
  event.SetUnderlyingEvent(&underlying_event);
  event.SetComposed(underlying_event.composed());
  if (!isConnected())
    event.SetCopyEventPathFromUnderlyingEvent();
  DispatchScopedEvent(event);

  // TODO(dtapuska): Dispatching scoped events shouldn't check the return
  // type because the scoped event could get put off in the delayed queue.
  return EventTarget::GetDispatchEventResult(event);
}

void Node::DispatchSimulatedClick(const Event* underlying_event,
                                  SimulatedClickMouseEventOptions event_options,
                                  SimulatedClickCreationScope scope) {
  if (auto* element = IsElementNode() ? To<Element>(this) : parentElement()) {
    element->ActivateDisplayLockIfNeeded(
        DisplayLockActivationReason::kSimulatedClick);
  }
  EventDispatcher::DispatchSimulatedClick(*this, underlying_event,
                                          event_options, scope);
}

void Node::DefaultEventHandler(Event& event) {
  if (event.target() != this)
    return;
  const AtomicString& event_type = event.type();
  if (event_type == event_type_names::kKeydown ||
      event_type == event_type_names::kKeypress ||
      event_type == event_type_names::kKeyup) {
    if (auto* keyboard_event = DynamicTo<KeyboardEvent>(&event)) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        frame->GetEventHandler().DefaultKeyboardEventHandler(keyboard_event);
      }
    }
  } else if (event_type == event_type_names::kClick) {
    auto* ui_event = DynamicTo<UIEvent>(event);
    int detail = ui_event ? ui_event->detail() : 0;
    if (DispatchDOMActivateEvent(detail, event) !=
        DispatchEventResult::kNotCanceled)
      event.SetDefaultHandled();
  } else if (event_type == event_type_names::kContextmenu &&
             IsA<MouseEvent>(event)) {
    if (Page* page = GetDocument().GetPage()) {
      page->GetContextMenuController().HandleContextMenuEvent(
          To<MouseEvent>(&event));
    }
  } else if (event_type == event_type_names::kTextInput) {
    if (event.HasInterface(event_interface_names::kTextEvent)) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        frame->GetEventHandler().DefaultTextInputEventHandler(
            To<TextEvent>(&event));
      }
    }
  } else if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled() &&
             event_type == event_type_names::kMousedown &&
             IsA<MouseEvent>(event)) {
    auto& mouse_event = To<MouseEvent>(event);
    if (mouse_event.button() ==
        static_cast<int16_t>(WebPointerProperties::Button::kMiddle)) {
      if (EnclosingLinkEventParentOrSelf())
        return;

      // Avoid that canBeScrolledAndHasScrollableArea changes layout tree
      // structure.
      // FIXME: We should avoid synchronous layout if possible. We can
      // remove this synchronous layout if we avoid synchronous layout in
      // LayoutTextControlSingleLine::scrollHeight
      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);
      LayoutObject* layout_object = GetLayoutObject();
      while (
          layout_object &&
          (!layout_object->IsBox() ||
           !ToLayoutBox(layout_object)->CanBeScrolledAndHasScrollableArea())) {
        if (auto* document = DynamicTo<Document>(layout_object->GetNode())) {
          Element* owner = document->LocalOwner();
          layout_object = owner ? owner->GetLayoutObject() : nullptr;
        } else {
          layout_object = layout_object->Parent();
        }
      }
      if (layout_object) {
        if (LocalFrame* frame = GetDocument().GetFrame())
          frame->GetEventHandler().StartMiddleClickAutoscroll(layout_object);
      }
    }
  }
}

void Node::UpdateHadKeyboardEvent(const Event& event) {
  if (GetDocument().HadKeyboardEvent())
    return;

  GetDocument().SetHadKeyboardEvent(true);

  // Changes to HadKeyboardEvent may affect :focus-visible matching,
  // ShouldHaveFocusAppearance and LayoutTheme::IsFocused().
  // Inform LayoutTheme if HadKeyboardEvent changes.
  if (GetLayoutObject()) {
    GetLayoutObject()->InvalidateIfControlStateChanged(kFocusControlState);

    auto* this_node = DynamicTo<ContainerNode>(this);
    if (RuntimeEnabledFeatures::CSSFocusVisibleEnabled() && this_node)
      this_node->FocusVisibleStateChanged();
  }
}

bool Node::HasActivationBehavior() const {
  return false;
}

bool Node::WillRespondToMouseMoveEvents() {
  if (IsDisabledFormControl(this))
    return false;
  return HasEventListeners(event_type_names::kMousemove) ||
         HasEventListeners(event_type_names::kMouseover) ||
         HasEventListeners(event_type_names::kMouseout);
}

bool Node::WillRespondToMouseClickEvents() {
  if (IsDisabledFormControl(this))
    return false;
  GetDocument().UpdateStyleAndLayoutTree();
  return HasEditableStyle(*this) ||
         HasEventListeners(event_type_names::kMouseup) ||
         HasEventListeners(event_type_names::kMousedown) ||
         HasEventListeners(event_type_names::kClick) ||
         HasEventListeners(event_type_names::kDOMActivate);
}

unsigned Node::ConnectedSubframeCount() const {
  return HasRareData() ? RareData()->ConnectedSubframeCount() : 0;
}

void Node::IncrementConnectedSubframeCount() {
  DCHECK(IsContainerNode());
  EnsureRareData().IncrementConnectedSubframeCount();
}

void Node::DecrementConnectedSubframeCount() {
  RareData()->DecrementConnectedSubframeCount();
}

StaticNodeList* Node::getDestinationInsertionPoints() {
  UpdateDistributionForLegacyDistributedNodes();
  HeapVector<Member<V0InsertionPoint>, 8> insertion_points;
  CollectDestinationInsertionPoints(*this, insertion_points);
  HeapVector<Member<Node>> filtered_insertion_points;
  for (const auto& insertion_point : insertion_points) {
    DCHECK(insertion_point->ContainingShadowRoot());
    if (!insertion_point->ContainingShadowRoot()->IsOpenOrV0())
      break;
    filtered_insertion_points.push_back(insertion_point);
  }
  return StaticNodeList::Adopt(filtered_insertion_points);
}

HTMLSlotElement* Node::AssignedSlot() const {
  DCHECK(!IsPseudoElement());
  ShadowRoot* root = V1ShadowRootOfParent();
  if (!root)
    return nullptr;

  if (!root->HasSlotAssignment())
    return nullptr;

  // TODO(hayato): Node::AssignedSlot() shouldn't be called while
  // in executing RecalcAssignment(), however, unfortunately,
  // that could happen as follows:
  //
  // 1. RecalcAssignment() can detach a node
  // 2. Then, DetachLayoutTree() may use FlatTreeTraversal via the hook of
  // AXObjectCacheImpl::ChildrenChanged().
  //
  // Note that using FlatTreeTraversal in detaching layout tree should be banned
  // in the long term.
  //
  // If we can remove such code path, we don't need to check
  // IsInSlotAssignmentRecalc() here.
  if (GetDocument().IsInSlotAssignmentRecalc()) {
    // FlatTreeNodeData is not realiable here. Entering slow path.
    return root->AssignedSlotFor(*this);
  }

  // Recalc assignment, if necessary, to make sure the FlatTreeNodeData is not
  // dirty. RecalcAssignment() is almost no-op if we don't need to recalc.
  root->GetSlotAssignment().RecalcAssignment();
  if (FlatTreeNodeData* data = GetFlatTreeNodeData()) {
    DCHECK_EQ(root->AssignedSlotFor(*this), data->AssignedSlot());
    return data->AssignedSlot();
  }
  return nullptr;
}

HTMLSlotElement* Node::assignedSlotForBinding() {
  // assignedSlot doesn't need to recalc slot assignment
  if (ShadowRoot* root = V1ShadowRootOfParent()) {
    if (root->GetType() == ShadowRootType::kOpen)
      return AssignedSlot();
  }
  return nullptr;
}

void Node::SetFocused(bool flag, mojom::blink::FocusType focus_type) {
  if (focus_type == mojom::blink::FocusType::kMouse)
    GetDocument().SetHadKeyboardEvent(false);
  GetDocument().UserActionElements().SetFocused(this, flag);
}

void Node::SetHasFocusWithin(bool flag) {
  GetDocument().UserActionElements().SetHasFocusWithin(this, flag);
}

void Node::SetDragged(bool flag) {
  GetDocument().UserActionElements().SetDragged(this, flag);
}

bool Node::IsUserActionElementActive() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().IsActive(this);
}

bool Node::IsUserActionElementInActiveChain() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().IsInActiveChain(this);
}

bool Node::IsUserActionElementDragged() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().IsDragged(this);
}

bool Node::IsUserActionElementHovered() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().IsHovered(this);
}

bool Node::IsUserActionElementFocused() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().IsFocused(this);
}

bool Node::IsUserActionElementHasFocusWithin() const {
  DCHECK(IsUserActionElement());
  return GetDocument().UserActionElements().HasFocusWithin(this);
}

void Node::SetCustomElementState(CustomElementState new_state) {
  CustomElementState old_state = GetCustomElementState();

  switch (new_state) {
    case CustomElementState::kUncustomized:
      NOTREACHED();  // Everything starts in this state
      return;

    case CustomElementState::kUndefined:
      DCHECK_EQ(CustomElementState::kUncustomized, old_state);
      break;

    case CustomElementState::kCustom:
      DCHECK(old_state == CustomElementState::kUndefined ||
             old_state == CustomElementState::kFailed);
      break;

    case CustomElementState::kFailed:
      DCHECK_NE(CustomElementState::kFailed, old_state);
      break;
  }

  DCHECK(IsHTMLElement());
  DCHECK_NE(kV0Upgraded, GetV0CustomElementState());

  auto* element = To<Element>(this);
  bool was_defined = element->IsDefined();

  node_flags_ = (node_flags_ & ~kCustomElementStateMask) |
                static_cast<NodeFlags>(new_state);
  DCHECK(new_state == GetCustomElementState());

  if (element->IsDefined() != was_defined) {
    element->PseudoStateChanged(CSSSelector::kPseudoDefined);
    if (RuntimeEnabledFeatures::CustomElementsV0Enabled(GetExecutionContext()))
      element->PseudoStateChanged(CSSSelector::kPseudoUnresolved);
  }
}

void Node::SetV0CustomElementState(V0CustomElementState new_state) {
  DCHECK(
      RuntimeEnabledFeatures::CustomElementsV0Enabled(GetExecutionContext()));
  V0CustomElementState old_state = GetV0CustomElementState();

  switch (new_state) {
    case kV0NotCustomElement:
      NOTREACHED();  // Everything starts in this state
      return;

    case kV0WaitingForUpgrade:
      DCHECK_EQ(kV0NotCustomElement, old_state);
      break;

    case kV0Upgraded:
      DCHECK_EQ(kV0WaitingForUpgrade, old_state);
      break;
  }

  DCHECK(IsHTMLElement() || IsSVGElement());
  DCHECK(CustomElementState::kCustom != GetCustomElementState());
  SetFlag(kV0CustomElementFlag);
  SetFlag(new_state == kV0Upgraded, kV0CustomElementUpgradedFlag);

  if (old_state == kV0NotCustomElement || new_state == kV0Upgraded) {
    To<Element>(this)->PseudoStateChanged(CSSSelector::kPseudoUnresolved);
    To<Element>(this)->PseudoStateChanged(CSSSelector::kPseudoDefined);
  }
}

void Node::CheckSlotChange(SlotChangeType slot_change_type) {
  // Common check logic is used in both cases, "after inserted" and "before
  // removed".

  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-insert
  // https://dom.spec.whatwg.org/#concept-node-remove

  // This function is usually called while DOM Mutation is still in-progress.
  // For "after inserted" case, we assume that a parent and a child have been
  // already connected. For "before removed" case, we assume that a parent and a
  // child have not been disconnected yet.

  if (!IsSlotable())
    return;

  if (ShadowRoot* root = V1ShadowRootOfParent()) {
    // A shadow host's child can be assigned to a slot in the host's shadow
    // tree.

    // Although DOM Standard requires "assign a slot for node / run assign
    // slotables" at this timing, we skip it as an optimization.
    if (HTMLSlotElement* slot = root->AssignedSlotFor(*this))
      slot->DidSlotChange(slot_change_type);
  } else if (IsInV1ShadowTree()) {
    // Checking for fallback content if the node is in a v1 shadow tree.
    if (auto* parent_slot = DynamicTo<HTMLSlotElement>(parentElement())) {
      DCHECK(parent_slot->SupportsAssignment());
      // The parent_slot's assigned nodes might not be calculated because they
      // are lazy evaluated later at UpdateDistribution() so we have to check it
      // here.
      if (!parent_slot->HasAssignedNodesSlow())
        parent_slot->DidSlotChange(slot_change_type);
    }
  }
}

bool Node::IsEffectiveRootScroller() const {
  return GetLayoutObject() ? GetLayoutObject()->IsEffectiveRootScroller()
                           : false;
}

LayoutBox* Node::AutoscrollBox() {
  return nullptr;
}

void Node::StopAutoscroll() {}

WebPluginContainerImpl* Node::GetWebPluginContainer() const {
  if (!IsA<HTMLObjectElement>(this) && !IsA<HTMLEmbedElement>(this)) {
    return nullptr;
  }

  LayoutObject* object = GetLayoutObject();
  if (object && object->IsLayoutEmbeddedContent()) {
    return ToLayoutEmbeddedContent(object)->Plugin();
  }

  return nullptr;
}

bool Node::HasMediaControlAncestor() const {
  const Node* current = this;

  while (current) {
    if (current->IsMediaControls() || current->IsMediaControlElement())
      return true;

    if (current->IsShadowRoot())
      current = current->OwnerShadowHost();
    else
      current = current->ParentOrShadowHostElement();
  }

  return false;
}

void Node::FlatTreeParentChanged() {
  if (!isConnected())
    return;
  // TODO(futhark): Replace with DCHECK(IsSlotable()) when Shadow DOM V0 support
  // is removed.
  if (!IsElementNode() && !IsTextNode()) {
    DCHECK(GetDocument().MayContainV0Shadow());
    return;
  }
  if (const ComputedStyle* style = GetComputedStyle()) {
    // We are moving a node with ensured computed style into the flat tree.
    // Clear ensured styles so that we can use IsEnsuredOutsideFlatTree() to
    // determine that we are outside the flat tree before updating the style
    // recalc root in MarkAncestorsWithChildNeedsStyleRecalc().
    if (style->IsEnsuredOutsideFlatTree())
      DetachLayoutTree();
  }
  // The node changed the flat tree position by being slotted to a new slot or
  // slotted for the first time. We need to recalc style since the inheritance
  // parent may have changed.
  if (NeedsStyleRecalc()) {
    // The ancestor chain may have changed. We need to make sure that the
    // child-dirty flags are updated, but the SetNeedsStyleRecalc() call below
    // will skip MarkAncestorsWithChildNeedsStyleRecalc() if the node was
    // already dirty.
    MarkAncestorsWithChildNeedsStyleRecalc();
  }
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kFlatTreeChange));
  // We also need to force a layout tree re-attach since the layout tree parent
  // box may have changed.
  SetForceReattachLayoutTree();
}

void Node::RemovedFromFlatTree() {
  // This node was previously part of the flat tree, but due to slot re-
  // assignment it no longer is. We need to detach the layout tree and notify
  // the StyleEngine in case the StyleRecalcRoot is removed from the flat tree.
  DetachLayoutTree();
  GetDocument().GetStyleEngine().RemovedFromFlatTree(*this);
}

void Node::RegisterScrollTimeline(ScrollTimeline* timeline) {
  EnsureRareData().RegisterScrollTimeline(timeline);
}
void Node::UnregisterScrollTimeline(ScrollTimeline* timeline) {
  EnsureRareData().UnregisterScrollTimeline(timeline);
}

void Node::Trace(Visitor* visitor) const {
  visitor->Trace(parent_or_shadow_host_node_);
  visitor->Trace(previous_);
  visitor->Trace(next_);
  visitor->Trace(data_);
  visitor->Trace(tree_scope_);
  EventTarget::Trace(visitor);
}

}  // namespace blink

#if DCHECK_IS_ON()

void showNode(const blink::Node* node) {
  if (node)
    LOG(INFO) << *node;
  else
    LOG(INFO) << "Cannot showNode for <null>";
}

void showTree(const blink::Node* node) {
  if (node)
    LOG(INFO) << "\n" << node->ToTreeStringForThis().Utf8();
  else
    LOG(INFO) << "Cannot showTree for <null>";
}

void showNodePath(const blink::Node* node) {
  if (node) {
    std::stringstream stream;
    node->PrintNodePathTo(stream);
    LOG(INFO) << stream.str();
  } else {
    LOG(INFO) << "Cannot showNodePath for <null>";
  }
}

#endif
