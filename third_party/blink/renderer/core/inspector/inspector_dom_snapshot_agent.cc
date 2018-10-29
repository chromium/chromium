// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/attribute_collection.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
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
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node_iterator.h"
#include "v8/include/v8-inspector.h"

namespace blink {
using protocol::Maybe;
using protocol::Response;

namespace {

std::unique_ptr<protocol::DOM::Rect> BuildRectForLayoutRect(
    const LayoutRect& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.X())
      .setY(rect.Y())
      .setWidth(rect.Width())
      .setHeight(rect.Height())
      .build();
}

std::unique_ptr<protocol::Array<double>> BuildRectForLayoutRect2(
    const LayoutRect& rect) {
  std::unique_ptr<protocol::Array<double>> result =
      protocol::Array<double>::create();
  result->addItem(rect.X());
  result->addItem(rect.Y());
  result->addItem(rect.Width());
  result->addItem(rect.Height());
  return result;
}

// Returns |layout_object|'s bounding box in document coordinates.
LayoutRect RectInDocument(const LayoutObject* layout_object) {
  LayoutRect rect_in_absolute(layout_object->AbsoluteBoundingBoxFloatRect());
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  // Don't do frame to document coordinate transformation for layout view,
  // whose bounding box is not affected by scroll offset.
  if (local_frame_view && !layout_object->IsLayoutView())
    return local_frame_view->FrameToDocument(rect_in_absolute);
  return rect_in_absolute;
}

LayoutRect TextFragmentRectInDocument(const LayoutObject* layout_object,
                                      const LayoutText::TextBoxInfo& text_box) {
  FloatRect local_coords_text_box_rect(text_box.local_rect);
  LayoutRect absolute_coords_text_box_rect(
      layout_object->LocalToAbsoluteQuad(local_coords_text_box_rect)
          .BoundingBox());
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view
             ? local_frame_view->FrameToDocument(absolute_coords_text_box_rect)
             : absolute_coords_text_box_rect;
}

Document* GetEmbeddedDocument(PaintLayer* layer) {
  // Documents are embedded on their own PaintLayer via a LayoutEmbeddedContent.
  if (layer->GetLayoutObject().IsLayoutEmbeddedContent()) {
    FrameView* frame_view =
        ToLayoutEmbeddedContent(layer->GetLayoutObject()).ChildFrameView();
    if (frame_view && frame_view->IsLocalFrameView()) {
      LocalFrameView* local_frame_view = ToLocalFrameView(frame_view);
      return local_frame_view->GetFrame().GetDocument();
    }
  }
  return nullptr;
}

std::unique_ptr<protocol::DOMSnapshot::RareStringData> StringData() {
  return protocol::DOMSnapshot::RareStringData::create()
      .setIndex(protocol::Array<int>::create())
      .setValue(protocol::Array<int>::create())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareIntegerData> IntegerData() {
  return protocol::DOMSnapshot::RareIntegerData::create()
      .setIndex(protocol::Array<int>::create())
      .setValue(protocol::Array<int>::create())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareBooleanData> BooleanData() {
  return protocol::DOMSnapshot::RareBooleanData::create()
      .setIndex(protocol::Array<int>::create())
      .build();
}

}  // namespace

struct InspectorDOMSnapshotAgent::VectorStringHashTraits
    : public WTF::GenericHashTraits<Vector<String>> {
  static unsigned GetHash(const Vector<String>& vec) {
    unsigned h = DefaultHash<size_t>::Hash::GetHash(vec.size());
    for (wtf_size_t i = 0; i < vec.size(); i++) {
      h = WTF::HashInts(h, DefaultHash<String>::Hash::GetHash(vec[i]));
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

  static void ConstructDeletedValue(Vector<String>& vec, bool) {
    new (NotNull, &vec) Vector<String>(WTF::kHashTableDeletedValue);
  }

  static bool IsDeletedValue(const Vector<String>& vec) {
    return vec.IsHashTableDeletedValue();
  }

  static bool IsEmptyValue(const Vector<String>& vec) { return vec.IsEmpty(); }

  static const bool kEmptyValueIsZero = false;
  static const bool safe_to_compare_to_empty_or_deleted = false;
  static const bool kHasIsEmptyValueFunction = true;
};

InspectorDOMSnapshotAgent::InspectorDOMSnapshotAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMDebuggerAgent* dom_debugger_agent)
    : inspected_frames_(inspected_frames),
      dom_debugger_agent_(dom_debugger_agent),
      enabled_(&agent_state_, /*default_value=*/false) {}

InspectorDOMSnapshotAgent::~InspectorDOMSnapshotAgent() = default;

void InspectorDOMSnapshotAgent::GetOriginUrl(String* origin_url_ptr,
                                             const Node* node) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!isolate || !isolate->InContext() || !debugger) {
    origin_url_ptr = nullptr;
    return;
  }
  // First try searching in one frame, since grabbing full trace is
  // expensive.
  auto trace = debugger->GetV8Inspector()->captureStackTrace(false);
  if (!trace) {
    origin_url_ptr = nullptr;
    return;
  }
  if (!trace->firstNonEmptySourceURL().length())
    trace = debugger->GetV8Inspector()->captureStackTrace(true);
  String origin_url = ToCoreString(trace->firstNonEmptySourceURL());
  if (origin_url.IsEmpty()) {
    // Fall back to document url.
    origin_url = node->GetDocument().Url().GetString();
  }
  *origin_url_ptr = origin_url;
}

void InspectorDOMSnapshotAgent::CharacterDataModified(
    CharacterData* character_data) {
  String origin_url;
  GetOriginUrl(&origin_url, character_data);
  if (origin_url)
    origin_url_map_->insert(DOMNodeIds::IdForNode(character_data), origin_url);
}

void InspectorDOMSnapshotAgent::DidInsertDOMNode(Node* node) {
  String origin_url;
  GetOriginUrl(&origin_url, node);
  if (origin_url)
    origin_url_map_->insert(DOMNodeIds::IdForNode(node), origin_url);
}

void InspectorDOMSnapshotAgent::EnableAndReset() {
  enabled_.Set(true);
  origin_url_map_ = std::make_unique<OriginUrlMap>();
  instrumenting_agents_->addInspectorDOMSnapshotAgent(this);
}

void InspectorDOMSnapshotAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

Response InspectorDOMSnapshotAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return Response::OK();
}

