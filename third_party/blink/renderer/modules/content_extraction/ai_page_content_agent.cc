// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_media.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

constexpr MapCoordinatesFlags kMapCoordinatesFlags =
    kTraverseDocumentBoundaries | kApplyRemoteViewportTransform;
constexpr VisualRectFlags kVisualRectFlags = static_cast<VisualRectFlags>(
    kUseGeometryMapper | kVisualRectApplyRemoteViewportTransform);

constexpr float kHeading1FontSizeMultiplier = 2;
constexpr float kHeading3FontSizeMultiplier = 1.17;
constexpr float kHeading5FontSizeMultiplier = 0.83;
constexpr float kHeading6FontSizeMultiplier = 0.67;

// TODO(crbug.com/383128653): This is duplicating logic from
// UnsupportedTagTypeValueForNode, consider reusing it.
bool IsHeadingTag(const HTMLElement& element) {
  return element.HasTagName(html_names::kH1Tag) ||
         element.HasTagName(html_names::kH2Tag) ||
         element.HasTagName(html_names::kH3Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH5Tag) ||
         element.HasTagName(html_names::kH6Tag);
}

mojom::blink::AIPageContentAnchorRel GetAnchorRel(const AtomicString& rel) {
  if (rel == "noopener") {
    return mojom::blink::AIPageContentAnchorRel::kRelationNoOpener;
  } else if (rel == "noreferrer") {
    return mojom::blink::AIPageContentAnchorRel::kRelationNoReferrer;
  } else if (rel == "opener") {
    return mojom::blink::AIPageContentAnchorRel::kRelationOpener;
  } else if (rel == "privacy-policy") {
    return mojom::blink::AIPageContentAnchorRel::kRelationPrivacyPolicy;
  } else if (rel == "terms-of-service") {
    return mojom::blink::AIPageContentAnchorRel::kRelationTermsOfService;
  }
  return mojom::blink::AIPageContentAnchorRel::kRelationUnknown;
}

// Returns the relative text size of the object compared to the document
// default. Ratios are based on browser defaults for headings, which are as
// follows:
//
// Heading 1: 2em
// Heading 2: 1.5em
// Heading 3: 1.17em
// Heading 4: 1em
// Heading 5: 0.83em
// Heading 6: 0.67em
mojom::blink::AIPageContentTextSize GetTextSize(
    const ComputedStyle& style,
    const ComputedStyle& document_style) {
  float font_size_multiplier =
      style.ComputedFontSize() / document_style.ComputedFontSize();
  if (font_size_multiplier >= kHeading1FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kXL;
  } else if (font_size_multiplier >= kHeading3FontSizeMultiplier &&
             font_size_multiplier < kHeading1FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kL;
  } else if (font_size_multiplier >= kHeading5FontSizeMultiplier &&
             font_size_multiplier < kHeading3FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kM;
  } else if (font_size_multiplier >= kHeading6FontSizeMultiplier &&
             font_size_multiplier < kHeading5FontSizeMultiplier) {
    return mojom::blink::AIPageContentTextSize::kS;
  } else {  // font_size_multiplier < kHeading6FontSizeMultiplier
    return mojom::blink::AIPageContentTextSize::kXS;
  }
}

// If the style has a non-normal font weight, has applied text decorations, or
// is a super/subscript, then the text is considered to have emphasis.
bool HasEmphasis(const ComputedStyle& style) {
  return style.GetFontWeight() != kNormalWeightValue ||
         style.GetFontStyle() != kNormalSlopeValue ||
         style.HasAppliedTextDecorations() ||
         style.VerticalAlign() == EVerticalAlign::kSub ||
         style.VerticalAlign() == EVerticalAlign::kSuper;
}

const LayoutIFrame* GetIFrame(const LayoutObject& object) {
  return DynamicTo<LayoutIFrame>(object);
}

bool IsGenericContainer(
    const LayoutObject& object,
    const Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) {
  if (object.Style()->GetPosition() == EPosition::kFixed) {
    return true;
  }

  if (object.Style()->GetPosition() == EPosition::kSticky) {
    return true;
  }

  if (object.Style()->ScrollsOverflow()) {
    return true;
  }

  if (object.IsInTopOrViewTransitionLayer()) {
    return true;
  }

  if (const auto* element = DynamicTo<HTMLElement>(object.GetNode())) {
    if (element->HasTagName(html_names::kFigureTag)) {
      return true;
    }
  }

  if (!annotated_roles.empty()) {
    return true;
  }

  return false;
}

