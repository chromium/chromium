// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/attribute_collection.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
#include "third_party/blink/renderer/core/inspector/inspector_contrast.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/legacy_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8-inspector.h"

namespace blink {

using mojom::blink::FormControlType;
using protocol::Maybe;

namespace {

std::unique_ptr<protocol::Array<double>> BuildRectForPhysicalRect(
    const PhysicalRect& rect) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {rect.X(), rect.Y(), rect.Width(), rect.Height()});
}

std::unique_ptr<protocol::Array<double>> BuildRectForLayout(const int x,
                                                            const int y,
                                                            const int width,
                                                            const int height) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {static_cast<double>(x), static_cast<double>(y),
       static_cast<double>(width), static_cast<double>(height)});
}

Document* GetEmbeddedDocument(PaintLayer* layer) {
  // Documents are embedded on their own PaintLayer via a LayoutEmbeddedContent.
  if (auto* embedded =
          DynamicTo<LayoutEmbeddedContent>(layer->GetLayoutObject())) {
    FrameView* frame_view = embedded->ChildFrameView();
    if (auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view))
      return local_frame_view->GetFrame().GetDocument();
  }
  return nullptr;
}

std::unique_ptr<protocol::DOMSnapshot::RareStringData> StringData() {
  return protocol::DOMSnapshot::RareStringData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .setValue(std::make_unique<protocol::Array<int>>())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareIntegerData> IntegerData() {
  return protocol::DOMSnapshot::RareIntegerData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .setValue(std::make_unique<protocol::Array<int>>())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareBooleanData> BooleanData() {
  return protocol::DOMSnapshot::RareBooleanData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .build();
}

String GetOriginUrl(const Node* node) {
  v8::Isolate* isolate = node->GetDocument().GetAgent().isolate();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!isolate || !isolate->InContext() || !debugger)
    return String();
  v8::HandleScope handleScope(isolate);
  String url = GetCurrentScriptUrl(isolate);
  if (!url.empty())
    return url;
  // If we did not get anything from the sync stack, let's try the slow
  // way that also checks async stacks.
  auto trace = debugger->GetV8Inspector()->captureStackTrace(true);
  if (trace)
    url = ToCoreString(trace->firstNonEmptySourceURL());
  if (!url.empty())
    return url;
  // Fall back to document url.
  return node->GetDocument().Url().GetString();
}

class DOMTreeIterator {
  STACK_ALLOCATED();

 public:
  DOMTreeIterator(Node* root, int root_node_id)
      : current_(root), path_to_current_node_({root_node_id}) {
    DCHECK(current_);
  }

  void Advance(int next_node_id) {
    DCHECK(current_);
    const bool skip_shadow_root =
        current_->GetShadowRoot() && current_->GetShadowRoot()->IsUserAgent();
    if (Node* first_child = skip_shadow_root
                                ? current_->firstChild()
                                : FlatTreeTraversal::FirstChild(*current_)) {
      current_ = first_child;
      path_to_current_node_.push_back(next_node_id);
      return;
    }
    // No children, let's try siblings, then ancestor siblings.
    while (current_) {
      const bool in_ua_shadow_tree =
          current_->ParentElementShadowRoot() &&
          current_->ParentElementShadowRoot()->IsUserAgent();
      if (Node* node = in_ua_shadow_tree
                           ? current_->nextSibling()
                           : FlatTreeTraversal::NextSibling(*current_)) {
        path_to_current_node_.back() = next_node_id;
        current_ = node;
        return;
      }
      current_ = in_ua_shadow_tree ? current_->parentNode()
                                   : FlatTreeTraversal::Parent(*current_);
      path_to_current_node_.pop_back();
    }
    DCHECK(path_to_current_node_.empty());
  }

  Node* CurrentNode() const { return current_; }

  int ParentNodeId() const {
    return path_to_current_node_.size() > 1
               ? *(path_to_current_node_.rbegin() + 1)
               : -1;
  }

 private:
  Node* current_;
  WTF::Vector<int> path_to_current_node_;
};

}  // namespace