Response InspectorDOMSnapshotAgent::disable() {
  if (!enabled_.Get())
    return Response::Error("DOM snapshot agent hasn't been enabled.");
  enabled_.Clear();
  origin_url_map_.reset();
  instrumenting_agents_->removeInspectorDOMSnapshotAgent(this);
  return Response::OK();
}

Response InspectorDOMSnapshotAgent::getSnapshot(
    std::unique_ptr<protocol::Array<String>> style_whitelist,
    protocol::Maybe<bool> include_event_listeners,
    protocol::Maybe<bool> include_paint_order,
    protocol::Maybe<bool> include_user_agent_shadow_tree,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>* dom_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
        layout_tree_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
        computed_styles) {
  Document* document = inspected_frames_->Root()->GetDocument();
  if (!document)
    return Response::Error("Document is not available");

  // Setup snapshot.
  dom_nodes_ = protocol::Array<protocol::DOMSnapshot::DOMNode>::create();
  layout_tree_nodes_ =
      protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>::create();
  computed_styles_ =
      protocol::Array<protocol::DOMSnapshot::ComputedStyle>::create();
  computed_styles_map_ = std::make_unique<ComputedStylesMap>();
  css_property_whitelist_ = std::make_unique<CSSPropertyWhitelist>();

  // Look up the CSSPropertyIDs for each entry in |style_whitelist|.
  for (wtf_size_t i = 0; i < style_whitelist->length(); i++) {
    CSSPropertyID property_id = cssPropertyID(style_whitelist->get(i));
    if (property_id == CSSPropertyInvalid)
      continue;
    css_property_whitelist_->push_back(
        std::make_pair(style_whitelist->get(i), property_id));
  }

  if (include_paint_order.fromMaybe(false)) {
    paint_order_map_ = std::make_unique<PaintOrderMap>();
    next_paint_order_index_ = 0;
    TraversePaintLayerTree(document);
  }

  // Actual traversal.
  VisitNode(document, include_event_listeners.fromMaybe(false),
            include_user_agent_shadow_tree.fromMaybe(false));

  // Extract results from state and reset.
  *dom_nodes = std::move(dom_nodes_);
  *layout_tree_nodes = std::move(layout_tree_nodes_);
  *computed_styles = std::move(computed_styles_);
  computed_styles_map_.reset();
  css_property_whitelist_.reset();
  paint_order_map_.reset();
  return Response::OK();
}

