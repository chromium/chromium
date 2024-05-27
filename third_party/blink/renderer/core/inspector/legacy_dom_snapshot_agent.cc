// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/legacy_dom_snapshot_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/attribute_collection.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/dom_traversal_utils.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "v8/include/v8-inspector.h"

namespace blink {

using mojom::blink::FormControlType;
using protocol::Maybe;

namespace {

std::unique_ptr<protocol::DOM::Rect> LegacyBuildRectForPhysicalRect(
    const PhysicalRect& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.X())
      .setY(rect.Y())
      .setWidth(rect.Width())
      .setHeight(rect.Height())
      .build();
}

}  // namespace

struct LegacyDOMSnapshotAgent::VectorStringHashTraits
    : public WTF::GenericHashTraits<Vector<String>> {
  static unsigned GetHash(const Vector<String>& vec) {
    unsigned h = WTF::GetHash(vec.size());
    for (const String& s : vec) {
      h = WTF::HashInts(h, WTF::GetHash(s));
    }
    return h;
  }

  static bool Equal(const Vector<String>& a, const Vector<String>& b) {
    if (a.size() != b.size())
      return false;
    for (wtf_size_t i = 0; i < a.size(); i++) {
      if (a[i] != b[i])
        return false;
    }
    return true;
  }

  static void ConstructDeletedValue(Vector<String>& vec) {
    new (WTF::NotNullTag::kNotNull, &vec)
        Vector<String>(WTF::kHashTableDeletedValue);
  }

  static bool IsDeletedValue(const Vector<String>& vec) {
    return vec.IsHashTableDeletedValue();
  }

  static bool IsEmptyValue(const Vector<String>& vec) { return vec.empty(); }

  static constexpr bool kEmptyValueIsZero = false;
  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

LegacyDOMSnapshotAgent::LegacyDOMSnapshotAgent(
    InspectorDOMDebuggerAgent* dom_debugger_agent,
    OriginUrlMap* origin_url_map)
    : origin_url_map_(origin_url_map),
      dom_debugger_agent_(dom_debugger_agent) {}

LegacyDOMSnapshotAgent::~LegacyDOMSnapshotAgent() = default;

protocol::Response LegacyDOMSnapshotAgent::GetSnapshot(
    Document* document,
    std::unique_ptr<protocol::Array<String>> style_filter,
    protocol::Maybe<bool> include_event_listeners,
    protocol::Maybe<bool> include_paint_order,
    protocol::Maybe<bool> include_user_agent_shadow_tree,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>* dom_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
        layout_tree_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
        computed_styles) {
  document->View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kInspector);
  // Setup snapshot.
  dom_nodes_ =
      std::make_unique<protocol::Array<protocol::DOMSnapshot::DOMNode>>();
  layout_tree_nodes_ = std::make_unique<
      protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>();
  computed_styles_ =
      std::make_unique<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>();
  computed_styles_map_ = std::make_unique<ComputedStylesMap>();
  css_property_filter_ = std::make_unique<CSSPropertyFilter>();

  // Look up the CSSPropertyIDs for each entry in |style_filter|.
  for (const String& entry : *style_filter) {
    CSSPropertyID property_id =
        CssPropertyID(document->GetExecutionContext(), entry);
    if (property_id == CSSPropertyID::kInvalid)
      continue;
    css_property_filter_->emplace_back(entry, property_id);
  }

  if (include_paint_order.value_or(false)) {
    paint_order_map_ = InspectorDOMSnapshotAgent::BuildPaintLayerTree(document);
  }

  // Actual traversal.
  VisitNode(document, include_event_listeners.value_or(false),
            include_user_agent_shadow_tree.value_or(false));

  // Extract results from state and reset.
  *dom_nodes = std::move(dom_nodes_);
  *layout_tree_nodes = std::move(layout_tree_nodes_);
  *computed_styles = std::move(computed_styles_);
  computed_styles_map_.reset();
  css_property_filter_.reset();
  paint_order_map_ = nullptr;
  return protocol::Response::Success();
}

