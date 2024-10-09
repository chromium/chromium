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
#include "third_party/blink/renderer/bindings/core/v8/v8_get_root_node_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_node_string_trustedscript.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedscript.h"
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
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/template_content_document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/text_visitor.h"
#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"
#include "third_party/blink/renderer/core/dom/user_action_element_set.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/event_util.h"
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
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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

using ReattachHookScope = LayoutShiftTracker::ReattachHookScope;

// We want to keep Node small.  This struct + assert calls our attention to a
// change that might be undesirable, so that we make sure to consider whether
// it's worthwhile.
struct SameSizeAsNode : EventTarget {
  uint32_t node_flags_;
  subtle::UncompressedMember<int> uncompressed[2];
  Member<void*> members[4];
};

ASSERT_SIZE(Node, SameSizeAsNode);

// Right now we have the member variables of Node ordered so as to
// reduce padding.  If the object layout of its base class changes, this
// ordering might stop being optimal.  This struct + assert are intended
// to catch if that happens, so that we can reorder the members again.
struct NotSmallerThanNode : EventTarget {
  subtle::UncompressedMember<int> uncompressed[2];
  Member<void*> members[4];
  uint32_t node_flags_;
};

static_assert(sizeof(Node) <= sizeof(NotSmallerThanNode),
              "members of node should be reordered for better packing");

#if DUMP_NODE_STATISTICS
using WeakNodeSet = HeapHashSet<WeakMember<Node>>;
static WeakNodeSet& LiveNodeSet() {
  DEFINE_STATIC_LOCAL(Persistent<WeakNodeSet>, set,
                      (MakeGarbageCollected<WeakNodeSet>()));
  return *set;
}

void Node::DumpStatistics() {
  size_t nodes_with_rare_data = 0;

  size_t element_nodes = 0;
  size_t attr_nodes = 0;
  size_t text_nodes = 0;
  size_t cdata_nodes = 0;
  size_t comment_nodes = 0;
  size_t pi_nodes = 0;
  size_t document_nodes = 0;
  size_t doc_type_nodes = 0;
  size_t fragment_nodes = 0;
  size_t shadow_root_nodes = 0;

  HashMap<String, size_t> per_tag_count;

  size_t attributes = 0;
  size_t elements_with_attribute_storage = 0;
  size_t elements_with_rare_data = 0;
  size_t elements_with_named_node_map = 0;

  {
    ScriptForbiddenScope forbid_script_during_raw_iteration;
    for (Node* node : LiveNodeSet()) {
      if (node->data_) {
        ++nodes_with_rare_data;
        if (auto* element = DynamicTo<Element>(node)) {
          ++elements_with_rare_data;
          if (element->HasNamedNodeMap())
            ++elements_with_named_node_map;
        }
      }

      switch (node->getNodeType()) {
        case kElementNode: {
          ++element_nodes;

          // Tag stats
          auto* element = To<Element>(node);
          auto result = per_tag_count.insert(element->tagName(), 1);
          if (!result.is_new_entry)
            result.stored_value->value++;

          size_t attributeCount = element->AttributesWithoutUpdate().size();
          if (attributeCount) {
            attributes += attributeCount;
            ++elements_with_attribute_storage;
          }
          break;
        }
        case kAttributeNode: {
          ++attr_nodes;
          break;
        }
        case kTextNode: {
          ++text_nodes;
          break;
        }
        case kCdataSectionNode: {
          ++cdata_nodes;
          break;
        }
        case kCommentNode: {
          ++comment_nodes;
          break;
        }
        case kProcessingInstructionNode: {
          ++pi_nodes;
          break;
        }
        case kDocumentNode: {
          ++document_nodes;
          break;
        }
        case kDocumentTypeNode: {
          ++doc_type_nodes;
          break;
        }
        case kDocumentFragmentNode: {
          if (node->IsShadowRoot())
            ++shadow_root_nodes;
          else
            ++fragment_nodes;
          break;
        }
      }
    }
  }

  std::stringstream per_tag_stream;
  for (const auto& entry : per_tag_count) {
    per_tag_stream << "  Number of <" << entry.key.Utf8().data()
                   << "> tags: " << entry.value << "\n";
  }

  LOG(INFO) << "\n"
            << "Number of Nodes: " << LiveNodeSet().size() << "\n"
            << "Number of Nodes with RareData: " << nodes_with_rare_data
            << "\n\n"

            << "NodeType distribution:\n"
            << "  Number of Element nodes: " << element_nodes << "\n"
            << "  Number of Attribute nodes: " << attr_nodes << "\n"
            << "  Number of Text nodes: " << text_nodes << "\n"
            << "  Number of CDATASection nodes: " << cdata_nodes << "\n"
            << "  Number of Comment nodes: " << comment_nodes << "\n"
            << "  Number of ProcessingInstruction nodes: " << pi_nodes << "\n"
            << "  Number of Document nodes: " << document_nodes << "\n"
            << "  Number of DocumentType nodes: " << doc_type_nodes << "\n"
            << "  Number of DocumentFragment nodes: " << fragment_nodes << "\n"
            << "  Number of ShadowRoot nodes: " << shadow_root_nodes << "\n"

            << "Element tag name distribution:\n"
            << per_tag_stream.str()

            << "Attributes:\n"
            << "  Number of Attributes (non-Node and Node): " << attributes
            << " x " << sizeof(Attribute) << "Bytes\n"
            << "  Number of Elements with attribute storage: "
            << elements_with_attribute_storage << " x " << sizeof(ElementData)
            << "Bytes\n"
            << "  Number of Elements with RareData: " << elements_with_rare_data
            << " x " << sizeof(ElementRareData) << "Bytes\n"
            << "  Number of Elements with NamedNodeMap: "
            << elements_with_named_node_map << " x " << sizeof(NamedNodeMap)
            << "Bytes";
}
#endif

Node::Node(TreeScope* tree_scope, ConstructionType type)
    : node_flags_(type),
      parent_or_shadow_host_node_(nullptr),
      tree_scope_(tree_scope),
      previous_(nullptr),
      next_(nullptr),
      layout_object_(nullptr),
      data_(nullptr) {
  DCHECK(tree_scope_ || type == kCreateDocument || type == kCreateShadowRoot);
#if DUMP_NODE_STATISTICS
  LiveNodeSet().insert(this);
#endif
  InstanceCounters::IncrementCounter(InstanceCounters::kNodeCounter);
  // Document is required for probe sink.
  if (tree_scope_)
    probe::NodeCreated(this);
}

Node::~Node() {
  InstanceCounters::DecrementCounter(InstanceCounters::kNodeCounter);
}

DOMNodeId Node::GetDomNodeId() {
  return DOMNodeIds::IdForNode(this);
}

// static
Node* Node::FromDomNodeId(DOMNodeId dom_node_id) {
  return DOMNodeIds::NodeForId(dom_node_id);
}

NodeRareData& Node::CreateRareData() {
  if (IsElementNode()) {
    data_ = MakeGarbageCollected<ElementRareDataVector>();
  } else {
    data_ = MakeGarbageCollected<NodeRareData>();
  }
  return *data_;
}

Node* Node::ToNode() {
  return this;
}

String Node::nodeValue() const {
  return String();
}

void Node::setNodeValue(const String&, ExceptionState&) {
  // By default, setting nodeValue has no effect.
}

NodeList* Node::childNodes() {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (this_node)
    return EnsureRareData().EnsureNodeLists().EnsureChildNodeList(*this_node);
  return EnsureRareData().EnsureNodeLists().EnsureEmptyChildNodeList(*this);
}