protocol::Response InspectorDOMSnapshotAgent::captureSnapshot(
    std::unique_ptr<protocol::Array<String>> computed_styles,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>*
        documents,
    std::unique_ptr<protocol::Array<String>>* strings) {
  strings_ = protocol::Array<String>::create();
  documents_ =
      protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>::create();

  css_property_whitelist_ = std::make_unique<CSSPropertyWhitelist>();
  // Look up the CSSPropertyIDs for each entry in |computed_styles|.
  for (size_t i = 0; i < computed_styles->length(); i++) {
    CSSPropertyID property_id = cssPropertyID(computed_styles->get(i));
    if (property_id == CSSPropertyInvalid)
      continue;
    css_property_whitelist_->push_back(
        std::make_pair(computed_styles->get(i), property_id));
  }

  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      document_order_map_.Set(document, document_order_map_.size());
  }
  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      VisitDocument2(document);
  }

  // Extract results from state and reset.
  *documents = std::move(documents_);
  *strings = std::move(strings_);
  css_property_whitelist_.reset();
  string_table_.clear();
  document_order_map_.clear();
  documents_.reset();
  return Response::OK();
}

int InspectorDOMSnapshotAgent::VisitNode(Node* node,
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
    if (!node->parentNode() || origin_url_map_->at(DOMNodeIds::IdForNode(
                                   node->parentNode())) != origin_url) {
      owned_value->setOriginURL(
          origin_url_map_->at(owned_value->getBackendNodeId()));
    }
  }
  protocol::DOMSnapshot::DOMNode* value = owned_value.get();
  int index = static_cast<int>(dom_nodes_->length());
  dom_nodes_->addItem(std::move(owned_value));

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
      if (!event_information.IsEmpty()) {
        value->setEventListeners(
            dom_debugger_agent_->BuildObjectsForEventListeners(
                event_information, context, v8_inspector::StringView()));
      }
    }
  }

  if (node->IsElementNode()) {
    Element* element = ToElement(node);
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (node->IsFrameOwnerElement()) {
      const HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
      if (LocalFrame* frame =
              frame_owner->ContentFrame() &&
                      frame_owner->ContentFrame()->IsLocalFrame()
                  ? ToLocalFrame(frame_owner->ContentFrame())
                  : nullptr) {
        value->setFrameId(IdentifiersFactory::FrameId(frame));
      }
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

    if (auto* textarea_element = ToHTMLTextAreaElementOrNull(*element))
      value->setTextValue(textarea_element->value());

    if (auto* input_element = ToHTMLInputElementOrNull(*element)) {
      value->setInputValue(input_element->value());
      if ((input_element->type() == InputTypeNames::radio) ||
          (input_element->type() == InputTypeNames::checkbox)) {
        value->setInputChecked(input_element->checked());
      }
    }

    if (auto* option_element = ToHTMLOptionElementOrNull(*element))
      value->setOptionSelected(option_element->Selected());

    if (element->GetPseudoId()) {
      protocol::DOM::PseudoType pseudo_type;
      if (InspectorDOMAgent::GetPseudoElementType(element->GetPseudoId(),
                                                  &pseudo_type)) {
        value->setPseudoType(pseudo_type);
        if (node->GetLayoutObject())
          VisitPseudoLayoutChildren(node, index);
      }
    } else {
      value->setPseudoElementIndexes(
          VisitPseudoElements(element, index, include_event_listeners,
                              include_user_agent_shadow_tree));
    }

    HTMLImageElement* image_element = ToHTMLImageElementOrNull(node);
    if (image_element)
      value->setCurrentSourceURL(image_element->currentSrc());
  } else if (auto* document = DynamicTo<Document>(node)) {
    value->setDocumentURL(InspectorDOMAgent::DocumentURLString(document));
    value->setBaseURL(InspectorDOMAgent::DocumentBaseURLString(document));
    if (document->ContentLanguage())
      value->setContentLanguage(document->ContentLanguage().Utf8().data());
    if (document->EncodingName())
      value->setDocumentEncoding(document->EncodingName().Utf8().data());
    value->setFrameId(IdentifiersFactory::FrameId(document->GetFrame()));
    if (document->View() && document->View()->LayoutViewport()) {
      auto offset = document->View()->LayoutViewport()->GetScrollOffset();
      value->setScrollOffsetX(offset.Width());
      value->setScrollOffsetY(offset.Height());
    }
  } else if (node->IsDocumentTypeNode()) {
    DocumentType* doc_type = ToDocumentType(node);
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

int InspectorDOMSnapshotAgent::AddString(const String& string) {
  if (string.IsEmpty())
    return -1;
  auto it = string_table_.find(string);
  int index;
  if (it == string_table_.end()) {
    index = static_cast<int>(strings_->length());
    strings_->addItem(string);
    string_table_.Set(string, index);
  } else {
    index = it->value;
  }
  return index;
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareIntegerData* data,
    int index,
    int value) {
  data->getIndex()->addItem(index);
  data->getValue()->addItem(value);
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareStringData* data,
    int index,
    const String& value) {
  data->getIndex()->addItem(index);
  data->getValue()->addItem(AddString(value));
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareBooleanData* data,
    int index) {
  data->getIndex()->addItem(index);
}

void InspectorDOMSnapshotAgent::VisitDocument2(Document* document) {
  // Update layout tree before traversal of document so that we inspect a
  // current and consistent state of all trees. No need to do this if paint
  // order was calculated, since layout trees were already updated during
  // TraversePaintLayerTree().
  document->UpdateStyleAndLayoutTree();
  DocumentType* doc_type = document->doctype();

  document_ =
      protocol::DOMSnapshot::DocumentSnapshot::create()
          .setDocumentURL(
              AddString(InspectorDOMAgent::DocumentURLString(document)))
          .setBaseURL(
              AddString(InspectorDOMAgent::DocumentBaseURLString(document)))
          .setContentLanguage(AddString(document->ContentLanguage()))
          .setEncodingName(AddString(document->EncodingName()))
          .setFrameId(
              AddString(IdentifiersFactory::FrameId(document->GetFrame())))
          .setPublicId(AddString(doc_type ? doc_type->publicId() : String()))
          .setSystemId(AddString(doc_type ? doc_type->systemId() : String()))
          .setNodes(protocol::DOMSnapshot::NodeTreeSnapshot::create()
                        .setParentIndex(protocol::Array<int>::create())
                        .setNodeType(protocol::Array<int>::create())
                        .setNodeName(protocol::Array<int>::create())
                        .setNodeValue(protocol::Array<int>::create())
                        .setBackendNodeId(protocol::Array<int>::create())
                        .setAttributes(
                            protocol::Array<protocol::Array<int>>::create())
                        .setTextValue(StringData())
                        .setInputValue(StringData())
                        .setInputChecked(BooleanData())
                        .setOptionSelected(BooleanData())
                        .setContentDocumentIndex(IntegerData())
                        .setPseudoType(StringData())
                        .setIsClickable(BooleanData())
                        .setCurrentSourceURL(StringData())
                        .setOriginURL(StringData())
                        .build())
          .setLayout(
              protocol::DOMSnapshot::LayoutTreeSnapshot::create()
                  .setNodeIndex(protocol::Array<int>::create())
                  .setBounds(protocol::Array<protocol::Array<double>>::create())
                  .setText(protocol::Array<int>::create())
                  .setStyles(protocol::Array<protocol::Array<int>>::create())
                  .setStackingContexts(BooleanData())
                  .build())
          .setTextBoxes(
              protocol::DOMSnapshot::TextBoxSnapshot::create()
                  .setLayoutIndex(protocol::Array<int>::create())
                  .setBounds(protocol::Array<protocol::Array<double>>::create())
                  .setStart(protocol::Array<int>::create())
                  .setLength(protocol::Array<int>::create())
                  .build())
          .build();

  if (document->View() && document->View()->LayoutViewport()) {
    auto offset = document->View()->LayoutViewport()->GetScrollOffset();
    document_->setScrollOffsetX(offset.Width());
    document_->setScrollOffsetY(offset.Height());
  }

  VisitNode2(document, -1);
  documents_->addItem(std::move(document_));
}

int InspectorDOMSnapshotAgent::VisitNode2(Node* node, int parent_index) {
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

  auto* nodes = document_->getNodes();
  int index = static_cast<int>(nodes->getNodeName(nullptr)->length());
  DOMNodeId backend_node_id = DOMNodeIds::IdForNode(node);

  // Create DOMNode object and add it to the result array before traversing
  // children, so that parents appear before their children in the array.
  nodes->getParentIndex(nullptr)->addItem(parent_index);

  nodes->getNodeType(nullptr)->addItem(static_cast<int>(node->getNodeType()));
  nodes->getNodeName(nullptr)->addItem(AddString(node->nodeName()));
  nodes->getNodeValue(nullptr)->addItem(AddString(node_value));
  nodes->getBackendNodeId(nullptr)->addItem(
      IdentifiersFactory::IntIdForNode(node));
  nodes->getAttributes(nullptr)->addItem(BuildArrayForElementAttributes2(node));
  BuildLayoutTreeNode(node->GetLayoutObject(), node, index);

  if (origin_url_map_ && origin_url_map_->Contains(backend_node_id)) {
    String origin_url = origin_url_map_->at(backend_node_id);
    // In common cases, it is implicit that a child node would have the same
    // origin url as its parent, so no need to mark twice.
    if (!node->parentNode() || origin_url_map_->at(DOMNodeIds::IdForNode(
                                   node->parentNode())) != origin_url) {
      SetRare(nodes->getOriginURL(nullptr), index, origin_url);
    }
  }

  if (node->WillRespondToMouseClickEvents())
    SetRare(nodes->getIsClickable(nullptr), index);

  if (node->IsElementNode()) {
    Element* element = ToElement(node);
    if (node->IsFrameOwnerElement()) {
      const HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
      if (Document* doc = frame_owner->contentDocument()) {
        SetRare(nodes->getContentDocumentIndex(nullptr), index,
                document_order_map_.at(doc));
      }
    }

    if (auto* textarea_element = ToHTMLTextAreaElementOrNull(*element)) {
      SetRare(nodes->getTextValue(nullptr), index, textarea_element->value());
    }

    if (auto* input_element = ToHTMLInputElementOrNull(*element)) {
      SetRare(nodes->getInputValue(nullptr), index, input_element->value());
      if ((input_element->type() == InputTypeNames::radio) ||
          (input_element->type() == InputTypeNames::checkbox)) {
        if (input_element->checked()) {
          SetRare(nodes->getInputChecked(nullptr), index);
        }
      }
    }

    if (auto* option_element = ToHTMLOptionElementOrNull(*element)) {
      if (option_element->Selected()) {
        SetRare(nodes->getOptionSelected(nullptr), index);
      }
    }

    if (element->GetPseudoId()) {
      protocol::DOM::PseudoType pseudo_type;
      if (InspectorDOMAgent::GetPseudoElementType(element->GetPseudoId(),
                                                  &pseudo_type)) {
        SetRare(nodes->getPseudoType(nullptr), index, pseudo_type);
        if (node->GetLayoutObject())
          VisitPseudoLayoutChildren2(node, index);
      }
    } else {
      VisitPseudoElements2(element, index);
    }

    HTMLImageElement* image_element = ToHTMLImageElementOrNull(node);
    if (image_element) {
      SetRare(nodes->getCurrentSourceURL(nullptr), index,
              image_element->currentSrc());
    }
  }
  if (node->IsContainerNode())
    VisitContainerChildren2(node, index);
  return index;
}

Node* InspectorDOMSnapshotAgent::FirstChild(
    const Node& node,
    bool include_user_agent_shadow_tree) {
  DCHECK(include_user_agent_shadow_tree || !node.IsInUserAgentShadowRoot());
  if (!include_user_agent_shadow_tree) {
    ShadowRoot* shadow_root = node.GetShadowRoot();
    if (shadow_root && shadow_root->GetType() == ShadowRootType::kUserAgent) {
      Node* child = node.firstChild();
      while (child && !child->CanParticipateInFlatTree())
        child = child->nextSibling();
      return child;
    }
  }
  return FlatTreeTraversal::FirstChild(node);
}

bool InspectorDOMSnapshotAgent::HasChildren(
    const Node& node,
    bool include_user_agent_shadow_tree) {
  return FirstChild(node, include_user_agent_shadow_tree);
}

Node* InspectorDOMSnapshotAgent::NextSibling(
    const Node& node,
    bool include_user_agent_shadow_tree) {
  DCHECK(include_user_agent_shadow_tree || !node.IsInUserAgentShadowRoot());
  if (!include_user_agent_shadow_tree) {
    if (node.ParentElementShadowRoot() &&
        node.ParentElementShadowRoot()->GetType() ==
            ShadowRootType::kUserAgent) {
      Node* sibling = node.nextSibling();
      while (sibling && !sibling->CanParticipateInFlatTree())
        sibling = sibling->nextSibling();
      return sibling;
    }
  }
  return FlatTreeTraversal::NextSibling(node);
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::VisitContainerChildren(
    Node* container,
    bool include_event_listeners,
    bool include_user_agent_shadow_tree) {
  auto children = protocol::Array<int>::create();

  if (!HasChildren(*container, include_user_agent_shadow_tree))
    return nullptr;

  Node* child = FirstChild(*container, include_user_agent_shadow_tree);
  while (child) {
    children->addItem(VisitNode(child, include_event_listeners,
                                include_user_agent_shadow_tree));
    child = NextSibling(*child, include_user_agent_shadow_tree);
  }

  return children;
}

void InspectorDOMSnapshotAgent::VisitContainerChildren2(Node* container,
                                                        int parent_index) {
  if (!HasChildren(*container, false))
    return;

  for (Node* child = FirstChild(*container, false); child;
       child = NextSibling(*child, false)) {
    VisitNode2(child, parent_index);
  }
}

void InspectorDOMSnapshotAgent::VisitPseudoLayoutChildren(Node* pseudo_node,
                                                          int index) {
  for (LayoutObject* child = pseudo_node->GetLayoutObject()->SlowFirstChild();
       child; child = child->NextSibling()) {
    if (child->IsAnonymous())
      VisitLayoutTreeNode(child, pseudo_node, index);
  }
}

void InspectorDOMSnapshotAgent::VisitPseudoLayoutChildren2(Node* pseudo_node,
                                                           int index) {
  for (LayoutObject* child = pseudo_node->GetLayoutObject()->SlowFirstChild();
       child; child = child->NextSibling()) {
    if (child->IsAnonymous())
      BuildLayoutTreeNode(child, pseudo_node, index);
  }
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::VisitPseudoElements(
    Element* parent,
    int index,
    bool include_event_listeners,
    bool include_user_agent_shadow_tree) {
  if (!parent->GetPseudoElement(kPseudoIdFirstLetter) &&
      !parent->GetPseudoElement(kPseudoIdBefore) &&
      !parent->GetPseudoElement(kPseudoIdAfter)) {
    return nullptr;
  }

  auto pseudo_elements = protocol::Array<int>::create();
  for (PseudoId pseudo_id :
       {kPseudoIdFirstLetter, kPseudoIdBefore, kPseudoIdAfter}) {
    if (Node* pseudo_node = parent->GetPseudoElement(pseudo_id)) {
      pseudo_elements->addItem(VisitNode(pseudo_node, include_event_listeners,
                                         include_user_agent_shadow_tree));
    }
  }
  return pseudo_elements;
}

void InspectorDOMSnapshotAgent::VisitPseudoElements2(Element* parent,
                                                     int parent_index) {
  for (PseudoId pseudo_id :
       {kPseudoIdFirstLetter, kPseudoIdBefore, kPseudoIdAfter}) {
    if (Node* pseudo_node = parent->GetPseudoElement(pseudo_id))
      VisitNode2(pseudo_node, parent_index);
  }
}

std::unique_ptr<protocol::Array<protocol::DOMSnapshot::NameValue>>
InspectorDOMSnapshotAgent::BuildArrayForElementAttributes(Element* element) {
  auto attributes_value =
      protocol::Array<protocol::DOMSnapshot::NameValue>::create();
  AttributeCollection attributes = element->Attributes();
  for (const auto& attribute : attributes) {
    attributes_value->addItem(protocol::DOMSnapshot::NameValue::create()
                                  .setName(attribute.GetName().ToString())
                                  .setValue(attribute.Value())
                                  .build());
  }
  if (attributes_value->length() == 0)
    return nullptr;
  return attributes_value;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildArrayForElementAttributes2(Node* node) {
  auto result = protocol::Array<int>::create();
  if (!node->IsElementNode())
    return result;
  AttributeCollection attributes = ToElement(node)->Attributes();
  for (const auto& attribute : attributes) {
    result->addItem(AddString(attribute.GetName().ToString()));
    result->addItem(AddString(attribute.Value()));
  }
  return result;
}

int InspectorDOMSnapshotAgent::VisitLayoutTreeNode(LayoutObject* layout_object,
                                                   Node* node,
                                                   int node_index) {
  if (!layout_object)
    return -1;

  auto layout_tree_node =
      protocol::DOMSnapshot::LayoutTreeNode::create()
          .setDomNodeIndex(node_index)
          .setBoundingBox(BuildRectForLayoutRect(RectInDocument(layout_object)))
          .build();

  int style_index = GetStyleIndexForNode(node);
  if (style_index != -1)
    layout_tree_node->setStyleIndex(style_index);

  if (layout_object->Style() && layout_object->Style()->IsStackingContext())
    layout_tree_node->setIsStackingContext(true);

  if (paint_order_map_) {
    PaintLayer* paint_layer = layout_object->EnclosingLayer();

    // We visited all PaintLayers when building |paint_order_map_|.
    DCHECK(paint_order_map_->Contains(paint_layer));

    if (int paint_order = paint_order_map_->at(paint_layer))
      layout_tree_node->setPaintOrder(paint_order);
  }

  if (layout_object->IsText()) {
    LayoutText* layout_text = ToLayoutText(layout_object);
    layout_tree_node->setLayoutText(layout_text->GetText());
    Vector<LayoutText::TextBoxInfo> text_boxes = layout_text->GetTextBoxInfo();
    if (!text_boxes.IsEmpty()) {
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::InlineTextBox>>
          inline_text_nodes =
              protocol::Array<protocol::DOMSnapshot::InlineTextBox>::create();
      for (const auto& text_box : text_boxes) {
        inline_text_nodes->addItem(
            protocol::DOMSnapshot::InlineTextBox::create()
                .setStartCharacterIndex(text_box.dom_start_offset)
                .setNumCharacters(text_box.dom_length)
                .setBoundingBox(BuildRectForLayoutRect(
                    TextFragmentRectInDocument(layout_object, text_box)))
                .build());
      }
      layout_tree_node->setInlineTextNodes(std::move(inline_text_nodes));
    }
  }

  int index = static_cast<int>(layout_tree_nodes_->length());
  layout_tree_nodes_->addItem(std::move(layout_tree_node));
  return index;
}

int InspectorDOMSnapshotAgent::BuildLayoutTreeNode(LayoutObject* layout_object,
                                                   Node* node,
                                                   int node_index) {
  if (!layout_object)
    return -1;
  auto* layout_tree_snapshot = document_->getLayout();
  auto* text_box_snapshot = document_->getTextBoxes();

  int layout_index =
      static_cast<int>(layout_tree_snapshot->getNodeIndex()->length());
  layout_tree_snapshot->getNodeIndex()->addItem(node_index);
  layout_tree_snapshot->getStyles()->addItem(BuildStylesForNode(node));
  layout_tree_snapshot->getBounds()->addItem(
      BuildRectForLayoutRect2(RectInDocument(layout_object)));

  if (layout_object->Style() && layout_object->Style()->IsStackingContext())
    SetRare(layout_tree_snapshot->getStackingContexts(), layout_index);

  String text = layout_object->IsText() ? ToLayoutText(layout_object)->GetText()
                                        : String();
  layout_tree_snapshot->getText()->addItem(AddString(text));

  if (!layout_object->IsText())
    return layout_index;

  LayoutText* layout_text = ToLayoutText(layout_object);
  Vector<LayoutText::TextBoxInfo> text_boxes = layout_text->GetTextBoxInfo();
  if (text_boxes.IsEmpty())
    return layout_index;

  for (const auto& text_box : text_boxes) {
    text_box_snapshot->getLayoutIndex()->addItem(layout_index);
    text_box_snapshot->getBounds()->addItem(BuildRectForLayoutRect2(
        TextFragmentRectInDocument(layout_object, text_box)));
    text_box_snapshot->getStart()->addItem(text_box.dom_start_offset);
    text_box_snapshot->getLength()->addItem(text_box.dom_length);
  }

  return layout_index;
}

int InspectorDOMSnapshotAgent::GetStyleIndexForNode(Node* node) {
  CSSComputedStyleDeclaration* computed_style_info =
      CSSComputedStyleDeclaration::Create(node, true);

  Vector<String> style;
  bool all_properties_empty = true;
  for (const auto& pair : *css_property_whitelist_) {
    String value = computed_style_info->GetPropertyValue(pair.second);
    if (!value.IsEmpty())
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
      protocol::Array<protocol::DOMSnapshot::NameValue>::create();

  for (wtf_size_t i = 0; i < style.size(); i++) {
    if (style[i].IsEmpty())
      continue;
    style_properties->addItem(protocol::DOMSnapshot::NameValue::create()
                                  .setName((*css_property_whitelist_)[i].first)
                                  .setValue(style[i])
                                  .build());
  }

  wtf_size_t index = static_cast<wtf_size_t>(computed_styles_->length());
  computed_styles_->addItem(protocol::DOMSnapshot::ComputedStyle::create()
                                .setProperties(std::move(style_properties))
                                .build());
  computed_styles_map_->insert(std::move(style), index);
  return index;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildStylesForNode(Node* node) {
  CSSComputedStyleDeclaration* computed_style_info =
      CSSComputedStyleDeclaration::Create(node, true);
  std::unique_ptr<protocol::Array<int>> result = protocol::Array<int>::create();
  for (const auto& pair : *css_property_whitelist_) {
    String value = computed_style_info->GetPropertyValue(pair.second);
    result->addItem(AddString(value));
  }
  return result;
}

void InspectorDOMSnapshotAgent::TraversePaintLayerTree(Document* document) {
  // Update layout tree before traversal of document so that we inspect a
  // current and consistent state of all trees.
  document->UpdateStyleAndLayoutTree();

  PaintLayer* root_layer = document->GetLayoutView()->Layer();
  // LayoutView requires a PaintLayer.
  DCHECK(root_layer);

  VisitPaintLayer(root_layer);
}

void InspectorDOMSnapshotAgent::VisitPaintLayer(PaintLayer* layer) {
  DCHECK(!paint_order_map_->Contains(layer));

  paint_order_map_->Set(layer, next_paint_order_index_);
  next_paint_order_index_++;

  // If there is an embedded document, integrate it into the painting order.
  Document* embedded_document = GetEmbeddedDocument(layer);
  if (embedded_document)
    TraversePaintLayerTree(embedded_document);

  // If there's an embedded document, there shouldn't be any children.
  DCHECK(!embedded_document || !layer->FirstChild());

  if (!embedded_document) {
    if (PaintLayerStackingNode* node = layer->StackingNode()) {
      PaintLayerStackingNodeIterator iterator(*node, kAllChildren);
      while (PaintLayer* child_layer = iterator.Next()) {
        VisitPaintLayer(child_layer);
      }
    }
  }
}

void InspectorDOMSnapshotAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_debugger_agent_);
  visitor->Trace(document_order_map_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
