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

#include "third_party/blink/renderer/bindings/core/v8/node_or_string.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
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
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/get_root_node_options.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
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
#include "third_party/blink/renderer/core/frame/use_counter.h"
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
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_customization_callbacks.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state_callback.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable_visitor.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
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
                      (new ScrollCustomizationCallbacks));
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
    builder.Append(impl->Is8Bit()
                       ? StringView(impl->Characters8(), impl->length())
                       : StringView(impl->Characters16(), impl->length()));
  }
}

}  // namespace

using namespace HTMLNames;

struct SameSizeAsNode : EventTarget {
  uint32_t node_flags_;
  Member<void*> willbe_member_[4];
  void* pointer_;
};

NodeRenderingData::NodeRenderingData(
    LayoutObject* layout_object,
    scoped_refptr<ComputedStyle> non_attached_style)
    : layout_object_(layout_object), non_attached_style_(non_attached_style) {}

NodeRenderingData::~NodeRenderingData() {
  CHECK(!layout_object_);
}

void NodeRenderingData::SetNonAttachedStyle(
    scoped_refptr<ComputedStyle> non_attached_style) {
  DCHECK_NE(&SharedEmptyData(), this);
  DCHECK(!non_attached_style || !non_attached_style_);
  non_attached_style_ = non_attached_style;
}

NodeRenderingData& NodeRenderingData::SharedEmptyData() {
  DEFINE_STATIC_LOCAL(NodeRenderingData, shared_empty_data, (nullptr, nullptr));
  return shared_empty_data;
}