int LegacyDOMSnapshotAgent::VisitNode(Node* node,
                                      bool include_event_listeners,
                                      bool include_user_agent_shadow_tree) {
  // Update layout tree before traversal of document so that we inspect a
  // current and consistent state of all trees. No need to do this if paint
  // order was calculated, since layout trees were already updated during
  // TraversePaintLayerTree().
  if (node->IsDocumentNode() && !paint_order_map_)
    node->GetDocument().UpdateStyleAndLayoutTree();

  String node_value;
  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kAttributeNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
    case Node::kDocumentFragmentNode:
      node_value = node->nodeValue();
      break;
    default:
      break;
  }

  // Create DOMNode object and add it to the result array before traversing
  // children, so that parents appear before their children in the array.
  std::unique_ptr<protocol::DOMSnapshot::DOMNode> owned_value =
      protocol::DOMSnapshot::DOMNode::create()
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setNodeValue(node_value)
          .setBackendNodeId(IdentifiersFactory::IntIdForNode(node))
          .build();
  if (origin_url_map_ &&
      origin_url_map_->Contains(owned_value->getBackendNodeId())) {
    String origin_url = origin_url_map_->at(owned_value->getBackendNodeId());
    // In common cases, it is implicit that a child node would have the same
    // origin url as its parent, so no need to mark twice.
    if (!node->parentNode()) {
      owned_value->setOriginURL(std::move(origin_url));
    } else {
      DOMNodeId parent_id = node->parentNode()->GetDomNodeId();
      auto it = origin_url_map_->find(parent_id);
      String parent_url = it != origin_url_map_->end() ? it->value : String();
      if (parent_url != origin_url)
        owned_value->setOriginURL(std::move(origin_url));
    }
  }
  protocol::DOMSnapshot::DOMNode* value = owned_value.get();
  int index = static_cast<int>(dom_nodes_->size());
  dom_nodes_->emplace_back(std::move(owned_value));

  int layoutNodeIndex =
      VisitLayoutTreeNode(node->GetLayoutObject(), node, index);
  if (layoutNodeIndex != -1)
    value->setLayoutNodeIndex(layoutNodeIndex);

  if (node->WillRespondToMouseClickEvents())
    value->setIsClickable(true);

  if (include_event_listeners && node->GetDocument().GetFrame()) {
    ScriptState* script_state =
        ToScriptStateForMainWorld(node->GetDocument().GetFrame());
    if (script_state->ContextIsValid()) {
      ScriptState::Scope scope(script_state);
      v8::Local<v8::Context> context = script_state->GetContext();
      V8EventListenerInfoList event_information;
      InspectorDOMDebuggerAgent::CollectEventListeners(
          script_state->GetIsolate(), node, v8::Local<v8::Value>(), node, true,
          &event_information);
      if (!event_information.empty()) {
        value->setEventListeners(
            dom_debugger_agent_->BuildObjectsForEventListeners(
                event_information, context, v8_inspector::StringView()));
      }
    }
  }

  auto* element = DynamicTo<Element>(node);
  if (element) {
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (LocalFrame* frame =
              DynamicTo<LocalFrame>(frame_owner->ContentFrame()))
        value->setFrameId(IdentifiersFactory::FrameId(frame));

      if (Document* doc = frame_owner->contentDocument()) {
        value->setContentDocumentIndex(VisitNode(
            doc, include_event_listeners, include_user_agent_shadow_tree));
      }
    }

    if (node->parentNode() && node->parentNode()->IsDocumentNode()) {
      LocalFrame* frame = node->GetDocument().GetFrame();
      if (frame)
        value->setFrameId(IdentifiersFactory::FrameId(frame));
    }

    if (auto* textarea_element = DynamicTo<HTMLTextAreaElement>(*element))
      value->setTextValue(textarea_element->Value());

    if (auto* input_element = DynamicTo<HTMLInputElement>(*element)) {
      value->setInputValue(input_element->Value());
      if ((input_element->FormControlType() == FormControlType::kInputRadio) ||
          (input_element->FormControlType() ==
           FormControlType::kInputCheckbox)) {
        value->setInputChecked(input_element->Checked());
      }
    }

    if (auto* option_element = DynamicTo<HTMLOptionElement>(*element))
      value->setOptionSelected(option_element->Selected());

    if (element->IsPseudoElement()) {
      value->setPseudoType(InspectorDOMAgent::ProtocolPseudoElementType(
          element->GetPseudoIdForStyling()));
    }
    value->setPseudoElementIndexes(
        VisitPseudoElements(element, index, include_event_listeners,
                            include_user_agent_shadow_tree));

    auto* image_element = DynamicTo<HTMLImageElement>(node);
    if (image_element)
      value->setCurrentSourceURL(image_element->currentSrc());
  } else if (auto* document = DynamicTo<Document>(node)) {
    value->setDocumentURL(InspectorDOMAgent::DocumentURLString(document));
    value->setBaseURL(InspectorDOMAgent::DocumentBaseURLString(document));
    if (document->ContentLanguage())
      value->setContentLanguage(document->ContentLanguage().Utf8().c_str());
    if (document->EncodingName())
      value->setDocumentEncoding(document->EncodingName().Utf8().c_str());
    value->setFrameId(IdentifiersFactory::FrameId(document->GetFrame()));
    if (document->View() && document->View()->LayoutViewport()) {
      auto offset = document->View()->LayoutViewport()->GetScrollOffset();
      value->setScrollOffsetX(offset.x());
      value->setScrollOffsetY(offset.y());
    }
  } else if (auto* doc_type = DynamicTo<DocumentType>(node)) {
    value->setPublicId(doc_type->publicId());
    value->setSystemId(doc_type->systemId());
  }
  if (node->IsInShadowTree()) {
    value->setShadowRootType(
        InspectorDOMAgent::GetShadowRootType(node->ContainingShadowRoot()));
  }

  if (node->IsContainerNode()) {
    value->setChildNodeIndexes(VisitContainerChildren(
        node, include_event_listeners, include_user_agent_shadow_tree));
  }
  return index;
}