// Returns |layout_object|'s bounding box in document coordinates.
// static
PhysicalRect InspectorDOMSnapshotAgent::RectInDocument(
    const LayoutObject* layout_object) {
  PhysicalRect rect_in_absolute =
      PhysicalRect::EnclosingRect(layout_object->AbsoluteBoundingBoxRectF());
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  // Don't do frame to document coordinate transformation for layout view,
  // whose bounding box is not affected by scroll offset.
  if (local_frame_view && !IsA<LayoutView>(layout_object))
    return local_frame_view->FrameToDocument(rect_in_absolute);
  return rect_in_absolute;
}

// static
PhysicalRect InspectorDOMSnapshotAgent::TextFragmentRectInDocument(
    const LayoutObject* layout_object,
    const LayoutText::TextBoxInfo& text_box) {
  PhysicalRect absolute_coords_text_box_rect =
      layout_object->LocalToAbsoluteRect(text_box.local_rect);
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view
             ? local_frame_view->FrameToDocument(absolute_coords_text_box_rect)
             : absolute_coords_text_box_rect;
}

InspectorDOMSnapshotAgent::InspectorDOMSnapshotAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMDebuggerAgent* dom_debugger_agent)
    : inspected_frames_(inspected_frames),
      dom_debugger_agent_(dom_debugger_agent),
      enabled_(&agent_state_, /*default_value=*/false) {
  DCHECK(dom_debugger_agent);
}

InspectorDOMSnapshotAgent::~InspectorDOMSnapshotAgent() = default;

void InspectorDOMSnapshotAgent::CharacterDataModified(
    CharacterData* character_data) {
  String origin_url = GetOriginUrl(character_data);
  if (origin_url)
    origin_url_map_->insert(character_data->GetDomNodeId(), origin_url);
}

void InspectorDOMSnapshotAgent::DidInsertDOMNode(Node* node) {
  String origin_url = GetOriginUrl(node);
  if (origin_url)
    origin_url_map_->insert(node->GetDomNodeId(), origin_url);
}

void InspectorDOMSnapshotAgent::EnableAndReset() {
  enabled_.Set(true);
  origin_url_map_ = std::make_unique<OriginUrlMap>();
  instrumenting_agents_->AddInspectorDOMSnapshotAgent(this);
}

void InspectorDOMSnapshotAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

protocol::Response InspectorDOMSnapshotAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return protocol::Response::Success();
}

protocol::Response InspectorDOMSnapshotAgent::disable() {
  if (!enabled_.Get()) {
    return protocol::Response::ServerError(
        "DOM snapshot agent hasn't been enabled.");
  }
  enabled_.Clear();
  origin_url_map_.reset();
  instrumenting_agents_->RemoveInspectorDOMSnapshotAgent(this);
  return protocol::Response::Success();
}

protocol::Response InspectorDOMSnapshotAgent::getSnapshot(
    std::unique_ptr<protocol::Array<String>> style_filter,
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
    return protocol::Response::ServerError("Document is not available");
  LegacyDOMSnapshotAgent legacySupport(dom_debugger_agent_,
                                       origin_url_map_.get());
  return legacySupport.GetSnapshot(
      document, std::move(style_filter), std::move(include_event_listeners),
      std::move(include_paint_order), std::move(include_user_agent_shadow_tree),
      dom_nodes, layout_tree_nodes, computed_styles);
}