Node* Node::PseudoAwarePreviousSibling() const {
  Element* parent = parentElement();
  if (!parent || HasPreviousSibling()) {
    return previousSibling();
  }

  // Note the [[fallthrough]] attributes, the order of the cases matters and
  // corresponds to the ordering of pseudo elements in a traversal:
  // ::scroll-marker-group(before), ::marker, ::scroll-marker, ::before,
  // non-pseudo Elements,
  // ::after, ::scroll-marker-group(after), ::view-transition. The fallthroughs
  // ensure this ordering by checking for each kind of node in-turn.
  switch (GetPseudoId()) {
    case kPseudoIdViewTransition:
      if (Node* previous =
              parent->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollNextButton:
      if (Node* next =
              parent->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupAfter:
      if (Node* next = parent->GetPseudoElement(kPseudoIdAfter)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdAfter:
      if (Node* previous = parent->lastChild())
        return previous;
      [[fallthrough]];
    case kPseudoIdNone:
      if (Node* previous = parent->GetPseudoElement(kPseudoIdBefore))
        return previous;
      [[fallthrough]];
    case kPseudoIdBefore:
      if (Node* previous = parent->GetPseudoElement(kPseudoIdScrollMarker)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarker:
      if (Node* previous = parent->GetPseudoElement(kPseudoIdMarker)) {
        return previous;
      }
      [[fallthrough]];
    case kPseudoIdMarker:
      if (Node* next =
              parent->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupBefore:
      if (Node* next = parent->GetPseudoElement(kPseudoIdScrollPrevButton)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollPrevButton:
      return nullptr;
    // The pseudos of the view transition subtree have a known structure and
    // cannot create other pseudos so these are handled separately of the above
    // fallthrough cases. For details on view-transition pseudo ordering, see
    // https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/view_transition/README.md#pseudo-element-traversal
    case kPseudoIdViewTransitionNew:
      CHECK_EQ(parent->GetPseudoId(), kPseudoIdViewTransitionImagePair);
      return parent->GetPseudoElement(
          kPseudoIdViewTransitionOld,
          To<PseudoElement>(this)->view_transition_name());
    case kPseudoIdViewTransitionGroup: {
      const Vector<AtomicString>& names =
          GetDocument().GetStyleEngine().ViewTransitionTags();
      wtf_size_t found_index =
          names.Find(To<PseudoElement>(this)->view_transition_name());
      CHECK_NE(found_index, kNotFound);
      if (found_index == 0) {
        return nullptr;
      }

      CHECK_EQ(parent->GetPseudoId(), kPseudoIdViewTransition);
      return parent->GetPseudoElement(kPseudoIdViewTransitionGroup,
                                      names[found_index - 1]);
    }
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionOld:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

Node* Node::PseudoAwareNextSibling() const {
  Element* parent = parentElement();
  if (!parent || HasNextSibling()) {
    return nextSibling();
  }

  // See comments in PseudoAwarePreviousSibling.
  switch (GetPseudoId()) {
    case kPseudoIdScrollPrevButton:
      if (Node* next =
              parent->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupBefore:
      if (Node* next = parent->GetPseudoElement(kPseudoIdMarker)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdMarker:
      if (Node* next = parent->GetPseudoElement(kPseudoIdScrollMarker)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarker:
      if (Node* next = parent->GetPseudoElement(kPseudoIdBefore))
        return next;
      [[fallthrough]];
    case kPseudoIdBefore:
      if (parent->HasChildren())
        return parent->firstChild();
      [[fallthrough]];
    case kPseudoIdNone:
      if (Node* next = parent->GetPseudoElement(kPseudoIdAfter))
        return next;
      [[fallthrough]];
    case kPseudoIdAfter:
      if (Node* next =
              parent->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollMarkerGroupAfter:
      if (Node* next = parent->GetPseudoElement(kPseudoIdScrollNextButton)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdScrollNextButton:
      if (Node* next = parent->GetPseudoElement(kPseudoIdViewTransition)) {
        return next;
      }
      [[fallthrough]];
    case kPseudoIdViewTransition:
      return nullptr;
    case kPseudoIdViewTransitionOld:
      CHECK_EQ(parent->GetPseudoId(), kPseudoIdViewTransitionImagePair);
      return parent->GetPseudoElement(
          kPseudoIdViewTransitionNew,
          To<PseudoElement>(this)->view_transition_name());
    case kPseudoIdViewTransitionGroup: {
      const Vector<AtomicString>& names =
          GetDocument().GetStyleEngine().ViewTransitionTags();
      wtf_size_t found_index =
          names.Find(To<PseudoElement>(this)->view_transition_name());
      CHECK_NE(found_index, kNotFound);
      if (found_index == names.size() - 1) {
        return nullptr;
      }

      CHECK_EQ(parent->GetPseudoId(), kPseudoIdViewTransition);
      return parent->GetPseudoElement(kPseudoIdViewTransitionGroup,
                                      names[found_index + 1]);
    }
    case kPseudoIdViewTransitionImagePair:
    case kPseudoIdViewTransitionNew:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

Node* Node::PseudoAwareFirstChild() const {
  if (const auto* current_element = DynamicTo<Element>(this)) {
    // See comments in PseudoAwarePreviousSibling for details on view-transition
    // pseudo traversal.
    if (GetPseudoId() == kPseudoIdViewTransition) {
      const Vector<AtomicString>& names =
          GetDocument().GetStyleEngine().ViewTransitionTags();
      if (names.empty()) {
        return nullptr;
      }
      return current_element->GetPseudoElement(kPseudoIdViewTransitionGroup,
                                               names.front());
    }
    if (GetPseudoId() == kPseudoIdViewTransitionGroup) {
      return current_element->GetPseudoElement(
          kPseudoIdViewTransitionImagePair,
          To<PseudoElement>(this)->view_transition_name());
    }
    if (GetPseudoId() == kPseudoIdViewTransitionImagePair) {
      const AtomicString& name =
          To<PseudoElement>(this)->view_transition_name();
      if (Node* first = current_element->GetPseudoElement(
              kPseudoIdViewTransitionOld, name)) {
        return first;
      }

      return current_element->GetPseudoElement(kPseudoIdViewTransitionNew,
                                               name);
    }
    if (Node* first =
            current_element->GetPseudoElement(kPseudoIdScrollPrevButton)) {
      return first;
    }
    if (Node* first = current_element->GetPseudoElement(
            kPseudoIdScrollMarkerGroupBefore)) {
      return first;
    }
    if (Node* first = current_element->GetPseudoElement(kPseudoIdMarker))
      return first;
    if (Node* first =
            current_element->GetPseudoElement(kPseudoIdScrollMarker)) {
      return first;
    }
    if (Node* first = current_element->GetPseudoElement(kPseudoIdBefore))
      return first;
    if (Node* first = current_element->firstChild())
      return first;
    if (Node* first = current_element->GetPseudoElement(kPseudoIdAfter)) {
      return first;
    }
    if (Node* first = current_element->GetPseudoElement(
            kPseudoIdScrollMarkerGroupAfter)) {
      return first;
    }
    if (Node* first =
            current_element->GetPseudoElement(kPseudoIdScrollNextButton)) {
      return first;
    }
    return current_element->GetPseudoElement(kPseudoIdViewTransition);
  }

  return firstChild();
}

Node* Node::PseudoAwareLastChild() const {
  if (const auto* current_element = DynamicTo<Element>(this)) {
    // See comments in PseudoAwarePreviousSibling for details on view-transition
    // pseudo traversal.
    if (GetPseudoId() == kPseudoIdViewTransition) {
      const Vector<AtomicString>& names =
          GetDocument().GetStyleEngine().ViewTransitionTags();
      if (names.empty()) {
        return nullptr;
      }
      return current_element->GetPseudoElement(kPseudoIdViewTransitionGroup,
                                               names.back());
    }
    if (GetPseudoId() == kPseudoIdViewTransitionGroup) {
      return current_element->GetPseudoElement(
          kPseudoIdViewTransitionImagePair,
          To<PseudoElement>(this)->view_transition_name());
    }
    if (GetPseudoId() == kPseudoIdViewTransitionImagePair) {
      const AtomicString& name =
          To<PseudoElement>(this)->view_transition_name();
      if (Node* last = current_element->GetPseudoElement(
              kPseudoIdViewTransitionNew, name)) {
        return last;
      }

      return current_element->GetPseudoElement(kPseudoIdViewTransitionOld,
                                               name);
    }
    if (Node* last =
            current_element->GetPseudoElement(kPseudoIdViewTransition)) {
      return last;
    }
    if (Node* last =
            current_element->GetPseudoElement(kPseudoIdScrollNextButton)) {
      return last;
    }
    if (Node* last = current_element->GetPseudoElement(
            kPseudoIdScrollMarkerGroupAfter)) {
      return last;
    }
    if (Node* last = current_element->GetPseudoElement(kPseudoIdAfter))
      return last;
    if (Node* last = current_element->lastChild())
      return last;
    if (Node* last = current_element->GetPseudoElement(kPseudoIdBefore))
      return last;
    if (Node* last = current_element->GetPseudoElement(kPseudoIdScrollMarker)) {
      return last;
    }
    if (Node* last = current_element->GetPseudoElement(kPseudoIdMarker)) {
      return last;
    }
    if (Node* last = current_element->GetPseudoElement(
            kPseudoIdScrollMarkerGroupBefore)) {
      return last;
    }
    return current_element->GetPseudoElement(kPseudoIdScrollPrevButton);
  }

  return lastChild();
}

Node& Node::TreeRoot() const {
  if (IsInTreeScope()) {
    return GetTreeScope().RootNode();
  }
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

Node* Node::moveBefore(Node* new_child,
                       Node* ref_child,
                       ExceptionState& exception_state) {
  DCHECK(new_child);

  // Only perform a state-preserving atomic move if the child is ALREADY
  // connected to this document, and its document is the same as `this`'s. If
  // the child is NOT connected to this document, then script can run during the
  // node's initial post-insertion steps (i.e.,
  // `Node::DidNotifySubtreeInsertionsToDocument()`), and no script is permitted
  // to run during atomic moves.
  const bool perform_state_preserving_atomic_move =
      isConnected() && new_child->isConnected() && GetDocument().IsActive() &&
      (GetDocument() == new_child->GetDocument()) && new_child->IsElementNode();

  if (perform_state_preserving_atomic_move) {
    // When `moveBefore()` is called, AND we're actually performing a
    // state-preserving atomic move, no script can run synchronously during the
    // move. That means it is impossible for nested `moveBefore()` calls to
    // occur. Assert that no atomic move is already in progress.
    DCHECK(!GetDocument().StatePreservingAtomicMoveInProgress());
    GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  }

  // Mutation events are disabled during the `moveBefore()` API.
  MutationEventSuppressionScope scope(GetDocument());

  Node* return_node = insertBefore(new_child, ref_child, exception_state);

  // Regardless of whether we were *actually* performing a state-preserving
  // atomic move, we can safely, unconditionally reset the document's boolean.
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);

  // We don't need to conditionally return `nullptr` if `exception_state` had an
  // exception. `insertBefore()` already handles this for us, so we can just
  // unconditionally return its value.
  return return_node;
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
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes) {
  for (const V8UnionNodeOrStringOrTrustedScript* node_or_string : nodes) {
    if (node_or_string->IsNode() && node_or_string->GetAsNode() == node)
      return true;
  }
  return false;
}

static Node* FindViablePreviousSibling(
    const Node& node,
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes) {
  for (Node* sibling = node.previousSibling(); sibling;
       sibling = sibling->previousSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* FindViableNextSibling(
    const Node& node,
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes) {
  for (Node* sibling = node.nextSibling(); sibling;
       sibling = sibling->nextSibling()) {
    if (!IsNodeInNodes(sibling, nodes))
      return sibling;
  }
  return nullptr;
}

static Node* NodeOrStringToNode(
    const V8UnionNodeOrStringOrTrustedScript* node_or_string,
    Document& document,
    bool needs_trusted_types_check,
    const char* property_name,
    ExceptionState& exception_state) {
  if (!needs_trusted_types_check) {
    // Without trusted type checks, we simply extract the string from whatever
    // constituent type we find.
    switch (node_or_string->GetContentType()) {
      case V8UnionNodeOrStringOrTrustedScript::ContentType::kNode:
        return node_or_string->GetAsNode();
      case V8UnionNodeOrStringOrTrustedScript::ContentType::kString:
        return Text::Create(document, node_or_string->GetAsString());
      case V8UnionNodeOrStringOrTrustedScript::ContentType::kTrustedScript:
        return Text::Create(document,
                            node_or_string->GetAsTrustedScript()->toString());
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  // With trusted type checks, we can process trusted script or non-text nodes
  // directly. Strings or text nodes need to be checked.
  if (node_or_string->IsNode() && !node_or_string->GetAsNode()->IsTextNode())
    return node_or_string->GetAsNode();

  if (node_or_string->IsTrustedScript()) {
    return Text::Create(document,
                        node_or_string->GetAsTrustedScript()->toString());
  }

  String string_value = node_or_string->IsString()
                            ? node_or_string->GetAsString()
                            : node_or_string->GetAsNode()->textContent();

  string_value =
      TrustedTypesCheckForScript(string_value, document.GetExecutionContext(),
                                 "Node", property_name, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return Text::Create(document, string_value);
}

// static
// Returns nullptr if an exception was thrown.
Node* Node::ConvertNodesIntoNode(const Node* parent,
                                 const VectorOf<Node>& nodes,
                                 Document& document,
                                 ExceptionState& exception_state) {
  if (nodes.size() == 1) {
    return nodes[0].Get();
  }

  Node* fragment = DocumentFragment::Create(document);
  for (const auto& node : nodes) {
    fragment->appendChild(node, exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
  }
  return fragment;
}

namespace {

// Converts |node_unions| from bindings into actual Nodes by converting strings
// and script into text nodes via NodeOrStringToNode.
// Returns nullptr if an exception was thrown.
VectorOf<Node> ConvertNodeUnionsIntoNodes(
    const Node* parent,
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& node_unions,
    Document& document,
    const char* property_name,
    ExceptionState& exception_state) {
  bool needs_check = IsA<HTMLScriptElement>(parent) &&
                     document.GetExecutionContext() &&
                     document.GetExecutionContext()->RequireTrustedTypes();
  VectorOf<Node> nodes;
  for (const auto& node_union : node_unions) {
    Node* node = NodeOrStringToNode(node_union, document, needs_check,
                                    property_name, exception_state);
    if (exception_state.HadException()) {
      nodes.clear();
      return nodes;
    }
    if (node) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

}  // namespace

// static
// Returns nullptr if an exception was thrown.
Node* Node::ConvertNodeUnionsIntoNode(
    const Node* parent,
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& node_unions,
    Document& document,
    const char* property_name,
    ExceptionState& exception_state) {
  VectorOf<Node> nodes = ConvertNodeUnionsIntoNodes(
      parent, node_unions, document, property_name, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return Node::ConvertNodesIntoNode(parent, nodes, document, exception_state);
}

void Node::prepend(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  if (Node* node = ConvertNodeUnionsIntoNode(this, nodes, GetDocument(),
                                             "prepend", exception_state)) {
    this_node->InsertBefore(node, this_node->firstChild(), exception_state);
  }
}

void Node::append(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  if (Node* node = ConvertNodeUnionsIntoNode(this, nodes, GetDocument(),
                                             "append", exception_state)) {
    this_node->AppendChild(node, exception_state);
  }
}

void Node::before(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  ContainerNode* parent = parentNode();
  if (!parent)
    return;
  Node* viable_previous_sibling = FindViablePreviousSibling(*this, nodes);
  if (Node* node = ConvertNodeUnionsIntoNode(parent, nodes, GetDocument(),
                                             "before", exception_state)) {
    parent->InsertBefore(node,
                         viable_previous_sibling
                             ? viable_previous_sibling->nextSibling()
                             : parent->firstChild(),
                         exception_state);
  }
}

void Node::after(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  ContainerNode* parent = parentNode();
  if (!parent)
    return;
  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  if (Node* node = ConvertNodeUnionsIntoNode(parent, nodes, GetDocument(),
                                             "after", exception_state)) {
    parent->InsertBefore(node, viable_next_sibling, exception_state);
  }
}

void Node::replaceWith(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  ContainerNode* parent = parentNode();
  if (!parent)
    return;
  Node* viable_next_sibling = FindViableNextSibling(*this, nodes);
  Node* node = ConvertNodeUnionsIntoNode(parent, nodes, GetDocument(),
                                         "replaceWith", exception_state);
  if (exception_state.HadException())
    return;
  if (parent == parentNode())
    parent->ReplaceChild(node, this, exception_state);
  else
    parent->InsertBefore(node, viable_next_sibling, exception_state);
}

// https://dom.spec.whatwg.org/#dom-parentnode-replacechildren
void Node::replaceChildren(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& node_unions,
    ExceptionState& exception_state) {
  auto* this_node = DynamicTo<ContainerNode>(this);
  if (!this_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "This node type does not support this method.");
    return;
  }

  VectorOf<Node> nodes = ConvertNodeUnionsIntoNodes(
      this, node_unions, GetDocument(), "replace", exception_state);
  if (!exception_state.HadException()) {
    this_node->ReplaceChildren(nodes, exception_state);
  }
}

void Node::remove(ExceptionState& exception_state) {
  if (ContainerNode* parent = parentNode())
    parent->RemoveChild(this, exception_state);
}

void Node::remove() {
  remove(ASSERT_NO_EXCEPTION);
}

Element* Node::previousElementSibling() {
  return ElementTraversal::PreviousSibling(*this);
}

Element* Node::nextElementSibling() {
  return ElementTraversal::NextSibling(*this);
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
  NodeCloningData data;
  if (deep) {
    data.Put(CloneOption::kIncludeDescendants);
  }
  return Clone(GetDocument(), data, /*append_to*/ nullptr);
}

Node* Node::cloneNode(bool deep) const {
  return cloneNode(deep, ASSERT_NO_EXCEPTION);
}

void Node::normalize() {
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
  return DynamicTo<LayoutBox>(GetLayoutObject());
}

LayoutBoxModelObject* Node::GetLayoutBoxModelObject() const {
  return DynamicTo<LayoutBoxModelObject>(GetLayoutObject());
}

PhysicalRect Node::BoundingBox() const {
  if (GetLayoutObject())
    return PhysicalRect(GetLayoutObject()->AbsoluteBoundingBoxRect());
  return PhysicalRect();
}

gfx::Rect Node::PixelSnappedBoundingBox() const {
  return ToPixelSnappedRect(BoundingBox());
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
    if (shadow_root && !shadow_root->IsOpen())
      break;
  }

  for (TreeScope* other_scope = &other.GetTreeScope(); other_scope;
       other_scope = other_scope->ParentTreeScope()) {
    if (other_scope == scope)
      return false;
  }
  return true;
}

void Node::SetIsLink(bool is_link) {
  SetFlag(is_link && !SVGImage::IsInSVGImage(To<Element>(this)), kIsLinkFlag);
}

void Node::SetNeedsStyleInvalidation() {
  DCHECK(IsContainerNode());
  DCHECK(!GetDocument().InvalidationDisallowed());
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

void Node::MarkSubtreeNeedsStyleRecalcForFontUpdates() {
  if (GetStyleChangeType() == kSubtreeStyleChange)
    return;

  if (auto* element = DynamicTo<Element>(this)) {
    const ComputedStyle* style = element->GetComputedStyle();
    if (!style)
      return;

    // We require font-specific metrics to resolve length units 'ex' and 'ch',
    // and to compute the adjusted font size when 'font-size-adjust' is set. All
    // other style computations are unaffected by font loading.
    if (!NeedsStyleRecalc()) {
      if (style->DependsOnFontMetrics() ||
          element->PseudoElementStylesDependOnFontMetrics()) {
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

bool Node::ShouldSkipMarkingStyleDirty() const {
  // If our parent element does not have a computed style, it's not necessary to
  // mark this node for style recalc.
  if (Element* parent = GetStyleRecalcParent()) {
    return !parent->GetComputedStyle();
  }
  if (const Element* element = DynamicTo<Element>(this)) {
    const Element* root_element = GetDocument().documentElement();
    if (!root_element || element == root_element) {
      // This is the root element, or we are about to insert the root element.
      // Should always allow marking it dirty.
      return false;
    }
    // This is an element outside the flat tree without a parent. Should only
    // mark dirty if it has an ensured style.
    return !element->GetComputedStyle();
  }
  // Text nodes outside the flat tree do not need to be marked for style recalc.
  return true;
}

void Node::MarkAncestorsWithChildNeedsStyleRecalc() {
  Element* style_parent = GetStyleRecalcParent();
  bool parent_dirty = style_parent && style_parent->IsDirtyForStyleRecalc();
  Element* ancestor = style_parent;
  for (; ancestor && !ancestor->ChildNeedsStyleRecalc();
       ancestor = ancestor->GetStyleRecalcParent()) {
    if (!ancestor->isConnected())
      return;
    ancestor->SetChildNeedsStyleRecalc();
    if (ancestor->IsDirtyForStyleRecalc())
      break;

    // If we reach a locked ancestor, we should abort since the ancestor marking
    // will be done when the lock is committed.
    if (ancestor->ChildStyleRecalcBlockedByDisplayLock())
      break;
  }
  if (!isConnected())
    return;
  // If the parent node is already dirty, we can keep the same recalc root. The
  // early return here is a performance optimization.
  if (parent_dirty)
    return;
  // If we are outside the flat tree we should not update the recalc root
  // because we should not traverse those nodes from StyleEngine::RecalcStyle().
  const ComputedStyle* current_style = nullptr;
  if (Element* element = DynamicTo<Element>(this)) {
    current_style = element->GetComputedStyle();
  }
  if (!current_style && style_parent) {
    current_style = style_parent->GetComputedStyle();
  }
  if (current_style && current_style->IsEnsuredOutsideFlatTree()) {
    return;
  }
  // If we're in a locked subtree, then we should not update the style recalc
  // roots. These would be updated when we commit the lock. If we have locked
  // display locks somewhere in the document, we iterate up the ancestor chain
  // to check if we're in one such subtree.
  if (GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() >
      0) {
    for (Element* ancestor_copy = ancestor; ancestor_copy;
         ancestor_copy = ancestor_copy->GetStyleRecalcParent()) {
      if (ancestor_copy->ChildStyleRecalcBlockedByDisplayLock())
        return;
    }
  }

  GetDocument().GetStyleEngine().UpdateStyleRecalcRoot(ancestor, this);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

Element* Node::FlatTreeParentForChildDirty() const {
  if (IsPseudoElement())
    return ParentOrShadowHostElement();
  if (IsChildOfShadowHost()) {
    if (auto* data = GetFlatTreeNodeData())
      return data->AssignedSlot();
    return nullptr;
  }
  Element* parent = ParentOrShadowHostElement();
  if (HTMLSlotElement* slot = DynamicTo<HTMLSlotElement>(parent)) {
    if (slot->HasAssignedNodesNoRecalc())
      return nullptr;
  }
  return parent;
}

void Node::MarkAncestorsWithChildNeedsReattachLayoutTree() {
  DCHECK(isConnected());
  Element* ancestor = GetReattachParent();
  bool parent_dirty = ancestor && ancestor->IsDirtyForRebuildLayoutTree();
  DCHECK(!ancestor || !ChildNeedsReattachLayoutTree() ||
         !ancestor->ChildNeedsReattachLayoutTree() || NeedsReattachLayoutTree())
      << "If both this and the parent are already marked with "
         "ChildNeedsReattachLayoutTree(), something is broken and "
         "UpdateLayoutTreeRebuildRoot() will be confused about common "
         "ancestors.";
  for (; ancestor && !ancestor->ChildNeedsReattachLayoutTree();
       ancestor = ancestor->GetReattachParent()) {
    ancestor->SetChildNeedsReattachLayoutTree();
    if (ancestor->IsDirtyForRebuildLayoutTree())
      break;

    // If we reach a locked ancestor, we should abort since the ancestor marking
    // will be done when the context is unlocked.
    if (ancestor->ChildStyleRecalcBlockedByDisplayLock())
      break;
  }
  // If the parent node is already dirty, we can keep the same rebuild root. The
  // early return here is a performance optimization.
  if (parent_dirty)
    return;

  // If we're in a locked subtree, then we should not update the layout tree
  // rebuild root. It would be updated when we unlock the context. In other
  // words, the only way we have a node in the locked subtree is if the ancestor
  // has a locked display lock context or it is dirty for reattach. In either of
  // those cases, we have a dirty bit trail up to the display lock context,
  // which will be propagated when the lock is removed.
  if (GetDocument().GetDisplayLockDocumentState().LockedDisplayLockCount() >
      0) {
    for (Element* ancestor_copy = ancestor; ancestor_copy;
         ancestor_copy = ancestor_copy->GetReattachParent()) {
      if (ancestor_copy->ChildStyleRecalcBlockedByDisplayLock())
        return;
    }
  }
  GetDocument().GetStyleEngine().UpdateLayoutTreeRebuildRoot(ancestor, this);
}

void Node::SetNeedsReattachLayoutTree() {
  DCHECK(GetDocument().InStyleRecalc());
  DCHECK(GetDocument().GetStyleEngine().MarkReattachAllowed());
  DCHECK(!GetDocument().InvalidationDisallowed());
  DCHECK(IsElementNode() || IsTextNode());
  DCHECK(InActiveDocument());
  SetFlag(kNeedsReattachLayoutTree);
  MarkAncestorsWithChildNeedsReattachLayoutTree();
}

void Node::SetNeedsStyleRecalc(StyleChangeType change_type,
                               const StyleChangeReasonForTracing& reason) {
  DCHECK(GetDocument().GetStyleEngine().MarkStyleDirtyAllowed());
  DCHECK(!GetDocument().InvalidationDisallowed());
  DCHECK(change_type != kNoStyleChange);
  DCHECK(IsElementNode() || IsTextNode());

  if (!InActiveDocument())
    return;
  if (ShouldSkipMarkingStyleDirty())
    return;

  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
      "StyleRecalcInvalidationTracking",
      inspector_style_recalc_invalidation_tracking_event::Data, this,
      change_type, reason);

  StyleChangeType existing_change_type = GetStyleChangeType();
  if (change_type > existing_change_type)
    SetStyleChange(change_type);

  if (existing_change_type == kNoStyleChange)
    MarkAncestorsWithChildNeedsStyleRecalc();

  // NOTE: If we are being called from SetNeedsAnimationStyleRecalc(), the
  // AnimationStyleChange bit may be reset to 'true'.
  if (auto* this_element = DynamicTo<Element>(this)) {
    this_element->SetAnimationStyleChange(false);

    // The style walk for the pseudo tree created for a ViewTransition is
    // done after resolving style for the author DOM. See
    // StyleEngine::RecalcTransitionPseudoStyle.
    // Since the dirty bits from the originating element (root element) are not
    // propagated to these pseudo elements during the default walk, we need to
    // invalidate style for these elements here.
    if (this_element->IsDocumentElement()) {
      auto update_style_change = [](PseudoElement* pseudo_element) {
        pseudo_element->SetNeedsStyleRecalc(
            kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                   style_change_reason::kViewTransition));
      };
      ViewTransitionUtils::ForEachTransitionPseudo(GetDocument(),
                                                   update_style_change);
    }
  }

  if (auto* svg_element = DynamicTo<SVGElement>(this))
    svg_element->SetNeedsStyleRecalcForInstances(change_type, reason);
}

void Node::ClearNeedsStyleRecalc() {
  node_flags_ &= ~kStyleChangeMask;
  ClearFlag(kForceReattachLayoutTree);
  if (!data_) {
    return;
  }
  if (auto* element = DynamicTo<Element>(this)) {
    element->SetAnimationStyleChange(false);
  }
}

bool Node::InActiveDocument() const {
  return isConnected() && GetDocument().IsActive();
}

bool Node::ShouldHaveFocusAppearance() const {
  DCHECK(IsFocused());
  return true;
}

void Node::FocusabilityLost() {
  if (IsA<HTMLFormElement>(this) || IsA<HTMLFormControlElement>(this)) {
    GetDocument().DidChangeFormRelatedElementDynamically(
        DynamicTo<HTMLElement>(this), WebFormRelatedChangeType::kHide);
  }
}

LinkHighlightCandidate Node::IsLinkHighlightCandidate() const {
  if (const LayoutObject* layout_object = GetLayoutObject()) {
    const ECursor cursor = layout_object->StyleRef().Cursor();
    if (cursor == ECursor::kPointer)
      return LinkHighlightCandidate::kYes;
    if (cursor != ECursor::kAuto)
      return LinkHighlightCandidate::kNo;
    if (EventHandler::UsesHandCursor(this))
      return LinkHighlightCandidate::kYes;
  }
  return LinkHighlightCandidate::kMayBe;
}

unsigned Node::NodeIndex() const {
  const Node* temp_node = previousSibling();
  unsigned count = 0;
  for (count = 0; temp_node; count++)
    temp_node = temp_node->previousSibling();
  return count;
}

NodeListsNodeData* Node::NodeLists() {
  return data_ ? data_->NodeLists() : nullptr;
}

void Node::ClearNodeLists() {
  RareData()->ClearNodeLists();
}

FlatTreeNodeData& Node::EnsureFlatTreeNodeData() {
  return EnsureRareData().EnsureFlatTreeNodeData();
}

FlatTreeNodeData* Node::GetFlatTreeNodeData() const {
  if (!data_) {
    return nullptr;
  }
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

bool Node::IsDescendantOrShadowDescendantOf(const Node* other) const {
  if (IsDescendantOf(other))
    return true;

  for (auto* host = OwnerShadowHost(); host; host = host->OwnerShadowHost()) {
    if (other->contains(host))
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
  DCHECK(GetDocument().InStyleRecalc() || IsDocumentNode() ||
         GetDocument().GetStyleEngine().InScrollMarkersAttachment());
  DCHECK(!GetDocument().Lifecycle().InDetach());
  DCHECK(!context.performing_reattach ||
         GetDocument().GetStyleEngine().InRebuildLayoutTree());

  LayoutObject* layout_object = GetLayoutObject();
  DCHECK(!layout_object ||
         (layout_object->Style() &&
          (layout_object->Parent() || IsA<LayoutView>(layout_object))));

  ClearNeedsReattachLayoutTree();

  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache()) {
    cache->NodeIsAttached(this);
  }

  if (context.performing_reattach)
    ReattachHookScope::NotifyAttach(*this);
}

void Node::DetachLayoutTree(bool performing_reattach) {
  // Re-attachment is not generally allowed from PositionTryStyleRecalc, but
  // computing style for display:none pseudo elements will insert a pseudo
  // element, compute the style, and remove it again, which includes a
  // DetachLayoutTree().
  DCHECK(GetDocument().Lifecycle().StateAllowsDetach() ||
         GetDocument().GetStyleEngine().InContainerQueryStyleRecalc() ||
         GetDocument().GetStyleEngine().InScrollMarkersAttachment() ||
         (GetDocument().GetStyleEngine().InPositionTryStyleRecalc() &&
          IsPseudoElement() && !GetLayoutObject()));
  DCHECK(!performing_reattach ||
         GetDocument().GetStyleEngine().InRebuildLayoutTree() ||
         GetDocument().GetStyleEngine().InScrollMarkersAttachment());
  DocumentLifecycle::DetachScope will_detach(GetDocument().Lifecycle());

  if (auto* cache = GetDocument().ExistingAXObjectCache()) {
    cache->RemoveSubtree(this);
  }

  if (performing_reattach) {
    ReattachHookScope::NotifyDetach(*this);
  }

  if (GetLayoutObject()) {
    GetLayoutObject()->DestroyAndCleanupAnonymousWrappers(performing_reattach);
  }
  SetLayoutObject(nullptr);
  if (!performing_reattach) {
    // We are clearing the ComputedStyle for elements, which means we should not
    // need to recalc style. Also, this way we can detect if we need to remove
    // this Node as a StyleRecalcRoot if this detach is because the node is
    // removed from the flat tree. That is necessary because we are not allowed
    // to have a style recalc root outside the flat tree when traversing the
    // flat tree for style recalc
    // (see StyleRecalcRoot::FlatTreePositionChanged()).
    ClearNeedsStyleRecalc();
    ClearChildNeedsStyleRecalc();
  }
}

void Node::SetForceReattachLayoutTree() {
  DCHECK(!GetDocument().GetStyleEngine().InRebuildLayoutTree());
  DCHECK(IsElementNode() || IsTextNode());
  if (GetForceReattachLayoutTree())
    return;
  if (!InActiveDocument())
    return;
  if (Element* element = DynamicTo<Element>(this)) {
    if (!element->GetComputedStyle()) {
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

bool Node::NeedsWhitespaceChildrenUpdate() const {
  if (const auto* layout_object = GetLayoutObject())
    return layout_object->WhitespaceChildrenMayChange();
  return false;
}

bool Node::NeedsLayoutSubtreeUpdate() const {
  if (const auto* layout_object = GetLayoutObject()) {
    return layout_object->WhitespaceChildrenMayChange() ||
           layout_object->WasNotifiedOfSubtreeChange();
  }
  return false;
}

// FIXME: Shouldn't these functions be in the editing code?  Code that asks
// questions about HTML in the core DOM class is obviously misplaced.
bool Node::CanStartSelection() const {
  if (DisplayLockUtilities::LockedAncestorPreventingPaint(*this)) {
    if (const Element* element =
            FlatTreeTraversal::InclusiveParentElement(*this)) {
      GetDocument().UpdateStyleAndLayoutTreeForElement(
          element, DocumentUpdateReason::kSelection);
    }
  }
  if (IsEditable(*this)) {
    return true;
  }

  if (GetLayoutObject()) {
    const ComputedStyle& style = GetLayoutObject()->StyleRef();
    EUserSelect user_select = style.UsedUserSelect();
    if (user_select == EUserSelect::kNone)
      return false;
    // We allow selections to begin within |user-select: text/all| sub trees
    // but not if the element is draggable.
    if (style.UserDrag() != EUserDrag::kElement &&
        (user_select == EUserSelect::kText || user_select == EUserSelect::kAll))
      return true;
  }
  ContainerNode* parent = FlatTreeTraversal::Parent(*this);
  return parent ? parent->CanStartSelection() : true;
}

bool Node::IsRichlyEditableForAccessibility() const {
#if DCHECK_IS_ON()  // Required in order to get Lifecycle().ToString()
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kStyleClean)
      << "Unclean document style at lifecycle state "
      << GetDocument().Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  return IsRichlyEditable(*this);
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

bool Node::IsActiveSlot() const {
  return ToHTMLSlotElementIfSupportsAssignmentOrNull(*this);
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

ShadowRoot* Node::ParentElementShadowRoot() const {
  Element* parent = parentElement();
  return parent ? parent->GetShadowRoot() : nullptr;
}

bool Node::IsChildOfShadowHost() const {
  return ParentElementShadowRoot();
}

ShadowRoot* Node::ShadowRootOfParent() const {
  if (Element* parent = parentElement())
    return parent->GetShadowRoot();
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
  const AtomicString& namespace_uri = namespace_uri_maybe_empty.empty()
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

  if (namespace_uri.empty() || namespace_uri.IsNull())
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
  if (!specified_prefix.IsNull() && specified_prefix.empty())
    prefix = String();

  // 2. Return the result of running locate a namespace for the context object
  // using prefix.

  // https://dom.spec.whatwg.org/#locate-a-namespace
  switch (getNodeType()) {
    case kElementNode: {
      const auto& element = To<Element>(*this);

      // 1. If prefix is "xml", then return the XML namespace.
      if (prefix == g_xml_atom) {
        return xml_names::kNamespaceURI;
      }

      // 2. If prefix is "xmlns", then return the XMLNS namespace.
      if (prefix == g_xmlns_atom) {
        return xmlns_names::kNamespaceURI;
      }

      // 3. If its namespace is not null and its namespace prefix is prefix,
      // then return namespace.
      if (!element.namespaceURI().IsNull() && element.prefix() == prefix)
        return element.namespaceURI();

      // 4. If it has an attribute whose namespace is the XMLNS namespace,
      // namespace prefix is "xmlns", and local name is prefix, or if prefix is
      // null and it has an attribute whose namespace is the XMLNS namespace,
      // namespace prefix is null, and local name is "xmlns", then return its
      // value if it is not the empty string, and null otherwise.
      AttributeCollection attributes = element.Attributes();
      for (const Attribute& attr : attributes) {
        if (attr.Prefix() == g_xmlns_atom && attr.LocalName() == prefix) {
          if (!attr.Value().empty())
            return attr.Value();
          return g_null_atom;
        }
        if (attr.LocalName() == g_xmlns_atom && prefix.IsNull()) {
          if (!attr.Value().empty())
            return attr.Value();
          return g_null_atom;
        }
      }

      // 5. If its parent element is null, then return null.
      // 6. Return the result of running locate a namespace on its parent
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

String Node::textContent(bool convert_brs_to_newlines,
                         TextVisitor* visitor,
                         unsigned int max_length) const {
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
    if (visitor) {
      visitor->WillVisit(node, content.length());
    }
    if (IsA<HTMLBRElement>(node) && convert_brs_to_newlines) {
      content.Append('\n');
    } else if (auto* text_node = DynamicTo<Text>(node)) {
      content.Append(text_node->data());
      // Only abridge text content when max_length is explicitly set.
      if (max_length < UINT_MAX && content.length() > max_length) {
        content.Resize(max_length);
        break;
      }
    }
  }

  return content.ReleaseString();
}

V8UnionStringOrTrustedScript* Node::textContentForBinding() const {
  const String& value = textContent();
  if (value.IsNull())
    return nullptr;
  return MakeGarbageCollected<V8UnionStringOrTrustedScript>(value);
}

void Node::setTextContentForBinding(const V8UnionStringOrTrustedScript* value,
                                    ExceptionState& exception_state) {
  if (!value)
    return setTextContent(g_empty_string);

  switch (value->GetContentType()) {
    case V8UnionStringOrTrustedScript::ContentType::kString:
      return setTextContent(value->GetAsString());
    case V8UnionStringOrTrustedScript::ContentType::kTrustedScript:
      return setTextContent(value->GetAsTrustedScript()->toString());
  }

  NOTREACHED_IN_MIGRATION();
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
          To<Text>(container->firstChild())->data() == text && !text.empty())
        return;

      ChildListMutationScope mutation(*this);
      // Note: This API will not insert empty text nodes:
      // https://dom.spec.whatwg.org/#dom-node-textcontent
      if (text.empty()) {
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
  NOTREACHED_IN_MIGRATION();
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

    NOTREACHED_IN_MIGRATION();
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

      if (!child2->PseudoAwareNextSibling()) {
        return kDocumentPositionFollowing | connection;
      }
      if (!child1->PseudoAwareNextSibling()) {
        return kDocumentPositionPreceding | connection;
      }

      // Otherwise we need to see which node occurs first.  Crawl backwards from
      // child2 looking for child1.
      for (const Node* child = child2->PseudoAwarePreviousSibling(); child;
           child = child->PseudoAwarePreviousSibling()) {
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

void Node::InvalidateIfHasEffectiveAppearance() const {
  auto* layout_object = GetLayoutObject();
  if (!layout_object)
    return;

  if (!layout_object->StyleRef().HasEffectiveAppearance())
    return;

  layout_object->SetSubtreeShouldDoFullPaintInvalidation();
}

Node::InsertionNotificationRequest Node::InsertedInto(
    ContainerNode& insertion_point) {
  DCHECK(!ChildNeedsStyleInvalidation());
  DCHECK(!NeedsStyleInvalidation());
  DCHECK(insertion_point.isConnected() || insertion_point.IsInShadowTree() ||
         IsContainerNode() || GetDOMParts());
  if (insertion_point.isConnected()) {
    SetFlag(kIsConnectedFlag);
#if DCHECK_IS_ON()
    insertion_point.GetDocument().IncrementNodeCount();
#endif
  }
  if (ParentOrShadowHostNode()->IsInShadowTree())
    SetFlag(kIsInShadowTreeFlag);
  if (auto* cache = GetDocument().ExistingAXObjectCache()) {
    cache->NodeIsConnected(this);
  }

  return kInsertionDone;
}

void Node::RemovedFrom(ContainerNode& insertion_point) {
  DCHECK(IsContainerNode() || IsInTreeScope() || GetDOMParts());
  if (insertion_point.isConnected()) {
    ClearNeedsStyleRecalc();
    ClearChildNeedsStyleRecalc();
    ClearNeedsStyleInvalidation();
    ClearChildNeedsStyleInvalidation();
    ClearFlag(kIsConnectedFlag);
#if DCHECK_IS_ON()
    insertion_point.GetDocument().DecrementNodeCount();
#endif
  }
  if (IsInShadowTree() && !GetTreeScope().RootNode().IsShadowRoot()) {
    ClearFlag(kIsInShadowTreeFlag);
  }
  if (auto* cache = GetDocument().ExistingAXObjectCache()) {
    cache->Remove(this);
  }
}

String Node::DebugName() const {
  StringBuilder name;
  name.Append(nodeName());
  if (const auto* vt_pseudo =
          DynamicTo<ViewTransitionPseudoElementBase>(this)) {
    name.Append("(");
    name.Append(vt_pseudo->view_transition_name());
    name.Append(")");
  } else if (const auto* this_element = DynamicTo<Element>(this)) {
    if (this_element->HasID()) {
      name.Append(" id=\'");
      name.Append(this_element->GetIdAttribute());
      name.Append('\'');
    }

    if (this_element->HasClass()) {
      name.Append(" class=\'");
      for (wtf_size_t i = 0; i < this_element->ClassNames().size(); ++i) {
        if (i > 0)
          name.Append(' ');
        name.Append(this_element->ClassNames()[i]);
      }
      name.Append('\'');
    }
  }
  return name.ReleaseString();
}

static void DumpAttributeDesc(const Node& node,
                              const QualifiedName& name,
                              StringBuilder& builder) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return;
  const AtomicString& value = element->getAttribute(name);
  if (value.empty())
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
    shadow_root_type << shadow_root->GetMode();
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
    return builder.ReleaseString();
  } else if (const auto* vt_pseudo =
                 DynamicTo<ViewTransitionPseudoElementBase>(this)) {
    builder.Append("(");
    builder.Append(vt_pseudo->view_transition_name());
    builder.Append(")");
  } else if (const auto* element = DynamicTo<Element>(this)) {
    const AtomicString& pseudo = element->ShadowPseudoId();
    if (!pseudo.empty()) {
      builder.Append(" ::");
      builder.Append(pseudo);
    }
    DumpAttributeDesc(*this, html_names::kIdAttr, builder);
    DumpAttributeDesc(*this, html_names::kClassAttr, builder);
    DumpAttributeDesc(*this, html_names::kStyleAttr, builder);
  }
  if (IsEditable(*this))
    builder.Append(" (editable)");
  if (GetDocument().FocusedElement() == this)
    builder.Append(" (focused)");
  return builder.ReleaseString();
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
        bool has_id_attr = !idattr.IsNull() && !idattr.empty();
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

    String indent_string = indent.ReleaseString();

    if (const auto* element = DynamicTo<Element>(node)) {
      if (Element* pseudo =
              element->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore)) {
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      }
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdMarker)) {
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      }
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdScrollMarker)) {
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      }
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdBefore))
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdAfter))
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo =
              element->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter)) {
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      }
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdFirstLetter))
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
      if (Element* pseudo = element->GetPseudoElement(kPseudoIdBackdrop))
        AppendMarkedTree(indent_string, pseudo, marked_node1, marked_label1,
                         marked_node2, marked_label2, builder);
    }

    if (ShadowRoot* shadow_root = node.GetShadowRoot()) {
      AppendMarkedTree(indent_string, shadow_root, marked_node1, marked_label1,
                       marked_node2, marked_label2, builder);
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
      AppendMarkedFlatTree(indent.ReleaseString(), child, marked_node1,
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
  return builder.ReleaseString();
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
  return builder.ReleaseString();
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

void Node::WillMoveToNewDocument(Document& new_document) {
  Document& old_document = GetDocument();
  DCHECK_NE(&old_document, &new_document);

  // In rare situations, this node may be the focused element of the old
  // document. In this case, we need to clear the focused element of the old
  // document, and since we are currently in an event forbidden scope, we can't
  // fire the blur event.
  if (old_document.FocusedElement() == this) {
    FocusParams params(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr);
    params.omit_blur_events = true;
    old_document.SetFocusedElement(nullptr, params);
  }

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
  DCHECK_NE(&GetDocument(), &old_document);

  if (auto* text_node = DynamicTo<Text>(this)) {
    old_document.Markers().RemoveMarkersForNode(*text_node);
  }
}

void Node::AddedEventListener(const AtomicString& event_type,
                              RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  GetDocument().AddListenerTypeIfNeeded(event_type, *this);
  GetDocument().DidAddEventListeners(/*count*/ 1);
  if (auto* frame = GetDocument().GetFrame()) {
    frame->GetEventHandlerRegistry().DidAddEventHandler(
        *this, event_type, registered_listener.Options());
    // We need to track the existence of the visibilitychange event listeners to
    // enable/disable sudden terminations.
    if (IsDocumentNode() && event_type == event_type_names::kVisibilitychange) {
      frame->AddedSuddenTerminationDisablerListener(*this, event_type);
    }
  }
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->HandleEventListenerAdded(*this, event_type);
}

void Node::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  GetDocument().DidRemoveEventListeners(/*count*/ 1);
  // FIXME: Notify Document that the listener has vanished. We need to keep
  // track of a number of listeners for each type, not just a bool - see
  // https://bugs.webkit.org/show_bug.cgi?id=33861
  if (auto* frame = GetDocument().GetFrame()) {
    frame->GetEventHandlerRegistry().DidRemoveEventHandler(
        *this, event_type, registered_listener.Options());
  }
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->HandleEventListenerRemoved(*this, event_type);
}

void Node::RemoveAllEventListeners() {
  Vector<AtomicString> event_types = EventTypes();
  Document& document = GetDocument();
  if (HasEventListeners()) {
    GetEventTargetData()->event_listener_map.ForAllEventListenerTypes(
        [&document](const AtomicString& event_type, uint32_t count) {
          document.DidRemoveEventListeners(count);
        });

    if (document.GetPage()) {
      document.GetFrame()->GetEventHandlerRegistry().DidRemoveAllEventHandlers(
          *this);
    }
  }
  EventTarget::RemoveAllEventListeners();
  if (AXObjectCache* cache = document.ExistingAXObjectCache()) {
    for (const AtomicString& event_type : event_types) {
      cache->HandleEventListenerRemoved(*this, event_type);
    }
  }
}

void Node::RemoveAllEventListenersRecursively() {
  ScriptForbiddenScope forbid_script_during_raw_iteration;
  for (Node& node : NodeTraversal::StartsAt(*this)) {
    node.RemoveAllEventListeners();
    if (ShadowRoot* root = node.GetShadowRoot())
      root->RemoveAllEventListenersRecursively();
  }
}

void Node::MoveEventListenersToNewDocument(Document& old_document,
                                           Document& new_document) {
  DCHECK_EQ(&new_document, &GetDocument());
  if (const EventTargetData* event_target_data = GetEventTargetData()) {
    const EventListenerMap& listener_map =
        event_target_data->event_listener_map;
    if (!listener_map.IsEmpty()) {
      listener_map.ForAllEventListenerTypes(
          [this, &old_document, &new_document](const AtomicString& event_type,
                                               uint32_t count) {
            old_document.DidRemoveEventListeners(count);
            new_document.AddListenerTypeIfNeeded(event_type, *this);
            new_document.DidAddEventListeners(count);
          });
    }
  }

  if (new_document.GetPage() &&
      new_document.GetPage() != old_document.GetPage()) {
    new_document.GetFrame()->GetEventHandlerRegistry().DidMoveIntoPage(*this);
  }
}

const HeapVector<Member<MutationObserverRegistration>>*
Node::MutationObserverRegistry() {
  if (!data_) {
    return nullptr;
  }
  NodeMutationObserverData* data = data_->MutationObserverData();
  if (!data)
    return nullptr;
  return &data->Registry();
}

const HeapHashSet<Member<MutationObserverRegistration>>*
Node::TransientMutationObserverRegistry() {
  if (!data_) {
    return nullptr;
  }
  NodeMutationObserverData* data = data_->MutationObserverData();
  if (!data)
    return nullptr;
  return &data->TransientRegistry();
}

void Node::MoveMutationObserversToNewDocument(Document& new_document) {
  DCHECK_EQ(&new_document, &GetDocument());
  if (const HeapVector<Member<MutationObserverRegistration>>* registry =
          MutationObserverRegistry()) {
    for (const auto& registration : *registry) {
      new_document.AddMutationObserverTypes(registration->MutationTypes());
    }
  }

  if (const HeapHashSet<Member<MutationObserverRegistration>>*
          transient_registry = TransientMutationObserverRegistry()) {
    for (const auto& registration : *transient_registry) {
      new_document.AddMutationObserverTypes(registration->MutationTypes());
    }
  }
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
  if (!GetEventTargetData()) {
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
  if (IsInShadowTree() || GetDocument().ShouldSuppressMutationEvents()) {
    return;
  }

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
                                  SimulatedClickCreationScope scope) {
  if (auto* element = IsElementNode() ? To<Element>(this) : parentElement()) {
    element->ActivateDisplayLockIfNeeded(
        DisplayLockActivationReason::kSimulatedClick);
  }
  EventDispatcher::DispatchSimulatedClick(*this, underlying_event, scope);
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

      // Avoid that IsUserScrollable changes layout tree structure.
      // FIXME: We should avoid synchronous layout if possible. We can
      // remove this synchronous layout if we avoid synchronous layout in
      // LayoutTextControlSingleLine::scrollHeight
      GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInput);
      LayoutObject* layout_object = GetLayoutObject();
      while (layout_object &&
             (!layout_object->IsBox() ||
              !To<LayoutBox>(layout_object)->IsUserScrollable())) {
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
  // ShouldHaveFocusAppearance and theme painting.
  if (GetLayoutObject()) {
    InvalidateIfHasEffectiveAppearance();

    if (auto* this_element = DynamicTo<Element>(this)) {
      this_element->FocusVisibleStateChanged();
    }
  }
}

bool Node::HasActivationBehavior() const {
  return false;
}

bool Node::WillRespondToMouseMoveEvents() const {
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
  return IsEditable(*this) ||
         HasAnyEventListeners(event_util::MouseButtonEventTypes());
}

unsigned Node::ConnectedSubframeCount() const {
  return data_ ? data_->ConnectedSubframeCount() : 0;
}

void Node::IncrementConnectedSubframeCount() {
  DCHECK(IsContainerNode());
  EnsureRareData().IncrementConnectedSubframeCount();
}

void Node::DecrementConnectedSubframeCount() {
  RareData()->DecrementConnectedSubframeCount();
}

ShadowRoot* Node::GetSlotAssignmentRoot() const {
  DCHECK(!IsPseudoElement());
  ShadowRoot* root = ShadowRootOfParent();
  return (root && root->HasSlotAssignment()) ? root : nullptr;
}

HTMLSlotElement* Node::AssignedSlot() const {
  ShadowRoot* root = GetSlotAssignmentRoot();
  if (!root)
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
#if DCHECK_IS_ON()
    // User agent shadow slot assignment (FindSlotInUserAgentShadow()) will
    // re-check the DOM tree, and if we're in the process of removing nodes
    // from the tree, there could be a mismatch here.
    if (root->IsNamedSlotting()) {
      DCHECK_EQ(root->AssignedSlotFor(*this), data->AssignedSlot())
          << "Assigned slot mismatch for node " << this;
    }
#endif
    return data->AssignedSlot();
  }
  return nullptr;
}

// Used when assignment recalc is forbidden, i.e., DetachLayoutTree().
// Returned assignedSlot is not guaranteed up to date.
HTMLSlotElement* Node::AssignedSlotWithoutRecalc() const {
  if (!GetSlotAssignmentRoot())
    return nullptr;

  if (FlatTreeNodeData* data = GetFlatTreeNodeData())
    return data->AssignedSlot();

  return nullptr;
}

HTMLSlotElement* Node::assignedSlotForBinding() {
  // assignedSlot doesn't need to recalc slot assignment
  if (ShadowRoot* root = ShadowRootOfParent()) {
    if (root->GetMode() == ShadowRootMode::kOpen) {
      return AssignedSlot();
    }
  }
  return nullptr;
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
      NOTREACHED_IN_MIGRATION();  // Everything starts in this state
      return;

    case CustomElementState::kUndefined:
      DCHECK_EQ(CustomElementState::kUncustomized, old_state);
      break;

    case CustomElementState::kCustom:
      DCHECK(old_state == CustomElementState::kUndefined ||
             old_state == CustomElementState::kFailed ||
             old_state == CustomElementState::kPreCustomized);
      break;

    case CustomElementState::kFailed:
      DCHECK_NE(CustomElementState::kFailed, old_state);
      break;

    case CustomElementState::kPreCustomized:
      DCHECK_EQ(CustomElementState::kFailed, old_state);
      break;
  }

  DCHECK(IsHTMLElement());

  auto* element = To<Element>(this);
  bool was_defined = element->IsDefined();

  node_flags_ = (node_flags_ & ~kCustomElementStateMask) |
                static_cast<NodeFlags>(new_state);
  DCHECK(new_state == GetCustomElementState());

  if (element->IsDefined() != was_defined)
    element->PseudoStateChanged(CSSSelector::kPseudoDefined);
}

void Node::CheckSlotChange(SlotChangeType slot_change_type) {
  // Common check logic is used in both cases, "after inserted" and "before
  // removed". This function calls DidSlotChange() on the appropriate nodes,
  // e.g. the assigned slot for this node, or the parent slot for a slot's
  // fallback content.

  // Relevant DOM Standard:
  // https://dom.spec.whatwg.org/#concept-node-insert
  // https://dom.spec.whatwg.org/#concept-node-remove

  // This function is usually called while DOM Mutation is still in-progress.
  // For "after inserted" case, we assume that a parent and a child have been
  // already connected. For "before removed" case, we assume that a parent and a
  // child have not been disconnected yet.

  if (!IsSlotable())
    return;

  if (ShadowRoot* root = ShadowRootOfParent()) {
    // A shadow host's child can be assigned to a slot in the host's shadow
    // tree.

    // Although DOM Standard requires "assign a slot for node / run assign
    // slotables" at this timing, we skip it as an optimization.
    if (HTMLSlotElement* slot = root->AssignedSlotFor(*this))
      slot->DidSlotChange(slot_change_type);
  } else if (IsInShadowTree()) {
    // Checking for fallback content if the node is in a shadow tree.
    if (auto* parent_slot = DynamicTo<HTMLSlotElement>(parentElement())) {
      // The parent_slot's assigned nodes might not be calculated because they
      // are lazy evaluated later in RecalcAssignment(), so we have to check
      // here. Also, parent_slot may have already been removed, if this was the
      // removal of nested slots, e.g.
      //   <slot name=parent-slot><slot name=this-slot>fallback</slot></slot>.
      // In that case, parent-slot has already been removed, so parent_slot->
      // SupportsAssignment() is false, but this-slot is still in the process
      // of being removed, so IsInShadowTree() is still true.
      if (parent_slot->SupportsAssignment() &&
          !parent_slot->HasAssignedNodesSlow())
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

  if (auto* embedded = DynamicTo<LayoutEmbeddedContent>(GetLayoutObject()))
    return embedded->Plugin();
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

void Node::ParentSlotChanged() {
  if (!isConnected()) {
    return;
  }
  DCHECK(IsSlotable());
  DCHECK(IsShadowHost(parentNode()) || IsA<HTMLSlotElement>(parentNode()));
  FlatTreeParentChanged();
}

void Node::FlatTreeParentChanged() {
  DCHECK(isConnected());
  const ComputedStyle* style =
      IsElementNode() ? To<Element>(this)->GetComputedStyle() : nullptr;
  bool detach = false;
  if (ShouldSkipMarkingStyleDirty()) {
    // If we should not mark the node dirty in the new flat tree position,
    // detach to make sure all computes styles, layout objects, and dirty
    // flags are cleared.
    detach = IsDirtyForStyleRecalc() || ChildNeedsStyleRecalc() || style ||
             GetLayoutObject();
  }
  if (!detach) {
    // We are moving a node with ensured computed style into the flat tree.
    // Clear ensured styles so that we can use IsEnsuredOutsideFlatTree() to
    // determine that we are outside the flat tree before updating the style
    // recalc root in MarkAncestorsWithChildNeedsStyleRecalc().
    detach = style && style->IsEnsuredOutsideFlatTree();
  }
  if (detach) {
    StyleEngine& engine = GetDocument().GetStyleEngine();
    StyleEngine::DetachLayoutTreeScope detach_scope(engine);
    DetachLayoutTree();
    engine.FlatTreePositionChanged(*this);
  }

  // The node changed the flat tree position by being slotted to a new slot or
  // slotted for the first time. We need to recalc style since the inheritance
  // parent may have changed.
  if (!ShouldSkipMarkingStyleDirty()) {
    if (NeedsStyleRecalc()) {
      // The ancestor chain may have changed. We need to make sure that the
      // child-dirty flags are updated, but the SetNeedsStyleRecalc() call below
      // will skip MarkAncestorsWithChildNeedsStyleRecalc() if the node was
      // already dirty.
      MarkAncestorsWithChildNeedsStyleRecalc();
    } else {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::Create(
                              style_change_reason::kFlatTreeChange));
    }
    // We also need to force a layout tree re-attach since the layout tree
    // parent box may have changed.
    SetForceReattachLayoutTree();
  }
}

void Node::RemovedFromFlatTree() {
  StyleEngine& engine = GetDocument().GetStyleEngine();
  StyleEngine::DetachLayoutTreeScope detach_scope(engine);
  // This node was previously part of the flat tree, but due to slot re-
  // assignment it no longer is. We need to detach the layout tree and notify
  // the StyleEngine in case the StyleRecalcRoot is removed from the flat tree.
  {
    StyleEngine::DOMRemovalScope style_scope(engine);
    DetachLayoutTree();
  }
  GetDocument().GetStyleEngine().FlatTreePositionChanged(*this);
}

void Node::RegisterScrollTimeline(ScrollTimeline* timeline) {
  EnsureRareData().RegisterScrollTimeline(timeline);
}
void Node::UnregisterScrollTimeline(ScrollTimeline* timeline) {
  EnsureRareData().UnregisterScrollTimeline(timeline);
}

void Node::SetManuallyAssignedSlot(HTMLSlotElement* slot) {
  EnsureFlatTreeNodeData().SetManuallyAssignedSlot(slot);
}
HTMLSlotElement* Node::ManuallyAssignedSlot() {
  if (FlatTreeNodeData* data = GetFlatTreeNodeData())
    return data->ManuallyAssignedSlot();
  return nullptr;
}

HeapHashSet<Member<TreeScope>> Node::GetAncestorTreeScopes() const {
  HeapHashSet<Member<TreeScope>> ancestor_tree_scopes;
  for (TreeScope* scope = &GetTreeScope(); scope;
       scope = scope->ParentTreeScope()) {
    ancestor_tree_scopes.insert(scope);
  }
  return ancestor_tree_scopes;
}

void Node::SetCachedDirectionality(TextDirection direction) {
  switch (direction) {
    case TextDirection::kRtl:
      SetFlag(kCachedDirectionalityIsRtl);
      break;
    case TextDirection::kLtr:
      ClearFlag(kCachedDirectionalityIsRtl);
      break;
  }
}

void Node::AddConsoleMessage(mojom::blink::ConsoleMessageSource source,
                             mojom::blink::ConsoleMessageLevel level,
                             const String& message) {
  auto* console_message =
      MakeGarbageCollected<ConsoleMessage>(source, level, message);
  console_message->SetNodes(GetDocument().GetFrame(), {GetDomNodeId()});
  GetDocument().AddConsoleMessage(console_message);
}

void Node::Trace(Visitor* visitor) const {
  visitor->Trace(parent_or_shadow_host_node_);
  visitor->Trace(tree_scope_);
  visitor->Trace(previous_);
  visitor->Trace(next_);
  visitor->Trace(layout_object_);
  visitor->Trace(data_);
  EventTarget::Trace(visitor);
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowNode(const blink::Node* node) {
  if (node)
    LOG(INFO) << *node;
  else
    LOG(INFO) << "Cannot showNode for <null>";
}

void ShowTree(const blink::Node* node) {
  if (node)
    LOG(INFO) << "\n" << node->ToTreeStringForThis().Utf8();
  else
    LOG(INFO) << "Cannot showTree for <null>";
}

void ShowNodePath(const blink::Node* node) {
  if (node) {
    std::stringstream stream;
    node->PrintNodePathTo(stream);
    LOG(INFO) << stream.str();
  } else {
    LOG(INFO) << "Cannot showNodePath for <null>";
  }
}

#endif