void AddAnnotatedRoles(
    const LayoutObject& object,
    Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) {
  const auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (!element) {
    return;
  }
  if (element->HasTagName(html_names::kHeaderTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "banner") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kHeader);
  }
  if (element->HasTagName(html_names::kNavTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "navigation") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kNav);
  }
  if (element->HasTagName(html_names::kSearchTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "search") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSearch);
  }
  if (element->HasTagName(html_names::kMainTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "main") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kMain);
  }
  if (element->HasTagName(html_names::kArticleTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "article") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kArticle);
  }
  if (element->HasTagName(html_names::kSectionTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "region") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSection);
  }
  if (element->HasTagName(html_names::kAsideTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "complementary") {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kAside);
  }
  if (element->HasTagName(html_names::kFooterTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "contentinfo") {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kFooter);
  }
}

std::optional<DOMNodeId> GetNodeId(const LayoutObject& object) {
  auto* node = object.GetNode();
  if (object.IsLayoutView()) {
    node = &object.GetDocument();
  }

  if (!node) {
    return std::nullopt;
  }
  return DOMNodeIds::IdForNode(node);
}

bool ShouldSkipContent(const LayoutObject& object) {
  // Don't add content when node is invisible.
  return object.Style()->Visibility() != EVisibility::kVisible;
}

bool ShouldSkipSubtree(const LayoutObject& object) {
  // Skip embedded content that is not an iframe.
  // TODO(crbug.com/381273397): Add content for embed and object.
  auto* layout_embedded_content = DynamicTo<LayoutEmbeddedContent>(object);
  if (layout_embedded_content && !GetIFrame(object)) {
    return true;
  }

  // List markers are communicated by the kOrderedList and kUnorderedList
  // annotated roles.
  if (object.IsListMarker()) {
    return true;
  }

  // Table caption is communicated by the table name.
  if (object.IsTableCaption()) {
    return true;
  }

  // Skip empty text.
  auto* layout_text = DynamicTo<LayoutText>(object);
  if (layout_text && layout_text->IsAllCollapsibleWhitespace()) {
    return true;
  }

  if (DynamicTo<LayoutHTMLCanvas>(object)) {
    return true;
  }

  if (DynamicTo<LayoutSVGRoot>(object)) {
    return true;
  }

  return false;
}

void ProcessTextNode(const LayoutText& layout_text,
                     mojom::blink::AIPageContentAttributes& attributes,
                     const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kText;
  CHECK(!ShouldSkipContent(layout_text));

  auto text_style = mojom::blink::AIPageContentTextStyle::New();
  text_style->text_size = GetTextSize(*layout_text.Style(), document_style);
  text_style->has_emphasis = HasEmphasis(*layout_text.Style());

  auto text_info = mojom::blink::AIPageContentTextInfo::New();
  text_info->text_content = layout_text.TransformedText();
  text_info->text_style = std::move(text_style);
  attributes.text_info = std::move(text_info);
}

void ProcessImageNode(const LayoutImage& layout_image,
                      mojom::blink::AIPageContentAttributes& attributes,
                      const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kImage;
  CHECK(!ShouldSkipContent(layout_image));

  if (DynamicTo<LayoutMedia>(layout_image)) {
    return;
  }

  auto image_info = mojom::blink::AIPageContentImageInfo::New();

  if (auto* image_element =
          DynamicTo<HTMLImageElement>(layout_image.GetNode())) {
    // TODO(crbug.com/383127202): A11y stack generates alt text using image
    // data which could be reused for this.
    image_info->image_caption = image_element->AltText();
  }

  // TODO(crbug.com/382558422): Include image source origin.
  attributes.image_info = std::move(image_info);
}

void ProcessAnchorNode(const HTMLAnchorElement& anchor_element,
                       mojom::blink::AIPageContentAttributes& attributes,
                       const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kAnchor;
  if (ShouldSkipContent(*anchor_element.GetLayoutObject())) {
    return;
  }

  auto anchor_data = mojom::blink::AIPageContentAnchorData::New();
  anchor_data->url = anchor_element.Url();
  for (unsigned i = 0; i < anchor_element.relList().length(); ++i) {
    anchor_data->rel.push_back(GetAnchorRel(anchor_element.relList().item(i)));
  }
  attributes.anchor_data = std::move(anchor_data);
}

void ProcessTableNode(const LayoutTable& layout_table,
                      mojom::blink::AIPageContentAttributes& attributes,
                      const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kTable;
  if (ShouldSkipContent(layout_table)) {
    return;
  }

  auto table_data = mojom::blink::AIPageContentTableData::New();
  for (auto* section = layout_table.FirstChild(); section;
       section = section->NextSibling()) {
    if (section->IsTableCaption()) {
      StringBuilder table_name;
      auto* caption = To<LayoutTableCaption>(section);
      for (auto* child = caption->FirstChild(); child;
           child = child->NextSibling()) {
        if (const auto* layout_text = DynamicTo<LayoutText>(*child)) {
          table_name.Append(layout_text->TransformedText());
        }
      }
      table_data->table_name = table_name.ToString();
    }
  }
  attributes.table_data = std::move(table_data);
}