protocol::Response InspectorDOMSnapshotAgent::captureSnapshot(
    std::unique_ptr<protocol::Array<String>> computed_styles,
    protocol::Maybe<bool> include_paint_order,
    protocol::Maybe<bool> include_dom_rects,
    protocol::Maybe<bool> include_blended_background_colors,
    protocol::Maybe<bool> include_text_color_opacities,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>*
        documents,
    std::unique_ptr<protocol::Array<String>>* strings) {
  // This function may kick the layout, but external clients may call this
  // function outside of the layout phase.
  FontCachePurgePreventer fontCachePurgePreventer;

  auto* main_window = inspected_frames_->Root()->DomWindow();
  if (!main_window)
    return protocol::Response::ServerError("Document is not available");

  // Update layout before traversal of document so that we inspect a
  // current and consistent state of all trees.
  inspected_frames_->Root()->View()->UpdateLifecycleToCompositingInputsClean(
      DocumentUpdateReason::kInspector);

  strings_ = std::make_unique<protocol::Array<String>>();
  documents_ = std::make_unique<
      protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>();

  css_property_filter_ = std::make_unique<CSSPropertyFilter>();
  // Resolve all property names to CSSProperty references.
  for (String& property_name : *computed_styles) {
    const CSSPropertyID id =
        UnresolvedCSSPropertyID(main_window, property_name);
    if (id == CSSPropertyID::kInvalid || id == CSSPropertyID::kVariable)
      return protocol::Response::InvalidParams("invalid CSS property");
    const auto& property = CSSProperty::Get(ResolveCSSPropertyID(id));
    css_property_filter_->push_back(&property);
  }

  if (include_paint_order.value_or(false)) {
    paint_order_map_ =
        InspectorDOMSnapshotAgent::BuildPaintLayerTree(main_window->document());
  }

  include_snapshot_dom_rects_ = include_dom_rects.value_or(false);
  include_blended_background_colors_ =
      include_blended_background_colors.value_or(false);
  include_text_color_opacities_ = include_text_color_opacities.value_or(false);

  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      document_order_map_.Set(document, document_order_map_.size());
  }
  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      VisitDocument(document);
  }

  // Extract results from state and reset.
  *documents = std::move(documents_);
  *strings = std::move(strings_);
  css_property_filter_.reset();
  paint_order_map_.Clear();
  string_table_.clear();
  document_order_map_.clear();
  documents_.reset();
  css_value_cache_.clear();
  style_cache_.clear();
  return protocol::Response::Success();
}