static_assert(sizeof(Node) <= sizeof(SameSizeAsNode), "Node should stay small");

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
        if (node->isElementNode()) {
          ++elementsWithRareData;
          if (toElement(node)->hasNamedNodeMap())
            ++elementsWithNamedNodeMap;
        }
      }

      switch (node->getNodeType()) {
        case kElementNode: {
          ++elementNodes;

          // Tag stats
          Element* element = toElement(node);
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
      next_(nullptr) {
  DCHECK(tree_scope_ || type == kCreateDocument || type == kCreateShadowRoot);
#if !defined(NDEBUG) || (defined(DUMP_NODE_STATISTICS) && DUMP_NODE_STATISTICS)
  TrackForDebugging();
#endif
  InstanceCounters::IncrementCounter(InstanceCounters::kNodeCounter);
}

Node::~Node() {
  if (!HasRareData() && !data_.node_layout_data_->IsSharedEmptyData())
    delete data_.node_layout_data_;
  InstanceCounters::DecrementCounter(InstanceCounters::kNodeCounter);
}

NodeRareData& Node::CreateRareData() {
  if (IsElementNode())
    data_.rare_data_ = ElementRareData::Create(data_.node_layout_data_);
  else
    data_.rare_data_ = NodeRareData::Create(data_.node_layout_data_);

  DCHECK(data_.rare_data_);
  SetFlag(kHasRareDataFlag);
  ScriptWrappableMarkingVisitor::WriteBarrier(RareData());
  MarkingVisitor::WriteBarrier(RareData());
  return *RareData();
}

Node* Node::ToNode() {
  return this;
}

int Node::tabIndex() const {
  return 0;
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

ContainerNode* Node::ParentNodeWithCounting() const {
  if (GetFlag(kInDOMNodeRemovedHandler))
    GetDocument().CountDetachingNodeAccessInDOMNodeRemovedHandler();
  return IsShadowRoot() ? nullptr : ParentOrShadowHostNode();
}

NodeList* Node::childNodes() {
  ThreadState::MainThreadGCForbiddenScope gc_forbidden;
  if (IsContainerNode())
    return EnsureRareData().EnsureNodeLists().EnsureChildNodeList(
        ToContainerNode(*this));
  return EnsureRareData().EnsureNodeLists().EnsureEmptyChildNodeList(*this);
}

Node* Node::PseudoAwarePreviousSibling() const {
  if (parentElement() && !previousSibling()) {
    Element* parent = parentElement();
    if (IsAfterPseudoElement() && parent->lastChild())
      return parent->lastChild();
    if (!IsBeforePseudoElement())
      return parent->GetPseudoElement(kPseudoIdBefore);
  }
  return previousSibling();
}

Node* Node::PseudoAwareNextSibling() const {
  if (parentElement() && !nextSibling()) {
    Element* parent = parentElement();
    if (IsBeforePseudoElement() && parent->HasChildren())
      return parent->firstChild();
    if (!IsAfterPseudoElement())
      return parent->GetPseudoElement(kPseudoIdAfter);
  }
  return nextSibling();
}

Node* Node::PseudoAwareFirstChild() const {
  if (IsElementNode()) {
    const Element* current_element = ToElement(this);
    Node* first = current_element->GetPseudoElement(kPseudoIdBefore);
    if (first)
      return first;
    first = current_element->firstChild();
    if (!first)
      first = current_element->GetPseudoElement(kPseudoIdAfter);
    return first;
  }

  return firstChild();
}

Node* Node::PseudoAwareLastChild() const {
  if (IsElementNode()) {
    const Element* current_element = ToElement(this);
    Node* last = current_element->GetPseudoElement(kPseudoIdAfter);
    if (last)
      return last;
    last = current_element->lastChild();
    if (!last)
      last = current_element->GetPseudoElement(kPseudoIdBefore);
    return last;
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

Node* Node::getRootNode(const GetRootNodeOptions& options) const {
  return (options.hasComposed() && options.composed()) ? &ShadowIncludingRoot()
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

  // All elements in the scroll chain should be boxes.
  DCHECK(GetLayoutObject()->IsBox());

  if (scroll_state.FullyConsumed())
    return;

  FloatSize delta(scroll_state.deltaX(), scroll_state.deltaY());

  if (delta.IsZero())
    return;

  // TODO(esprehn): This should use
  // updateStyleAndLayoutIgnorePendingStylesheetsForNode.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  LayoutBox* box_to_scroll = ToLayoutBox(GetLayoutObject());

  ScrollableArea* scrollable_area =
      box_to_scroll->EnclosingBox()->GetScrollableArea();

  if (!scrollable_area)
    return;

  ScrollResult result = scrollable_area->UserScroll(
      ScrollGranularity(static_cast<int>(scroll_state.deltaGranularity())),
      delta);

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

  disable_custom_callbacks |=
      !root_scroller_util::IsGlobal(this) &&
      RuntimeEnabledFeatures::ScrollCustomizationEnabled() &&
      !GetScrollCustomizationCallbacks().InScrollPhase(this);

  if (!callback || disable_custom_callbacks) {
    NativeDistributeScroll(scroll_state);
    return;
  }
  if (callback->NativeScrollBehavior() !=
      WebNativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
  if (callback->NativeScrollBehavior() !=
      WebNativeScrollBehavior::kDisableNativeScroll)
    NativeDistributeScroll(scroll_state);
  if (callback->NativeScrollBehavior() ==
      WebNativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
}

void Node::CallApplyScroll(ScrollState& scroll_state) {
  TRACE_EVENT0("input", "Node::CallApplyScroll");
  // Hits ASSERTs when trying to determine whether we need to scroll on main
  // or CC. http://crbug.com/625676.
  DisableCompositingQueryAsserts disabler;

  if (!GetDocument().GetPage()) {
    // We should always have a Page if we're scrolling. See
    // crbug.com/689074 for details.
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
  disable_custom_callbacks |=
      !root_scroller_util::IsGlobal(this) &&
      RuntimeEnabledFeatures::ScrollCustomizationEnabled() &&
      !GetScrollCustomizationCallbacks().InScrollPhase(this);

  if (!callback || disable_custom_callbacks) {
    NativeApplyScroll(scroll_state);
    return;
  }
  if (callback->NativeScrollBehavior() !=
      WebNativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
  if (callback->NativeScrollBehavior() !=
      WebNativeScrollBehavior::kDisableNativeScroll)
    NativeApplyScroll(scroll_state);
  if (callback->NativeScrollBehavior() ==
      WebNativeScrollBehavior::kPerformAfterNativeScroll)
    callback->Invoke(&scroll_state);
}

void Node::WillBeginCustomizedScrollPhase(
    ScrollCustomization::ScrollDirection direction) {
  DCHECK(!GetScrollCustomizationCallbacks().InScrollPhase(this));
  LayoutBox* box = GetLayoutBox();
  if (!box)
    return;

  ScrollCustomization::ScrollDirection scroll_customization =
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
  if (IsContainerNode())
    return ToContainerNode(this)->InsertBefore(new_child, ref_child,
                                               exception_state);

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
  if (IsContainerNode())
    return ToContainerNode(this)->ReplaceChild(new_child, old_child,
                                               exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kHierarchyRequestError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::replaceChild(Node* new_child, Node* old_child) {
  return replaceChild(new_child, old_child, ASSERT_NO_EXCEPTION);
}

Node* Node::removeChild(Node* old_child, ExceptionState& exception_state) {
  if (IsContainerNode())
    return ToContainerNode(this)->RemoveChild(old_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotFoundError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::removeChild(Node* old_child) {
  return removeChild(old_child, ASSERT_NO_EXCEPTION);
}

Node* Node::appendChild(Node* new_child, ExceptionState& exception_state) {
  if (IsContainerNode())
    return ToContainerNode(this)->AppendChild(new_child, exception_state);

  exception_state.ThrowDOMException(
      DOMExceptionCode::kHierarchyRequestError,
      "This node type does not support this method.");
  return nullptr;
}

Node* Node::appendChild(Node* new_child) {
  return appendChild(new_child, ASSERT_NO_EXCEPTION);
}

static bool IsNodeInNodes(const Node* const node,
                          const HeapVector<NodeOrString>& nodes) {
  for (const NodeOrString& node_or_string : nodes) {
    if (node_or_string.IsNode() && node_or_string.GetAsNode() == node)
      return true;
  }
  return false;
}

static Node* FindViablePreviousSibling(const Node& node,
                                       const HeapVector<NodeOrString>& nodes) {
  for (Node* sibling = node.previousSibling(); sibling;
       sibling = sibling->previousSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* FindViableNextSibling(const Node& node,
                                   const HeapVector<NodeOrString>& nodes) {
  for (Node* sibling = node.nextSibling(); sibling;
       sibling = sibling->nextSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* NodeOrStringToNode(const NodeOrString& node_or_string,
                                Document& document) {
  if (node_or_string.IsNode())
    return node_or_string.GetAsNode();
  return Text::Create(document, node_or_string.GetAsString());
}

// Returns nullptr if an exception was thrown.
static Node* ConvertNodesIntoNode(const HeapVector<NodeOrString>& nodes,
                                  Document& document,
                                  ExceptionState& exception_state) {
  if (nodes.size() == 1)
    return NodeOrStringToNode(nodes[0], document);

  Node* fragment = DocumentFragment::Create(document);
  for (const NodeOrString& node_or_string : nodes) {
    fragment->appendChild(NodeOrStringToNode(node_or_string, document),
                          exception_state);
    if (exception_state.HadException())
      return nullptr;
  }
  return fragment;
}

void Node::Prepend(const HeapVector<NodeOrString>& nodes,
                   ExceptionState& exception_state) {
  if (Node* node = ConvertNodesIntoNode(nodes, GetDocument(), exception_state))
    insertBefore(node, firstChild(), exception_state);
}

void Node::Append(const HeapVector<NodeOrString>& nodes,
                  ExceptionState& exception_state) {
  if (Node* node = ConvertNodesIntoNode(nodes, GetDocument(), exception_state))
    appendChild(node, exception_state);
}

void Node::Before(const HeapVector<NodeOrString>& nodes,
                  ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viable_previous_sibling = FindViablePreviousSibling(*this, nodes);
  if (Node* node = ConvertNodesIntoNode(nodes, GetDocument(), exception_state))
    parent->insertBefore(node,
                         viable_previous_sibling
                             ? viable_previous_sibling->nextSibling()
                             : parent->firstChild(),
                         exception_state);
}

void Node::After(const HeapVector<NodeOrString>& nodes,
                 ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  if (Node* node = ConvertNodesIntoNode(nodes, GetDocument(), exception_state))
    parent->insertBefore(node, viable_next_sibling, exception_state);
}

void Node::ReplaceWith(const HeapVector<NodeOrString>& nodes,
                       ExceptionState& exception_state) {
  Node* parent = parentNode();
  if (!parent)
    return;
  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  Node* node = ConvertNodesIntoNode(nodes, GetDocument(), exception_state);
  if (exception_state.HadException())
    return;
  if (parent == parentNode())
    parent->replaceChild(node, this, exception_state);
  else
    parent->insertBefore(node, viable_next_sibling, exception_state);
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

  // 1. If context object is a shadow root, then throw a
  // "NotSupportedError" DOMException.
  if (IsShadowRoot()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "ShadowRoot nodes are not clonable.");
    return nullptr;
  }

  // 2. Return a clone of the context object, with the clone children
  // flag set if deep is true.
  return Clone(GetDocument(),
               deep ? CloneChildrenFlag::kClone : CloneChildrenFlag::kSkip);
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
      node = ToText(node)->MergeNextSiblingNodesIfPossible();
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
      HasRareData() ? data_.rare_data_->GetNodeRenderingData()
                    : data_.node_layout_data_;

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
  DCHECK(!node_layout_data->GetNonAttachedStyle());
  node_layout_data = new NodeRenderingData(layout_object, nullptr);
  if (HasRareData())
    data_.rare_data_->SetNodeRenderingData(node_layout_data);
  else
    data_.node_layout_data_ = node_layout_data;
}

void Node::SetNonAttachedStyle(
    scoped_refptr<ComputedStyle> non_attached_style) {
  // We don't set non-attached style for text nodes.
  DCHECK(IsElementNode());

  NodeRenderingData* node_layout_data =
      HasRareData() ? data_.rare_data_->GetNodeRenderingData()
                    : data_.node_layout_data_;

  // Already pointing to a non empty NodeRenderingData so just set the pointer
  // to the new LayoutObject.
  if (!node_layout_data->IsSharedEmptyData()) {
    node_layout_data->SetNonAttachedStyle(non_attached_style);
    return;
  }

  if (!non_attached_style)
    return;

  // Ensure we don't unnecessarily set non-attached style for elements which are
  // not part of the flat tree and consequently won't be attached.
  DCHECK(LayoutTreeBuilderTraversal::Parent(*this));

  // Swap the NodeRenderingData to point to a new NodeRenderingData instead of
  // the static SharedEmptyData instance.
  DCHECK(!node_layout_data->GetLayoutObject());
  node_layout_data = new NodeRenderingData(nullptr, non_attached_style);
  if (HasRareData())
    data_.rare_data_->SetNodeRenderingData(node_layout_data);
  else
    data_.node_layout_data_ = node_layout_data;
}

LayoutBoxModelObject* Node::GetLayoutBoxModelObject() const {
  LayoutObject* layout_object = GetLayoutObject();
  return layout_object && layout_object->IsBoxModelObject()
             ? ToLayoutBoxModelObject(layout_object)
             : nullptr;
}

LayoutRect Node::BoundingBox() const {
  if (GetLayoutObject())
    return LayoutRect(GetLayoutObject()->AbsoluteBoundingBoxRect());
  return LayoutRect();
}

LayoutRect Node::BoundingBoxForScrollIntoView() const {
  if (GetLayoutObject()) {
    return LayoutRect(
        GetLayoutObject()->AbsoluteBoundingBoxRectForScrollIntoView());
  }

  return LayoutRect();
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
    if (root.IsShadowRoot() && !ToShadowRoot(root).IsOpenOrV0())
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
  SetFlag(is_link && !SVGImage::IsInSVGImage(ToElement(this)), kIsLinkFlag);
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

inline void Node::SetStyleChange(StyleChangeType change_type) {
  node_flags_ = (node_flags_ & ~kStyleChangeMask) | change_type;
}

void Node::MarkAncestorsWithChildNeedsStyleRecalc() {
  ContainerNode* ancestor = ParentOrShadowHostNode();
  bool parent_dirty = ancestor && ancestor->NeedsStyleRecalc();
  for (; ancestor && !ancestor->ChildNeedsStyleRecalc();
       ancestor = ancestor->ParentOrShadowHostNode()) {
    ancestor->SetChildNeedsStyleRecalc();
    if (ancestor->NeedsStyleRecalc())
      break;
  }
  if (!isConnected())
    return;
  // If the parent node is already dirty, we can keep the same recalc root. The
  // early return here is a performance optimization.
  if (parent_dirty)
    return;
  GetDocument().GetStyleEngine().UpdateStyleRecalcRoot(ancestor, this);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

ContainerNode* Node::GetReattachParent() const {
  if (IsPseudoElement())
    return ParentOrShadowHostNode();
  if (IsChildOfV1ShadowHost()) {
    if (HTMLSlotElement* slot = AssignedSlot())
      return slot;
  }
  if (IsInV0ShadowTree() || IsChildOfV0ShadowHost()) {
    if (ShadowRootWhereNodeCanBeDistributedForV0(*this)) {
      if (V0InsertionPoint* insertion_point =
              const_cast<V0InsertionPoint*>(ResolveReprojection(this))) {
        return insertion_point;
      }
    }
  }
  return ParentOrShadowHostNode();
}

void Node::MarkAncestorsWithChildNeedsReattachLayoutTree() {
  DCHECK(isConnected());
  ContainerNode* ancestor = GetReattachParent();
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
  SetFlag(kNeedsReattachLayoutTree);
  MarkAncestorsWithChildNeedsReattachLayoutTree();
}

void Node::SetNeedsStyleRecalc(StyleChangeType change_type,
                               const StyleChangeReasonForTracing& reason) {
  DCHECK(!GetDocument().GetStyleEngine().InRebuildLayoutTree());
  DCHECK(change_type != kNoStyleChange);
  if (!InActiveDocument())
    return;
  if (!IsContainerNode() && !IsTextNode())
    return;

  TRACE_EVENT_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "StyleRecalcInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorStyleRecalcInvalidationTrackingEvent::Data(this, reason));

  StyleChangeType existing_change_type = GetStyleChangeType();
  if (change_type > existing_change_type)
    SetStyleChange(change_type);

  if (existing_change_type == kNoStyleChange)
    MarkAncestorsWithChildNeedsStyleRecalc();

  if (IsElementNode() && HasRareData())
    ToElement(*this).SetAnimationStyleChange(false);

  if (IsSVGElement())
    ToSVGElement(this)->SetNeedsStyleRecalcForInstances(change_type, reason);
}

void Node::ClearNeedsStyleRecalc() {
  node_flags_ &= ~kStyleChangeMask;

  if (IsElementNode() && HasRareData())
    ToElement(*this).SetAnimationStyleChange(false);
}

bool Node::InActiveDocument() const {
  return isConnected() && GetDocument().IsActive();
}

const Node* Node::FocusDelegate() const {
  return this;
}

bool Node::ShouldHaveFocusAppearance() const {
  DCHECK(IsFocused());
  return true;
}

bool Node::IsInert() const {
  if (!isConnected() || !CanParticipateInFlatTree())
    return true;

  DCHECK(!ChildNeedsDistributionRecalc());

  if (this != GetDocument()) {
    const Element* modal_element = GetDocument().ActiveModalDialog();
    if (!modal_element)
      modal_element = Fullscreen::FullscreenElementFrom(GetDocument());
    if (modal_element && !FlatTreeTraversal::ContainsIncludingPseudoElement(
                             *modal_element, *this)) {
      return true;
    }
  }

  if (RuntimeEnabledFeatures::InertAttributeEnabled()) {
    const Element* element = IsElementNode()
                                 ? ToElement(this)
                                 : FlatTreeTraversal::ParentElement(*this);
    while (element) {
      if (element->hasAttribute(HTMLNames::inertAttr))
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

bool Node::IsDescendantOf(const Node* other) const {
  // Return true if other is an ancestor of this, otherwise false
  if (!other || !other->hasChildren() || isConnected() != other->isConnected())
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

bool Node::IsShadowIncludingInclusiveAncestorOf(const Node* node) const {
  if (!node)
    return false;

  if (this == node)
    return true;

  if (GetDocument() != node->GetDocument())
    return false;

  if (isConnected() != node->isConnected())
    return false;

  bool has_children = IsContainerNode() && ToContainerNode(this)->HasChildren();
  bool has_shadow = IsShadowHost(this);
  if (!has_children && !has_shadow)
    return false;

  for (; node; node = node->OwnerShadowHost()) {
    if (GetTreeScope() == node->GetTreeScope())
      return contains(node);
  }

  return false;
}

bool Node::ContainsIncludingHostElements(const Node& node) const {
  const Node* current = &node;
  do {
    if (current == this)
      return true;
    if (current->IsDocumentFragment() &&
        ToDocumentFragment(current)->IsTemplateContent())
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

  // We only need to detach if the node has already been through
  // attachLayoutTree().
  if (GetStyleChangeType() < kNeedsReattachStyleChange)
    DetachLayoutTree(context);
  AttachLayoutTree(context);
  DCHECK(!NeedsReattachLayoutTree());
  DCHECK(!GetNonAttachedStyle());
}

void Node::AttachLayoutTree(AttachContext& context) {
  DCHECK(GetDocument().InStyleRecalc() || IsDocumentNode());
  DCHECK(!GetDocument().Lifecycle().InDetach());
  DCHECK(NeedsAttach());

  LayoutObject* layout_object = GetLayoutObject();
  DCHECK(!layout_object ||
         (layout_object->Style() &&
          (layout_object->Parent() || layout_object->IsLayoutView())));

  ClearNeedsStyleRecalc();
  ClearNeedsReattachLayoutTree();

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->UpdateCacheAfterNodeIsAttached(this);
}

void Node::DetachLayoutTree(const AttachContext& context) {
  DCHECK(GetDocument().Lifecycle().StateAllowsDetach());
  DocumentLifecycle::DetachScope will_detach(GetDocument().Lifecycle());

  if (GetLayoutObject())
    GetLayoutObject()->DestroyAndCleanupAnonymousWrappers();
  SetLayoutObject(nullptr);
  SetStyleChange(kNeedsReattachStyleChange);
  ClearChildNeedsStyleInvalidation();
}

const ComputedStyle* Node::VirtualEnsureComputedStyle(
    PseudoId pseudo_element_specifier) {
  return ParentOrShadowHostNode()
             ? ParentOrShadowHostNode()->EnsureComputedStyle(
                   pseudo_element_specifier)
             : nullptr;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks
// questions about HTML in the core DOM class is obviously misplaced.
bool Node::CanStartSelection() const {
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
  return IsHTMLElement() || IsSVGElement() ||
         (IsElementNode() &&
          ToElement(this)->namespaceURI() == mathml_names::kNamespaceURI);
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
  if (IsElementNode())
    return HTMLSlotElement::NormalizeSlotName(
        ToElement(*this).FastGetAttribute(HTMLNames::slotAttr));
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
  return root.IsShadowRoot() ? ToShadowRoot(&root) : nullptr;
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

  if (parent->IsShadowRoot())
    return &ToShadowRoot(parent)->host();

  if (!parent->IsElementNode())
    return nullptr;

  return ToElement(parent);
}

ContainerNode* Node::ParentOrShadowHostOrTemplateHostNode() const {
  if (IsDocumentFragment() && ToDocumentFragment(this)->IsTemplateContent())
    return static_cast<const TemplateContentDocumentFragment*>(this)->Host();
  return ParentOrShadowHostNode();
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

  if (IsAttributeNode()) {
    if (ToAttr(this)->localName() != ToAttr(other)->localName())
      return false;

    if (ToAttr(this)->namespaceURI() != ToAttr(other)->namespaceURI())
      return false;
  } else if (IsElementNode()) {
    if (ToElement(this)->TagQName() != ToElement(other)->TagQName())
      return false;

    if (!ToElement(this)->HasEquivalentAttributes(ToElement(*other)))
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

  if (IsDocumentTypeNode()) {
    const DocumentType* document_type_this = ToDocumentType(this);
    const DocumentType* document_type_other = ToDocumentType(other);

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
      context = ToElement(this);
      break;
    case kDocumentNode:
      context = To<Document>(this)->documentElement();
      break;
    case kDocumentFragmentNode:
    case kDocumentTypeNode:
      context = nullptr;
      break;
    case kAttributeNode:
      context = ToAttr(this)->ownerElement();
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
      const Element& element = ToElement(*this);

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
      const Attr* attr = ToAttr(this);
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
  if (IsCharacterDataNode())
    return ToCharacterData(this)->data();

  // Attribute nodes have their attribute values as textContent.
  if (IsAttributeNode())
    return ToAttr(this)->value();

  // Documents and non-container nodes (that are not CharacterData)
  // have null textContent.
  if (IsDocumentNode() || !IsContainerNode())
    return String();

  StringBuilder content;
  for (const Node& node : NodeTraversal::InclusiveDescendantsOf(*this)) {
    if (IsHTMLBRElement(node) && convert_brs_to_newlines) {
      content.Append('\n');
    } else if (node.IsTextNode()) {
      content.Append(ToText(node).data());
    }
  }
  return content.ToString();
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
      ContainerNode* container = ToContainerNode(this);

      // Note: This is an intentional optimization.
      // See crbug.com/352836 also.
      // No need to do anything if the text is identical.
      if (container->HasOneTextChild() &&
          ToText(container->firstChild())->data() == text && !text.IsEmpty())
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

unsigned short Node::compareDocumentPosition(
    const Node* other_node,
    ShadowTreesTreatment treatment) const {
  if (other_node == this)
    return kDocumentPositionEquivalent;

  const Attr* attr1 = getNodeType() == kAttributeNode ? ToAttr(this) : nullptr;
  const Attr* attr2 = other_node->getNodeType() == kAttributeNode
                          ? ToAttr(other_node)
                          : nullptr;

  const Node* start1 = attr1 ? attr1->ownerElement() : this;
  const Node* start2 = attr2 ? attr2->ownerElement() : other_node;

  // If either of start1 or start2 is null, then we are disconnected, since one
  // of the nodes is an orphaned attribute node.
  if (!start1 || !start2) {
    unsigned short direction = (this > other_node) ? kDocumentPositionPreceding
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
    unsigned short direction = (this > other_node) ? kDocumentPositionPreceding
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
    unsigned short direction = (this > other_node) ? kDocumentPositionPreceding
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
  if (IsElementNode()) {
    const Element& this_element = ToElement(*this);
    if (this_element.HasID()) {
      name.Append(" id=\'");
      AppendUnsafe(name, this_element.GetIdAttribute());
      name.Append('\'');
    }

    if (this_element.HasClass()) {
      name.Append(" class=\'");
      for (wtf_size_t i = 0; i < this_element.ClassNames().size(); ++i) {
        if (i > 0)
          name.Append(' ');
        AppendUnsafe(name, this_element.ClassNames()[i]);
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
  if (!node.IsElementNode())
    return;
  const AtomicString& value = ToElement(node).getAttribute(name);
  if (value.IsEmpty())
    return;
  builder.Append(' ');
  builder.Append(name.ToString());
  builder.Append("=");
  builder.Append(String(value).EncodeForDebugging());
}

std::ostream& operator<<(std::ostream& ostream, const Node& node) {
  return ostream << node.ToString().Utf8().data();
}

std::ostream& operator<<(std::ostream& ostream, const Node* node) {
  if (!node)
    return ostream << "null";
  return ostream << *node;
}

String Node::ToString() const {
  if (getNodeType() == Node::kProcessingInstructionNode)
    return "?" + nodeName();
  if (IsShadowRoot()) {
    // nodeName of ShadowRoot is #document-fragment.  It's confused with
    // DocumentFragment.
    std::stringstream shadow_root_type;
    shadow_root_type << ToShadowRoot(this)->GetType();
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
  DumpAttributeDesc(*this, HTMLNames::idAttr, builder);
  DumpAttributeDesc(*this, HTMLNames::classAttr, builder);
  DumpAttributeDesc(*this, HTMLNames::styleAttr, builder);
  if (HasEditableStyle(*this))
    builder.Append(" (editable)");
  if (GetDocument().FocusedElement() == this)
    builder.Append(" (focused)");
  return builder.ToString();
}

#ifndef NDEBUG

String Node::ToTreeStringForThis() const {
  return ToMarkedTreeString(this, "*");
}

String Node::ToFlatTreeStringForThis() const {
  return ToMarkedFlatTreeString(this, "*");
}

void Node::PrintNodePathTo(std::ostream& stream) const {
  HeapVector<Member<const Node>, 16> chain;
  const Node* node = this;
  while (node->ParentOrShadowHostNode()) {
    chain.push_back(node);
    node = node->ParentOrShadowHostNode();
  }
  for (unsigned index = chain.size(); index > 0; --index) {
    const Node* node = chain[index - 1];
    if (node->IsShadowRoot()) {
      stream << "/#shadow-root";
      continue;
    }

    switch (node->getNodeType()) {
      case kElementNode: {
        stream << "/" << node->nodeName().Utf8().data();

        const Element* element = ToElement(node);
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
            stream << "[@id=\"" << idattr.Utf8().data()
                   << "\" and position()=" << count << "]";
          else
            stream << "[" << count << "]";
        } else if (has_id_attr) {
          stream << "[@id=\"" << idattr.Utf8().data() << "\"]";
        }
        break;
      }
      case kTextNode:
        stream << "/text()";
        break;
      case kAttributeNode:
        stream << "/@" << node->nodeName().Utf8().data();
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

    if (node.IsElementNode()) {
      const Element& element = ToElement(node);
      if (Element* pseudo = element.GetPseudoElement(kPseudoIdBefore))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element.GetPseudoElement(kPseudoIdAfter))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element.GetPseudoElement(kPseudoIdFirstLetter))
        AppendMarkedTree(indent.ToString(), pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element.GetPseudoElement(kPseudoIdBackdrop))
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
  while (node->ParentOrShadowHostNode() && !IsHTMLBodyElement(*node))
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
  while (node->ParentOrShadowHostNode() && !IsHTMLBodyElement(*node))
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
  stream << indent.Utf8().data() << *node << "\n";
  if (node->IsFrameOwnerElement()) {
    PrintSubTreeAcrossFrame(ToHTMLFrameOwnerElement(node)->contentDocument(),
                            marked_node, indent + "\t", stream);
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
    if (node->IsLink() && !IsHTMLImageElement(*node)) {
      // Casting to Element is safe because only HTMLAnchorElement,
      // HTMLImageElement and SVGAElement can return true for isLink().
      return ToElement(const_cast<Node*>(node));
    }
  }

  return nullptr;
}

const AtomicString& Node::InterfaceName() const {
  return EventTargetNames::Node;
}

ExecutionContext* Node::GetExecutionContext() const {
  return GetDocument().ContextDocument();
}

void Node::WillMoveToNewDocument(Document& old_document,
                                 Document& new_document) {
  if (!old_document.GetPage() ||
      old_document.GetPage() == new_document.GetPage())
    return;

  old_document.GetFrame()->GetEventHandlerRegistry().DidMoveOutOfPage(*this);

  if (IsElementNode()) {
    StylePropertyMapReadOnly* computed_style_map_item =
        old_document.RemoveComputedStyleMapItem(ToElement(this));
    if (computed_style_map_item) {
      new_document.AddComputedStyleMapItem(ToElement(this),
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
  if (IsTextNode())
    old_document.Markers().RemoveMarkersForNode(*ToText(this));
  if (GetDocument().GetPage() &&
      GetDocument().GetPage() != old_document.GetPage()) {
    GetDocument().GetFrame()->GetEventHandlerRegistry().DidMoveIntoPage(*this);
  }

  if (const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
          registry = MutationObserverRegistry()) {
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
    HeapHashMap<WeakMember<Node>, TraceWrapperMember<EventTargetData>>;
static EventTargetDataMap& GetEventTargetDataMap() {
  DEFINE_STATIC_LOCAL(Persistent<EventTargetDataMap>, map,
                      (new EventTargetDataMap));
  return *map;
}

EventTargetData* Node::GetEventTargetData() {
  return HasEventTargetData() ? GetEventTargetDataMap().at(this) : nullptr;
}

EventTargetData& Node::EnsureEventTargetData() {
  if (HasEventTargetData())
    return *GetEventTargetDataMap().at(this);
  DCHECK(!GetEventTargetDataMap().Contains(this));
  SetHasEventTargetData(true);
  EventTargetData* data = new EventTargetData;
  GetEventTargetDataMap().Set(this, data);
  return *data;
}

const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
Node::MutationObserverRegistry() {
  if (!HasRareData())
    return nullptr;
  NodeMutationObserverData* data = RareData()->MutationObserverData();
  if (!data)
    return nullptr;
  return &data->Registry();
}

const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
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
    registration = MutationObserverRegistration::Create(observer, this, options,
                                                        attribute_filter);
    EnsureRareData().EnsureMutationObserverData().AddRegistration(registration);
  }

  GetDocument().AddMutationObserverTypes(registration->MutationTypes());
}

void Node::UnregisterMutationObserver(
    MutationObserverRegistration* registration) {
  const HeapVector<TraceWrapperMember<MutationObserverRegistration>>* registry =
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
  const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
      transient_registry = TransientMutationObserverRegistry();
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
    if (const HeapVector<TraceWrapperMember<MutationObserverRegistration>>*
            registry = node->MutationObserverRegistry()) {
      for (const auto& registration : *registry)
        registration->ObservedSubtreeNodeWillDetach(*this);
    }

    if (const HeapHashSet<TraceWrapperMember<MutationObserverRegistration>>*
            transient_registry = node->TransientMutationObserverRegistry()) {
      for (auto& registration : *transient_registry)
        registration->ObservedSubtreeNodeWillDetach(*this);
    }
  }
}

void Node::HandleLocalEvents(Event& event) {
  if (!HasEventTargetData())
    return;

  if (IsDisabledFormControl(this) && event.IsMouseEvent() &&
      !RuntimeEnabledFeatures::SendMouseEventsDisabledFormControlsEnabled()) {
    if (HasEventListeners(event.type())) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kDispatchMouseEventOnDisabledFormControl);
      if (event.type() == EventTypeNames::mousedown ||
          event.type() == EventTypeNames::mouseup) {
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

  DispatchScopedEvent(*MutationEvent::Create(EventTypeNames::DOMSubtreeModified,
                                             Event::Bubbles::kYes));
}

DispatchEventResult Node::DispatchDOMActivateEvent(int detail,
                                                   Event& underlying_event) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  UIEvent& event = *UIEvent::Create();
  event.initUIEvent(EventTypeNames::DOMActivate, true, true,
                    GetDocument().domWindow(), detail);
  event.SetUnderlyingEvent(&underlying_event);
  event.SetComposed(underlying_event.composed());
  DispatchScopedEvent(event);

  // TODO(dtapuska): Dispatching scoped events shouldn't check the return
  // type because the scoped event could get put off in the delayed queue.
  return EventTarget::GetDispatchEventResult(event);
}

void Node::DispatchSimulatedClick(Event* underlying_event,
                                  SimulatedClickMouseEventOptions event_options,
                                  SimulatedClickCreationScope scope) {
  EventDispatcher::DispatchSimulatedClick(*this, underlying_event,
                                          event_options, scope);
}

void Node::DispatchInputEvent() {
  // Legacy 'input' event for forms set value and checked.
  DispatchScopedEvent(*Event::CreateBubble(EventTypeNames::input));
}

void Node::DefaultEventHandler(Event& event) {
  if (event.target() != this)
    return;
  const AtomicString& event_type = event.type();
  if (event_type == EventTypeNames::keydown ||
      event_type == EventTypeNames::keypress) {
    if (event.IsKeyboardEvent()) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        frame->GetEventHandler().DefaultKeyboardEventHandler(
            ToKeyboardEvent(&event));
      }
    }
  } else if (event_type == EventTypeNames::click) {
    int detail = event.IsUIEvent() ? ToUIEvent(event).detail() : 0;
    if (DispatchDOMActivateEvent(detail, event) !=
        DispatchEventResult::kNotCanceled)
      event.SetDefaultHandled();
  } else if (event_type == EventTypeNames::contextmenu &&
             event.IsMouseEvent()) {
    if (Page* page = GetDocument().GetPage()) {
      page->GetContextMenuController().HandleContextMenuEvent(
          ToMouseEvent(&event));
    }
  } else if (event_type == EventTypeNames::textInput) {
    if (event.HasInterface(EventNames::TextEvent)) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        frame->GetEventHandler().DefaultTextInputEventHandler(
            ToTextEvent(&event));
      }
    }
  } else if (RuntimeEnabledFeatures::MiddleClickAutoscrollEnabled() &&
             event_type == EventTypeNames::mousedown && event.IsMouseEvent()) {
    auto& mouse_event = ToMouseEvent(event);
    if (mouse_event.button() ==
        static_cast<short>(WebPointerProperties::Button::kMiddle)) {
      if (EnclosingLinkEventParentOrSelf())
        return;

      // Avoid that canBeScrolledAndHasScrollableArea changes layout tree
      // structure.
      // FIXME: We should avoid synchronous layout if possible. We can
      // remove this synchronous layout if we avoid synchronous layout in
      // LayoutTextControlSingleLine::scrollHeight
      GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
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
  } else if (event_type == EventTypeNames::mouseup && event.IsMouseEvent()) {
    auto& mouse_event = ToMouseEvent(event);
    if (mouse_event.button() ==
        static_cast<short>(WebPointerProperties::Button::kBack)) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        if (frame->Client()->NavigateBackForward(-1))
          event.SetDefaultHandled();
      }
    } else if (mouse_event.button() ==
               static_cast<short>(WebPointerProperties::Button::kForward)) {
      if (LocalFrame* frame = GetDocument().GetFrame()) {
        if (frame->Client()->NavigateBackForward(1))
          event.SetDefaultHandled();
      }
    }
  }
}

void Node::WillCallDefaultEventHandler(const Event& event) {
  if (!event.IsKeyboardEvent())
    return;

  if (!IsFocused() || GetDocument().LastFocusType() != kWebFocusTypeMouse)
    return;

  if (event.type() != EventTypeNames::keydown ||
      GetDocument().HadKeyboardEvent())
    return;

  GetDocument().SetHadKeyboardEvent(true);

  // Changes to HadKeyboardEvent may affect :focus-visible matching,
  // ShouldHaveFocusAppearance and LayoutTheme::IsFocused().
  // Inform LayoutTheme if HadKeyboardEvent changes.
  if (GetLayoutObject()) {
    GetLayoutObject()->InvalidateIfControlStateChanged(kFocusControlState);

    if (RuntimeEnabledFeatures::CSSFocusVisibleEnabled() && IsContainerNode())
      ToContainerNode(*this).FocusVisibleStateChanged();
  }
}

bool Node::HasActivationBehavior() const {
  return false;
}

bool Node::WillRespondToMouseMoveEvents() {
  if (IsDisabledFormControl(this))
    return false;
  return HasEventListeners(EventTypeNames::mousemove) ||
         HasEventListeners(EventTypeNames::mouseover) ||
         HasEventListeners(EventTypeNames::mouseout);
}

bool Node::WillRespondToMouseClickEvents() {
  if (IsDisabledFormControl(this))
    return false;
  GetDocument().UpdateStyleAndLayoutTree();
  return HasEditableStyle(*this) ||
         HasEventListeners(EventTypeNames::mouseup) ||
         HasEventListeners(EventTypeNames::mousedown) ||
         HasEventListeners(EventTypeNames::click) ||
         HasEventListeners(EventTypeNames::DOMActivate);
}

bool Node::WillRespondToTouchEvents() {
  if (IsDisabledFormControl(this))
    return false;
  return HasEventListeners(EventTypeNames::touchstart) ||
         HasEventListeners(EventTypeNames::touchmove) ||
         HasEventListeners(EventTypeNames::touchcancel) ||
         HasEventListeners(EventTypeNames::touchend);
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
  // assignedSlot doesn't need to call updateDistribution().
  DCHECK(!IsPseudoElement());
  if (ShadowRoot* root = V1ShadowRootOfParent())
    return root->AssignedSlotFor(*this);
  return nullptr;
}

HTMLSlotElement* Node::FinalDestinationSlot() const {
  HTMLSlotElement* slot = AssignedSlot();
  if (!slot)
    return nullptr;
  for (HTMLSlotElement* next = slot->AssignedSlot(); next;
       next = next->AssignedSlot()) {
    slot = next;
  }
  return slot;
}

HTMLSlotElement* Node::assignedSlotForBinding() {
  // assignedSlot doesn't need to call updateDistribution().
  if (ShadowRoot* root = V1ShadowRootOfParent()) {
    if (root->GetType() == ShadowRootType::kOpen)
      return root->AssignedSlotFor(*this);
  }
  return nullptr;
}

void Node::SetFocused(bool flag, WebFocusType focus_type) {
  if (!flag || focus_type == kWebFocusTypeMouse)
    GetDocument().SetHadKeyboardEvent(false);
  GetDocument().UserActionElements().SetFocused(this, flag);
}

void Node::SetHasFocusWithin(bool flag) {
  GetDocument().UserActionElements().SetHasFocusWithin(this, flag);
}

void Node::SetActive(bool flag) {
  GetDocument().UserActionElements().SetActive(this, flag);
}

void Node::SetDragged(bool flag) {
  GetDocument().UserActionElements().SetDragged(this, flag);
}

void Node::SetHovered(bool flag) {
  GetDocument().UserActionElements().SetHovered(this, flag);
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
      DCHECK_EQ(CustomElementState::kUndefined, old_state);
      break;

    case CustomElementState::kFailed:
      DCHECK_NE(CustomElementState::kFailed, old_state);
      break;
  }

  DCHECK(IsHTMLElement());
  DCHECK_NE(kV0Upgraded, GetV0CustomElementState());

  Element* element = ToElement(this);
  bool was_defined = element->IsDefined();

  node_flags_ = (node_flags_ & ~kCustomElementStateMask) |
                static_cast<NodeFlags>(new_state);
  DCHECK(new_state == GetCustomElementState());

  if (element->IsDefined() != was_defined) {
    element->PseudoStateChanged(CSSSelector::kPseudoDefined);
    element->PseudoStateChanged(CSSSelector::kPseudoUnresolved);
  }
}

void Node::SetV0CustomElementState(V0CustomElementState new_state) {
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
    ToElement(this)->PseudoStateChanged(CSSSelector::kPseudoUnresolved);
    ToElement(this)->PseudoStateChanged(CSSSelector::kPseudoDefined);
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
    if (auto* parent_slot = ToHTMLSlotElementOrNull(parentElement())) {
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

WebPluginContainerImpl* Node::GetWebPluginContainer() const {
  if (!IsHTMLObjectElement(this) && !IsHTMLEmbedElement(this)) {
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

void Node::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_or_shadow_host_node_);
  visitor->Trace(previous_);
  visitor->Trace(next_);
  // rareData() and data_.node_layout_data_ share their storage. We have to
  // trace only one of them.
  if (HasRareData())
    visitor->TraceWithWrappers(RareData());
  visitor->TraceWithWrappers(GetEventTargetData());
  visitor->Trace(tree_scope_);
  EventTarget::Trace(visitor);
}

}  // namespace blink

#ifndef NDEBUG

void showNode(const blink::Node* node) {
  if (node)
    LOG(INFO) << *node;
  else
    LOG(INFO) << "Cannot showNode for <null>";
}

void showTree(const blink::Node* node) {
  if (node)
    LOG(INFO) << "\n" << node->ToTreeStringForThis().Utf8().data();
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