mojom::blink::AIPageContentTableRowType GetTableRowType(
    const LayoutTableRow& layout_table_row) {
  if (auto* section = layout_table_row.Section()) {
    if (auto* table_section_element =
            DynamicTo<HTMLElement>(section->GetNode())) {
      if (table_section_element->HasTagName(html_names::kTheadTag)) {
        return mojom::blink::AIPageContentTableRowType::kHeader;
      } else if (table_section_element->HasTagName(html_names::kTfootTag)) {
        return mojom::blink::AIPageContentTableRowType::kFooter;
      }
    }
  }
  return mojom::blink::AIPageContentTableRowType::kBody;
}

void ProcessTableRowNode(const LayoutTableRow& layout_table_row,
                         mojom::blink::AIPageContentAttributes& attributes,
                         const ComputedStyle& document_style) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kTableRow;
  if (ShouldSkipContent(layout_table_row)) {
    return;
  }

  auto table_row_data = mojom::blink::AIPageContentTableRowData::New();
  table_row_data->row_type = GetTableRowType(layout_table_row);
  attributes.table_row_data = std::move(table_row_data);
}

}  // namespace

// static
const char AIPageContentAgent::kSupplementName[] = "AIPageContentAgent";

// static
AIPageContentAgent* AIPageContentAgent::From(Document& document) {
  return Supplement<Document>::From<AIPageContentAgent>(document);
}

// static
void AIPageContentAgent::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver) {
  CHECK(frame && frame->GetDocument());
  CHECK(frame->IsLocalRoot());

  auto& document = *frame->GetDocument();
  auto* agent = AIPageContentAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<AIPageContentAgent>(
        base::PassKey<AIPageContentAgent>(), *frame);
    Supplement<Document>::ProvideTo(document, agent);
  }
  agent->Bind(std::move(receiver));
}

AIPageContentAgent* AIPageContentAgent::GetOrCreateForTesting(
    Document& document) {
  auto* agent = AIPageContentAgent::From(document);
  if (!agent) {
    agent = MakeGarbageCollected<AIPageContentAgent>(
        base::PassKey<AIPageContentAgent>(), *document.GetFrame());
    Supplement<Document>::ProvideTo(document, agent);
  }
  return agent;
}

AIPageContentAgent::AIPageContentAgent(base::PassKey<AIPageContentAgent>,
                                       LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()) {}

AIPageContentAgent::~AIPageContentAgent() = default;

void AIPageContentAgent::Bind(
    mojo::PendingReceiver<mojom::blink::AIPageContentAgent> receiver) {
  receiver_set_.Add(
      std::move(receiver),
      GetSupplementable()->GetTaskRunner(TaskType::kInternalUserInteraction));
}

void AIPageContentAgent::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_set_);
  Supplement<Document>::Trace(visitor);
}

void AIPageContentAgent::GetAIPageContent(GetAIPageContentCallback callback) {
  std::move(callback).Run(GetAIPageContentSync());
}

mojom::blink::AIPageContentPtr AIPageContentAgent::GetAIPageContentSync()
    const {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->View()) {
    return nullptr;
  }

  auto& document = *frame->GetDocument();
  mojom::blink::AIPageContentPtr page_content =
      mojom::blink::AIPageContent::New();

  document.View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kUnknown);

  auto* layout_view = document.GetLayoutView();
  auto* document_style = layout_view->Style();
  auto root_node = MaybeGenerateContentNode(*layout_view, *document_style);
  CHECK(root_node);

  WalkChildren(*layout_view, *root_node, *document_style);
  page_content->root_node = std::move(root_node);
  return page_content;
}

void AIPageContentAgent::WalkChildren(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const ComputedStyle& document_style) const {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return;
  }

  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (ShouldSkipSubtree(*child)) {
      continue;
    }

    auto child_content_node = MaybeGenerateContentNode(*child, document_style);
    if (child_content_node &&
        child_content_node->content_attributes->attribute_type ==
            mojom::blink::AIPageContentAttributeType::kIframe) {
      // If the child is an iframe, it does its own tree walk.
    } else {
      auto& node_for_child =
          child_content_node ? *child_content_node : content_node;
      WalkChildren(*child, node_for_child, document_style);
    }

    if (child_content_node) {
      content_node.children_nodes.emplace_back(std::move(child_content_node));
    }
  }
}