int InspectorDOMSnapshotAgent::AddString(const String& string) {
  if (string.empty())
    return -1;
  auto it = string_table_.find(string);
  int index;
  if (it == string_table_.end()) {
    index = static_cast<int>(strings_->size());
    strings_->emplace_back(string);
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
  data->getIndex()->emplace_back(index);
  data->getValue()->emplace_back(value);
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareStringData* data,
    int index,
    const String& value) {
  data->getIndex()->emplace_back(index);
  data->getValue()->emplace_back(AddString(value));
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareBooleanData* data,
    int index) {
  data->getIndex()->emplace_back(index);
}

void InspectorDOMSnapshotAgent::VisitDocument(Document* document) {
  DocumentType* doc_type = document->doctype();
  InspectorContrast contrast(document);

  document_ =
      protocol::DOMSnapshot::DocumentSnapshot::create()
          .setDocumentURL(
              AddString(InspectorDOMAgent::DocumentURLString(document)))
          .setBaseURL(
              AddString(InspectorDOMAgent::DocumentBaseURLString(document)))
          .setTitle(AddString(document->title()))
          .setContentLanguage(AddString(document->ContentLanguage()))
          .setEncodingName(AddString(document->EncodingName()))
          .setFrameId(
              AddString(IdentifiersFactory::FrameId(document->GetFrame())))
          .setPublicId(AddString(doc_type ? doc_type->publicId() : String()))
          .setSystemId(AddString(doc_type ? doc_type->systemId() : String()))
          .setNodes(
              protocol::DOMSnapshot::NodeTreeSnapshot::create()
                  .setParentIndex(std::make_unique<protocol::Array<int>>())
                  .setNodeType(std::make_unique<protocol::Array<int>>())
                  .setNodeName(std::make_unique<protocol::Array<int>>())
                  .setShadowRootType(StringData())
                  .setNodeValue(std::make_unique<protocol::Array<int>>())
                  .setBackendNodeId(std::make_unique<protocol::Array<int>>())
                  .setAttributes(
                      std::make_unique<protocol::Array<protocol::Array<int>>>())
                  .setTextValue(StringData())
                  .setInputValue(StringData())
                  .setInputChecked(BooleanData())
                  .setOptionSelected(BooleanData())
                  .setContentDocumentIndex(IntegerData())
                  .setPseudoType(StringData())
                  .setPseudoIdentifier(StringData())
                  .setIsClickable(BooleanData())
                  .setCurrentSourceURL(StringData())
                  .setOriginURL(StringData())
                  .build())
          .setLayout(
              protocol::DOMSnapshot::LayoutTreeSnapshot::create()
                  .setNodeIndex(std::make_unique<protocol::Array<int>>())
                  .setBounds(std::make_unique<
                             protocol::Array<protocol::Array<double>>>())
                  .setText(std::make_unique<protocol::Array<int>>())
                  .setStyles(
                      std::make_unique<protocol::Array<protocol::Array<int>>>())
                  .setStackingContexts(BooleanData())
                  .build())
          .setTextBoxes(
              protocol::DOMSnapshot::TextBoxSnapshot::create()
                  .setLayoutIndex(std::make_unique<protocol::Array<int>>())
                  .setBounds(std::make_unique<
                             protocol::Array<protocol::Array<double>>>())
                  .setStart(std::make_unique<protocol::Array<int>>())
                  .setLength(std::make_unique<protocol::Array<int>>())
                  .build())
          .build();

  if (document->View() && document->View()->LayoutViewport()) {
    auto offset = document->View()->LayoutViewport()->GetScrollOffset();
    document_->setScrollOffsetX(offset.x());
    document_->setScrollOffsetY(offset.y());
    auto contents_size = document->View()->LayoutViewport()->ContentsSize();
    document_->setContentWidth(contents_size.width());
    document_->setContentHeight(contents_size.height());
  }

  if (paint_order_map_) {
    document_->getLayout()->setPaintOrders(
        std::make_unique<protocol::Array<int>>());
  }
  if (include_snapshot_dom_rects_) {
    document_->getLayout()->setOffsetRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
    document_->getLayout()->setClientRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
    document_->getLayout()->setScrollRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
  }

  if (include_blended_background_colors_) {
    document_->getLayout()->setBlendedBackgroundColors(
        std::make_unique<protocol::Array<int>>());
  }
  if (include_text_color_opacities_) {
    document_->getLayout()->setTextColorOpacities(
        std::make_unique<protocol::Array<double>>());
  }

  auto* node_names = document_->getNodes()->getNodeName(nullptr);
  // Note: node_names->size() changes as the loop runs.
  for (DOMTreeIterator it(document,
                          base::checked_cast<int>(node_names->size()));
       it.CurrentNode();
       it.Advance(base::checked_cast<int>(node_names->size()))) {
    DCHECK(!it.CurrentNode()->IsInUserAgentShadowRoot());
    VisitNode(it.CurrentNode(), it.ParentNodeId(), contrast);
  }
  documents_->emplace_back(std::move(document_));
}

void InspectorDOMSnapshotAgent::VisitNode(Node* node,
                                          int parent_index,
                                          InspectorContrast& contrast) {
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
  int index = static_cast<int>(nodes->getNodeName(nullptr)->size());
  DOMNodeId backend_node_id = node->GetDomNodeId();

  // Create DOMNode object and add it to the result array before traversing
  // children, so that parents appear before their children in the array.
  nodes->getParentIndex(nullptr)->emplace_back(parent_index);

  nodes->getNodeType(nullptr)->emplace_back(
      static_cast<int>(node->getNodeType()));
  nodes->getNodeName(nullptr)->emplace_back(AddString(node->nodeName()));
  nodes->getNodeValue(nullptr)->emplace_back(AddString(node_value));
  if (node->IsInShadowTree()) {
    SetRare(nodes->getShadowRootType(nullptr), index,
            InspectorDOMAgent::GetShadowRootType(node->ContainingShadowRoot()));
  }
  nodes->getBackendNodeId(nullptr)->emplace_back(
      IdentifiersFactory::IntIdForNode(node));
  nodes->getAttributes(nullptr)->emplace_back(
      BuildArrayForElementAttributes(node));
  BuildLayoutTreeNode(node->GetLayoutObject(), node, index, contrast);

  if (origin_url_map_ && origin_url_map_->Contains(backend_node_id)) {
    String origin_url = origin_url_map_->at(backend_node_id);
    // In common cases, it is implicit that a child node would have the same
    // origin url as its parent, so no need to mark twice.
    if (!node->parentNode()) {
      SetRare(nodes->getOriginURL(nullptr), index, std::move(origin_url));
    } else {
      DOMNodeId parent_id = node->parentNode()->GetDomNodeId();
      auto it = origin_url_map_->find(parent_id);
      String parent_url = it != origin_url_map_->end() ? it->value : String();
      if (parent_url != origin_url)
        SetRare(nodes->getOriginURL(nullptr), index, std::move(origin_url));
    }
  }

  if (node->WillRespondToMouseClickEvents())
    SetRare(nodes->getIsClickable(nullptr), index);

  auto* element = DynamicTo<Element>(node);
  if (element) {
    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (Document* doc = frame_owner->contentDocument()) {
        auto it = document_order_map_.find(doc);
        if (it != document_order_map_.end())
          SetRare(nodes->getContentDocumentIndex(nullptr), index, it->value);
      }
    }

    if (auto* textarea_element = DynamicTo<HTMLTextAreaElement>(*element)) {
      SetRare(nodes->getTextValue(nullptr), index, textarea_element->Value());
    }

    if (auto* input_element = DynamicTo<HTMLInputElement>(*element)) {
      SetRare(nodes->getInputValue(nullptr), index, input_element->Value());
      if ((input_element->FormControlType() == FormControlType::kInputRadio) ||
          (input_element->FormControlType() ==
           FormControlType::kInputCheckbox)) {
        if (input_element->Checked()) {
          SetRare(nodes->getInputChecked(nullptr), index);
        }
      }
    }

    if (auto* option_element = DynamicTo<HTMLOptionElement>(*element)) {
      if (option_element->Selected()) {
        SetRare(nodes->getOptionSelected(nullptr), index);
      }
    }

    if (element->IsPseudoElement()) {
      SetRare(nodes->getPseudoType(nullptr), index,
              InspectorDOMAgent::ProtocolPseudoElementType(
                  element->GetPseudoIdForStyling()));
      if (auto tag = To<PseudoElement>(element)->view_transition_name()) {
        SetRare(nodes->getPseudoIdentifier(nullptr), index, tag);
      }
    }
    VisitPseudoElements(element, index, contrast);

    auto* image_element = DynamicTo<HTMLImageElement>(node);
    if (image_element) {
      SetRare(nodes->getCurrentSourceURL(nullptr), index,
              image_element->currentSrc());
    }
  }
}

