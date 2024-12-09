// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
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
    kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform;
constexpr VisualRectFlags kVisualRectFlags = kUseGeometryMapper;

constexpr float kHeading1FontSizeMultiplier = 2;
constexpr float kHeading3FontSizeMultiplier = 1.17;
constexpr float kHeading5FontSizeMultiplier = 0.83;
constexpr float kHeading6FontSizeMultiplier = 0.67;

// TODO(b/383128653): This is duplicating logic from
// UnsupportedTagTypeValueForNode, consider reusing it.
bool IsHeadingTag(const HTMLElement& element) {
  return element.HasTagName(html_names::kH1Tag) ||
         element.HasTagName(html_names::kH2Tag) ||
         element.HasTagName(html_names::kH3Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH5Tag) ||
         element.HasTagName(html_names::kH6Tag);
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

std::optional<mojom::blink::AIPageContentAttributeType> GetAttributeType(
    const LayoutObject& object) {
  if (GetIFrame(object)) {
    return mojom::blink::AIPageContentAttributeType::kIframe;
  }

  if (object.IsLayoutView()) {
    return mojom::blink::AIPageContentAttributeType::kRoot;
  }

  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (!element) {
    return std::nullopt;
  }

  if (element->HasTagName(html_names::kPTag)) {
    return mojom::blink::AIPageContentAttributeType::kParagraph;
  }

  if (IsHeadingTag(*element)) {
    return mojom::blink::AIPageContentAttributeType::kHeading;
  }

  if (element->HasTagName(html_names::kOlTag)) {
    return mojom::blink::AIPageContentAttributeType::kOrderedList;
  }

  if (element->HasTagName(html_names::kUlTag) ||
      element->HasTagName(html_names::kDlTag)) {
    return mojom::blink::AIPageContentAttributeType::kUnorderedList;
  }

  if (element->HasTagName(html_names::kFigureTag)) {
    return mojom::blink::AIPageContentAttributeType::kFigure;
  }

  if (object.IsTable()) {
    return mojom::blink::AIPageContentAttributeType::kTable;
  }

  if (object.IsTableCell()) {
    return mojom::blink::AIPageContentAttributeType::kTableCell;
  }

  if (element->HasTagName(html_names::kHeaderTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "banner") {
    return mojom::blink::AIPageContentAttributeType::kHeader;
  }

  if (element->HasTagName(html_names::kNavTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "navigation") {
    return mojom::blink::AIPageContentAttributeType::kNav;
  }

  if (element->HasTagName(html_names::kSearchTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "search") {
    return mojom::blink::AIPageContentAttributeType::kSearch;
  }

  if (element->HasTagName(html_names::kMainTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "main") {
    return mojom::blink::AIPageContentAttributeType::kMain;
  }

  if (element->HasTagName(html_names::kArticleTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "article") {
    return mojom::blink::AIPageContentAttributeType::kArticle;
  }

  if (element->HasTagName(html_names::kSectionTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "region") {
    return mojom::blink::AIPageContentAttributeType::kSection;
  }

  if (element->HasTagName(html_names::kAsideTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "complementary") {
    return mojom::blink::AIPageContentAttributeType::kAside;
  }

  if (element->HasTagName(html_names::kFooterTag) ||
      element->FastGetAttribute(html_names::kRoleAttr) == "contentinfo") {
    return mojom::blink::AIPageContentAttributeType::kFooter;
  }

  // TODO: Add FormData for attribute_type = FORM.
  return std::nullopt;
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

// TODO(crbug.com/381273397): Add content for embed and object.
bool ShouldSkipEmbeddedContent(const LayoutObject& object) {
  auto* layout_embedded_content = DynamicTo<LayoutEmbeddedContent>(object);
  return layout_embedded_content && !GetIFrame(object);
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
  auto root_node = MaybeGenerateContentNode(*layout_view);
  CHECK(root_node);

  ProcessNode(*layout_view, *root_node, *document_style);
  page_content->root_node = std::move(root_node);
  return page_content;
}

void AIPageContentAgent::ProcessNode(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const ComputedStyle& document_style) const {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return;
  }

  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (ShouldSkipEmbeddedContent(*child)) {
      continue;
    }

    if (child->IsListMarker()) {
      continue;
    }

    auto child_content_node = MaybeGenerateContentNode(*child);
    if (child_content_node &&
        child_content_node->content_attributes->attribute_type ==
            mojom::blink::AIPageContentAttributeType::kIframe) {
      ProcessIframe(*GetIFrame(*child), *child_content_node);
    } else if (child_content_node &&
        child_content_node->content_attributes->attribute_type ==
            mojom::blink::AIPageContentAttributeType::kTable) {
      ProcessTable(To<LayoutTable>(*child), *child_content_node,
                   document_style);
    } else {
      auto& node_for_child =
          child_content_node ? *child_content_node : content_node;
      MaybeAddNodeContent(*child, *node_for_child.content_attributes,
                          document_style);
      ProcessNode(*child, node_for_child, document_style);
    }

    if (child_content_node) {
      content_node.children_nodes.emplace_back(std::move(child_content_node));
    }
  }
}

void AIPageContentAgent::ProcessIframe(
    const LayoutIFrame& object,
    mojom::blink::AIPageContentNode& content_node) const {
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
    auto child_content_node = MaybeGenerateContentNode(*child_layout_view);
    MaybeAddNodeContent(*child_layout_view,
                        *child_content_node->content_attributes,
                        *child_layout_view->Style());
    ProcessNode(*child_layout_view, *child_content_node,
                *child_layout_view->Style());
    content_node.children_nodes.emplace_back(std::move(child_content_node));
  }
}

mojom::blink::AIPageContentNodePtr AIPageContentAgent::MaybeGenerateContentNode(
    const LayoutObject& object) const {
  const auto attribute_type = GetAttributeType(object);
  if (!attribute_type) {
    return nullptr;
  }
  LOG(ERROR) << "Generated content node : " << object;
  auto content_node = mojom::blink::AIPageContentNode::New();
  content_node->content_attributes =
      mojom::blink::AIPageContentAttributes::New();
  auto& attributes = *content_node->content_attributes;
  AddNodeId(object, attributes);
  attributes.common_ancestor_dom_node_id = attributes.dom_node_ids.back();

  attributes.attribute_type = *attribute_type;

  attributes.geometry = mojom::blink::AIPageContentGeometry::New();
  AddNodeGeometry(object, *attributes.geometry);

  return content_node;
}

void AIPageContentAgent::MaybeAddNodeContent(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes,
    const ComputedStyle& document_style) const {
  if (object.Style()->Visibility() != EVisibility::kVisible) {
    return;
  }

  if (const auto* layout_text = DynamicTo<LayoutText>(object)) {
    AddNodeId(object, attributes);

    auto text_style = mojom::blink::AIPageContentTextStyle::New();
    text_style->text_size = GetTextSize(*layout_text->Style(), document_style);
    text_style->has_emphasis = HasEmphasis(*layout_text->Style());

    auto text_info = mojom::blink::AIPageContentTextInfo::New();
    text_info->text_content = layout_text->TransformedText();
    text_info->text_bounding_box =
        layout_text->AbsoluteBoundingBoxRect(kMapCoordinatesFlags);
    text_info->text_style = std::move(text_style);
    attributes.text_info.emplace_back(std::move(text_info));
    return;
  }

  if (DynamicTo<LayoutHTMLCanvas>(object)) {
    return;
  }

  if (DynamicTo<LayoutSVGRoot>(object)) {
    return;
  }

  if (const auto* image = DynamicTo<LayoutImage>(object)) {
    if (DynamicTo<LayoutMedia>(image)) {
      return;
    }

    AddNodeId(object, attributes);

    auto image_info = mojom::blink::AIPageContentImageInfo::New();
    image_info->image_bounding_box =
        image->AbsoluteBoundingBoxRect(kMapCoordinatesFlags);
    if (auto* image_element = DynamicTo<HTMLImageElement>(image->GetNode())) {
      // TODO(b/383127202): A11y stack generates alt text using image data
      // which could be reused for this.
      image_info->image_caption = image_element->AltText();
    }
    // TODO(b/382558422): Include image source origin.
    attributes.image_info.emplace_back(std::move(image_info));
  }
}

void AIPageContentAgent::AddNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (auto node_id = GetNodeId(object)) {
    attributes.dom_node_ids.push_back(*node_id);
  }
}

void AIPageContentAgent::AddNodeGeometry(
    const LayoutObject& object,
    mojom::blink::AIPageContentGeometry& geometry) const {
  geometry.outer_bounding_box =
      object.AbsoluteBoundingBoxRect(kMapCoordinatesFlags);

  // TODO(crbug.com/381273397): Ensure that the clips/transforms from the remote
  // ancestor are applied when computing this.
  // See
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/dom/element.cc;l=2476;drc=1651676a30cd7abcd177975f7cd0e37bd945f663.
  gfx::RectF visible_bounding_box =
      object.LocalBoundingBoxRectForAccessibility();
  object.MapToVisualRectInAncestorSpace(nullptr, visible_bounding_box,
                                        kVisualRectFlags);
  geometry.visible_bounding_box = ToEnclosingRect(visible_bounding_box);
}

void AIPageContentAgent::ProcessTable(
    const LayoutTable& object,
    mojom::blink::AIPageContentNode& content_node,
    const ComputedStyle& document_style) const {
  auto table_data = mojom::blink::AIPageContentTableData::New();
  for (auto* child = object.FirstChild(); child; child = child->NextSibling()) {
    if (child->IsTableCaption()) {
      ProcessTableCaption(To<LayoutTableCaption>(*child), *table_data);
    }
    if (child->IsTableSection()) {
      ProcessTableSection(To<LayoutTableSection>(*child), *table_data,
                          document_style);
    }
  }
  content_node.content_attributes->table_data = std::move(table_data);
}

void AIPageContentAgent::ProcessTableCaption(
    const LayoutTableCaption& object,
    mojom::blink::AIPageContentTableData& table_data) const {
  StringBuilder table_name;
  for (auto* child = object.FirstChild(); child; child = child->NextSibling()) {
    if (const auto* layout_text = DynamicTo<LayoutText>(*child)) {
      table_name.Append(layout_text->TransformedText());
    }
  }
  table_data.table_name = table_name.ToString();
}

void AIPageContentAgent::ProcessTableSection(
    const LayoutTableSection& object,
    mojom::blink::AIPageContentTableData& table_data,
    const ComputedStyle& document_style) const {
  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  for (auto* child = object.FirstChild(); child; child = child->NextSibling()) {
    auto row = mojom::blink::AIPageContentTableRow::New();
    ProcessTableRow(To<LayoutTableRow>(*child), *row, document_style);
    if (element && element->HasTagName(html_names::kTheadTag)) {
      table_data.header_rows.emplace_back(std::move(row));
    } else if (element && element->HasTagName(html_names::kTfootTag)) {
      table_data.footer_rows.emplace_back(std::move(row));
    } else {
      table_data.body_rows.emplace_back(std::move(row));
    }
  }
}

void AIPageContentAgent::ProcessTableRow(
    const LayoutTableRow& object,
    mojom::blink::AIPageContentTableRow& table_row,
    const ComputedStyle& document_style) const {
  for (auto* child = object.FirstChild(); child; child = child->NextSibling()) {
    // Add the cell contents as a ContentNode.
    // TODO(b/383127685): Consider adding additional information as
    // CellContentData, such as the cell's column span.
    auto child_content_node = MaybeGenerateContentNode(*child);
    MaybeAddNodeContent(*child, *child_content_node->content_attributes,
                        document_style);
    ProcessNode(*child, *child_content_node, document_style);
    table_row.cells.emplace_back(std::move(child_content_node));
  }
}

}  // namespace blink