void AIPageContentAgent::ProcessIframe(
    const LayoutIFrame& object,
    mojom::blink::AIPageContentNode& content_node) const {
  content_node.content_attributes->attribute_type =
      mojom::blink::AIPageContentAttributeType::kIframe;

  auto& frame = object.ChildFrameView()->GetFrame();

  auto iframe_data = mojom::blink::AIPageContentIframeData::New();
  iframe_data->frame_token = frame.GetFrameToken();
  iframe_data->likely_ad_frame = frame.IsAdFrame();
  content_node.content_attributes->iframe_data = std::move(iframe_data);

  auto* local_frame = DynamicTo<LocalFrame>(frame);
  auto* child_layout_view =
      local_frame ? local_frame->ContentLayoutObject() : nullptr;
  if (child_layout_view) {
    // Add a node for the iframe's LayoutView for consistency with remote
    // frames.
    auto child_content_node = MaybeGenerateContentNode(
        *child_layout_view, *child_layout_view->Style());
    WalkChildren(*child_layout_view, *child_content_node,
                 *child_layout_view->Style());
    content_node.children_nodes.emplace_back(std::move(child_content_node));
  }
}

mojom::blink::AIPageContentNodePtr AIPageContentAgent::MaybeGenerateContentNode(
    const LayoutObject& object,
    const ComputedStyle& document_style) const {
  auto content_node = mojom::blink::AIPageContentNode::New();
  content_node->content_attributes =
      mojom::blink::AIPageContentAttributes::New();
  auto& attributes = *content_node->content_attributes;
  AddAnnotatedRoles(object, attributes.annotated_roles);

  // Set the attribute type and add any special attributes if the attribute type
  // requires it.
  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (const auto* iframe = GetIFrame(object)) {
    ProcessIframe(*iframe, *content_node);
  } else if (object.IsLayoutView()) {
    attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kRoot;
  } else if (object.IsText()) {
    // Since text is a leaf node, do not create a content node if should skip
    // content.
    if (ShouldSkipContent(object)) {
      return nullptr;
    }
    ProcessTextNode(To<LayoutText>(object), attributes, document_style);
  } else if (object.IsLayoutImage()) {
    // Since image is a leaf node, do not create a content node if should skip
    // content.
    if (ShouldSkipContent(object)) {
      return nullptr;
    }
    ProcessImageNode(To<LayoutImage>(object), attributes, document_style);
  } else if (const auto* anchor_element =
                 DynamicTo<HTMLAnchorElement>(object.GetNode())) {
    ProcessAnchorNode(*anchor_element, attributes, document_style);
  } else if (object.IsTable()) {
    ProcessTableNode(To<LayoutTable>(object), attributes, document_style);
  } else if (object.IsTableRow()) {
    ProcessTableRowNode(To<LayoutTableRow>(object), attributes, document_style);
  } else if (object.IsTableCell()) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kTableCell;
  } else if (element && IsHeadingTag(*element)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kHeading;
  } else if (element && element->HasTagName(html_names::kPTag)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kParagraph;
  } else if (element && element->HasTagName(html_names::kOlTag)) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kOrderedList;
  } else if (element && (element->HasTagName(html_names::kUlTag) ||
                         element->HasTagName(html_names::kDlTag))) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kUnorderedList;
  } else if (element && (element->HasTagName(html_names::kLiTag) ||
                         element->HasTagName(html_names::kDtTag) ||
                         element->HasTagName(html_names::kDdTag))) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kListItem;
  } else if (IsGenericContainer(object, attributes.annotated_roles)) {
    // Be sure to set annotated roles before calling IsGenericContainer, as
    // IsGenericContainer will check for annotated roles.
    // Keep container at the bottom of the list as it is the least specific.
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kContainer;
  } else {
    // If no attribute type was set, do not generate a content node.
    return nullptr;
  }

  if (auto node_id = AddNodeId(object, attributes)) {
    attributes.common_ancestor_dom_node_id = *node_id;
  }

  attributes.geometry = mojom::blink::AIPageContentGeometry::New();
  AddNodeGeometry(object, *attributes.geometry);

  return content_node;
}

std::optional<DOMNodeId> AIPageContentAgent::AddNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (auto node_id = GetNodeId(object)) {
    attributes.dom_node_ids.push_back(*node_id);
    return node_id;
  }

  return std::nullopt;
}

void AIPageContentAgent::AddNodeGeometry(
    const LayoutObject& object,
    mojom::blink::AIPageContentGeometry& geometry) const {
  geometry.outer_bounding_box =
      object.AbsoluteBoundingBoxRect(kMapCoordinatesFlags);

  gfx::RectF visible_bounding_box =
      object.LocalBoundingBoxRectForAccessibility();
  object.MapToVisualRectInAncestorSpace(nullptr, visible_bounding_box,
                                        kVisualRectFlags);
  geometry.visible_bounding_box = ToEnclosingRect(visible_bounding_box);

  geometry.is_fixed_or_sticky_position =
      object.Style()->GetPosition() == EPosition::kFixed ||
      object.Style()->GetPosition() == EPosition::kSticky;
  geometry.scrolls_overflow_x = object.Style()->ScrollsOverflowX();
  geometry.scrolls_overflow_y = object.Style()->ScrollsOverflowY();
}

}  // namespace blink