void InspectorDOMSnapshotAgent::VisitPseudoElements(
    Element* parent,
    int parent_index,
    InspectorContrast& contrast) {
  for (PseudoId pseudo_id : {kPseudoIdFirstLetter, kPseudoIdBefore,
                             kPseudoIdAfter, kPseudoIdMarker}) {
    if (Node* pseudo_node = parent->GetPseudoElement(pseudo_id))
      VisitNode(pseudo_node, parent_index, contrast);
  }
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildArrayForElementAttributes(Node* node) {
  auto result = std::make_unique<protocol::Array<int>>();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return result;
  AttributeCollection attributes = element->Attributes();
  for (const auto& attribute : attributes) {
    result->emplace_back(AddString(attribute.GetName().ToString()));
    result->emplace_back(AddString(attribute.Value()));
  }
  return result;
}

int InspectorDOMSnapshotAgent::BuildLayoutTreeNode(
    LayoutObject* layout_object,
    Node* node,
    int node_index,
    InspectorContrast& contrast) {
  if (!layout_object)
    return -1;

  auto* layout_tree_snapshot = document_->getLayout();
  auto* text_box_snapshot = document_->getTextBoxes();

  int layout_index =
      static_cast<int>(layout_tree_snapshot->getNodeIndex()->size());
  layout_tree_snapshot->getNodeIndex()->emplace_back(node_index);
  layout_tree_snapshot->getStyles()->emplace_back(BuildStylesForNode(node));
  layout_tree_snapshot->getBounds()->emplace_back(BuildRectForPhysicalRect(
      InspectorDOMSnapshotAgent::RectInDocument(layout_object)));

  if (include_snapshot_dom_rects_) {
    protocol::Array<protocol::Array<double>>* offsetRects =
        layout_tree_snapshot->getOffsetRects(nullptr);
    DCHECK(offsetRects);

    protocol::Array<protocol::Array<double>>* clientRects =
        layout_tree_snapshot->getClientRects(nullptr);
    DCHECK(clientRects);

    protocol::Array<protocol::Array<double>>* scrollRects =
        layout_tree_snapshot->getScrollRects(nullptr);
    DCHECK(scrollRects);

    if (auto* element = DynamicTo<Element>(node)) {
      offsetRects->emplace_back(
          BuildRectForLayout(element->OffsetLeft(), element->OffsetTop(),
                             element->OffsetWidth(), element->OffsetHeight()));

      clientRects->emplace_back(
          BuildRectForLayout(element->clientLeft(), element->clientTop(),
                             element->clientWidth(), element->clientHeight()));

      scrollRects->emplace_back(
          BuildRectForLayout(element->scrollLeft(), element->scrollTop(),
                             element->scrollWidth(), element->scrollHeight()));
    } else {
      offsetRects->emplace_back(std::make_unique<protocol::Array<double>>());
      clientRects->emplace_back(std::make_unique<protocol::Array<double>>());
      scrollRects->emplace_back(std::make_unique<protocol::Array<double>>());
    }
  }

  if (include_blended_background_colors_ || include_text_color_opacities_) {
    float opacity = 1;
    Vector<Color> colors;
    auto* element = DynamicTo<Element>(node);
    if (element)
      colors = contrast.GetBackgroundColors(element, &opacity);
    if (include_blended_background_colors_) {
      if (colors.size()) {
        layout_tree_snapshot->getBlendedBackgroundColors(nullptr)->emplace_back(
            AddString(colors[0].SerializeAsCSSColor()));
      } else {
        layout_tree_snapshot->getBlendedBackgroundColors(nullptr)->emplace_back(
            -1);
      }
    }
    if (include_text_color_opacities_) {
      layout_tree_snapshot->getTextColorOpacities(nullptr)->emplace_back(
          opacity);
    }
  }

  if (layout_object->IsStackingContext())
    SetRare(layout_tree_snapshot->getStackingContexts(), layout_index);

  if (paint_order_map_) {
    PaintLayer* paint_layer = layout_object->EnclosingLayer();
    const auto paint_order = paint_order_map_->find(paint_layer);
    if (paint_order != paint_order_map_->end()) {
      layout_tree_snapshot->getPaintOrders(nullptr)->emplace_back(
          paint_order->value);
    } else {
      // Previously this returned the empty value if the paint order wasn't
      // found. The empty value for this HashMap is 0, so just pick that here.
      layout_tree_snapshot->getPaintOrders(nullptr)->emplace_back(0);
    }
  }

  String text = layout_object->IsText()
                    ? To<LayoutText>(layout_object)->TransformedText()
                    : String();
  layout_tree_snapshot->getText()->emplace_back(AddString(text));

  if (node->GetPseudoIdForStyling()) {
    // For pseudo elements, visit the children of the layout object.
    // Combinding ::before { content: 'hello' } and ::first-letter would produce
    // two boxes for the ::before node, one for 'hello' and one for 'ello'.
    for (LayoutObject* child = layout_object->SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsAnonymous())
        BuildLayoutTreeNode(child, node, node_index, contrast);
    }
  }

  if (!layout_object->IsText())
    return layout_index;

  auto* layout_text = To<LayoutText>(layout_object);
  Vector<LayoutText::TextBoxInfo> text_boxes = layout_text->GetTextBoxInfo();
  if (text_boxes.empty())
    return layout_index;

  for (const auto& text_box : text_boxes) {
    text_box_snapshot->getLayoutIndex()->emplace_back(layout_index);
    text_box_snapshot->getBounds()->emplace_back(BuildRectForPhysicalRect(
        InspectorDOMSnapshotAgent::TextFragmentRectInDocument(layout_object,
                                                              text_box)));
    text_box_snapshot->getStart()->emplace_back(text_box.dom_start_offset);
    text_box_snapshot->getLength()->emplace_back(text_box.dom_length);
  }

  return layout_index;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildStylesForNode(Node* node) {
  DCHECK(
      !node->GetDocument().NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
          *node));
  auto result = std::make_unique<protocol::Array<int>>();
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    // This doesn't make sense for display:contents elements. They are also
    // rendered, but with no LayoutObject.
    return result;
  }
  Element* element = DynamicTo<Element>(node);
  if (!element) {
    element = FlatTreeTraversal::ParentElement(*node);
  }
  const ComputedStyle* style = element ? element->GetComputedStyle() : nullptr;
  if (!style) {
    return result;
  }
  auto cached_style = style_cache_.find(style);
  if (cached_style != style_cache_.end())
    return std::make_unique<protocol::Array<int>>(*cached_style->value);
  style_cache_.insert(style, result.get());
  result->reserve(css_property_filter_->size());
  for (const auto* property : *css_property_filter_) {
    const CSSValue* value = property->CSSValueFromComputedStyle(
        *style, layout_object, /* allow_visited_style= */ true,
        CSSValuePhase::kResolvedValue);
    if (!value) {
      result->emplace_back(-1);
      continue;
    }
    int index;
    auto it = css_value_cache_.find(value);
    if (it == css_value_cache_.end()) {
      index = AddString(value->CssText());
      css_value_cache_.insert(value, index);
    } else {
      index = it->value;
    }
    result->emplace_back(index);
  }
  return result;
}