std::unique_ptr<protocol::Array<int>>
LegacyDOMSnapshotAgent::VisitContainerChildren(
    Node* container,
    bool include_event_listeners,
    bool include_user_agent_shadow_tree) {
  auto children = std::make_unique<protocol::Array<int>>();

  if (!blink::dom_traversal_utils::HasChildren(*container,
                                               include_user_agent_shadow_tree))
    return nullptr;

  Node* child = blink::dom_traversal_utils::FirstChild(
      *container, include_user_agent_shadow_tree);
  while (child) {
    children->emplace_back(VisitNode(child, include_event_listeners,
                                     include_user_agent_shadow_tree));
    child = blink::dom_traversal_utils::NextSibling(
        *child, include_user_agent_shadow_tree);
  }

  return children;
}

std::unique_ptr<protocol::Array<int>>
LegacyDOMSnapshotAgent::VisitPseudoElements(
    Element* parent,
    int index,
    bool include_event_listeners,
    bool include_user_agent_shadow_tree) {
  if (!parent->GetPseudoElement(kPseudoIdFirstLetter) &&
      !parent->GetPseudoElement(kPseudoIdBefore) &&
      !parent->GetPseudoElement(kPseudoIdAfter)) {
    return nullptr;
  }

  auto pseudo_elements = std::make_unique<protocol::Array<int>>();
  for (PseudoId pseudo_id :
       {kPseudoIdFirstLetter, kPseudoIdBefore, kPseudoIdAfter}) {
    if (Node* pseudo_node = parent->GetPseudoElement(pseudo_id)) {
      pseudo_elements->emplace_back(VisitNode(pseudo_node,
                                              include_event_listeners,
                                              include_user_agent_shadow_tree));
    }
  }
  return pseudo_elements;
}

std::unique_ptr<protocol::Array<protocol::DOMSnapshot::NameValue>>
LegacyDOMSnapshotAgent::BuildArrayForElementAttributes(Element* element) {
  AttributeCollection attributes = element->Attributes();
  if (attributes.IsEmpty())
    return nullptr;
  auto attributes_value =
      std::make_unique<protocol::Array<protocol::DOMSnapshot::NameValue>>();
  for (const auto& attribute : attributes) {
    attributes_value->emplace_back(protocol::DOMSnapshot::NameValue::create()
                                       .setName(attribute.GetName().ToString())
                                       .setValue(attribute.Value())
                                       .build());
  }
  return attributes_value;
}