// static
InspectorDOMSnapshotAgent::PaintOrderMap*
InspectorDOMSnapshotAgent::BuildPaintLayerTree(Document* document) {
  auto* result = MakeGarbageCollected<PaintOrderMap>();
  TraversePaintLayerTree(document, result);
  return result;
}

// static
void InspectorDOMSnapshotAgent::TraversePaintLayerTree(
    Document* document,
    PaintOrderMap* paint_order_map) {
  PaintLayer* root_layer = document->GetLayoutView()->Layer();
  // LayoutView requires a PaintLayer.
  DCHECK(root_layer);

  VisitPaintLayer(root_layer, paint_order_map);
}

// static
void InspectorDOMSnapshotAgent::VisitPaintLayer(
    PaintLayer* layer,
    PaintOrderMap* paint_order_map) {
  DCHECK(!paint_order_map->Contains(layer));

  paint_order_map->Set(layer, paint_order_map->size());

  Document* embedded_document = GetEmbeddedDocument(layer);
  // If there's an embedded document, there shouldn't be any children.
  DCHECK(!embedded_document || !layer->FirstChild());
  if (embedded_document) {
    TraversePaintLayerTree(embedded_document, paint_order_map);
    return;
  }

  PaintLayerPaintOrderIterator iterator(layer, kAllChildren);
  while (PaintLayer* child_layer = iterator.Next())
    VisitPaintLayer(child_layer, paint_order_map);
}

void InspectorDOMSnapshotAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_debugger_agent_);
  visitor->Trace(paint_order_map_);
  visitor->Trace(document_order_map_);
  visitor->Trace(css_value_cache_);
  visitor->Trace(style_cache_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