int LegacyDOMSnapshotAgent::VisitLayoutTreeNode(LayoutObject* layout_object,
                                                Node* node,
                                                int node_index) {
  if (!layout_object)
    return -1;

  if (node->IsPseudoElement()) {
    // For pseudo elements, visit the children of the layout object.
    for (LayoutObject* child = layout_object->SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsAnonymous())
        VisitLayoutTreeNode(child, node, node_index);
    }
  }

  auto layout_tree_node =
      protocol::DOMSnapshot::LayoutTreeNode::create()
          .setDomNodeIndex(node_index)
          .setBoundingBox(LegacyBuildRectForPhysicalRect(
              InspectorDOMSnapshotAgent::RectInDocument(layout_object)))
          .build();

  int style_index = GetStyleIndexForNode(node);
  if (style_index != -1)
    layout_tree_node->setStyleIndex(style_index);

  if (layout_object->Style() && layout_object->IsStackingContext())
    layout_tree_node->setIsStackingContext(true);

  if (paint_order_map_) {
    PaintLayer* paint_layer = layout_object->EnclosingLayer();

    // We visited all PaintLayers when building |paint_order_map_|.
    const auto paint_order = paint_order_map_->find(paint_layer);
    if (paint_order != paint_order_map_->end())
      layout_tree_node->setPaintOrder(paint_order->value);
  }

  if (layout_object->IsText()) {
    auto* layout_text = To<LayoutText>(layout_object);
    layout_tree_node->setLayoutText(layout_text->TransformedText());
    Vector<LayoutText::TextBoxInfo> text_boxes = layout_text->GetTextBoxInfo();
    if (!text_boxes.empty()) {
      auto inline_text_nodes = std::make_unique<
          protocol::Array<protocol::DOMSnapshot::InlineTextBox>>();
      for (const auto& text_box : text_boxes) {
        inline_text_nodes->emplace_back(
            protocol::DOMSnapshot::InlineTextBox::create()
                .setStartCharacterIndex(text_box.dom_start_offset)
                .setNumCharacters(text_box.dom_length)
                .setBoundingBox(LegacyBuildRectForPhysicalRect(
                    InspectorDOMSnapshotAgent::TextFragmentRectInDocument(
                        layout_object, text_box)))
                .build());
      }
      layout_tree_node->setInlineTextNodes(std::move(inline_text_nodes));
    }
  }

  int index = static_cast<int>(layout_tree_nodes_->size());
  layout_tree_nodes_->emplace_back(std::move(layout_tree_node));
  return index;
}

const ComputedStyle* ComputedStyleForNode(Node& node) {
  if (Element* element = DynamicTo<Element>(node)) {
    return element->EnsureComputedStyle();
  }
  if (!node.IsTextNode()) {
    return nullptr;
  }
  if (LayoutObject* layout_object = node.GetLayoutObject()) {
    return layout_object->Style();
  }
  if (Element* parent_element = FlatTreeTraversal::ParentElement(node)) {
    return parent_element->EnsureComputedStyle();
  }
  return nullptr;
}

int LegacyDOMSnapshotAgent::GetStyleIndexForNode(Node* node) {
  CHECK(node);
  const ComputedStyle* computed_style = ComputedStyleForNode(*node);
  if (!computed_style) {
    return -1;
  }
  Vector<String> style;
  bool all_properties_empty = true;
  for (const auto& pair : *css_property_filter_) {
    String value;
    if (const CSSValue* css_value =
            CSSProperty::Get(pair.second)
                .CSSValueFromComputedStyle(*computed_style,
                                           node->GetLayoutObject(), true,
                                           CSSValuePhase::kResolvedValue)) {
      value = css_value->CssText();
    }
    if (!value.empty())
      all_properties_empty = false;
    style.push_back(value);
  }

  // -1 means an empty style.
  if (all_properties_empty)
    return -1;

  ComputedStylesMap::iterator it = computed_styles_map_->find(style);
  if (it != computed_styles_map_->end())
    return it->value;

  // It's a distinct style, so append to |computedStyles|.
  auto style_properties =
      std::make_unique<protocol::Array<protocol::DOMSnapshot::NameValue>>();

  for (wtf_size_t i = 0; i < style.size(); i++) {
    if (style[i].empty())
      continue;
    style_properties->emplace_back(
        protocol::DOMSnapshot::NameValue::create()
            .setName((*css_property_filter_)[i].first)
            .setValue(style[i])
            .build());
  }

  wtf_size_t index = static_cast<wtf_size_t>(computed_styles_->size());
  computed_styles_->emplace_back(protocol::DOMSnapshot::ComputedStyle::create()
                                     .setProperties(std::move(style_properties))
                                     .build());
  computed_styles_map_->insert(std::move(style), index);
  return index;
}

}  // namespace blink
