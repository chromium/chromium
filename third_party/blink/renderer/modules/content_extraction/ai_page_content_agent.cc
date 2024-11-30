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
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_media.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

constexpr MapCoordinatesFlags kMapCoordinatesFlags =
    kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform;
constexpr VisualRectFlags kVisualRectFlags = kUseGeometryMapper;

// TODO(khushalsagar): This is duplicating logic from
// UnsupportedTagTypeValueForNode, consider reusing it.
bool IsHeadingTag(const HTMLElement& element) {
  return element.HasTagName(html_names::kH1Tag) ||
         element.HasTagName(html_names::kH2Tag) ||
         element.HasTagName(html_names::kH3Tag) ||
         element.HasTagName(html_names::kH4Tag) ||
         element.HasTagName(html_names::kH5Tag) ||
         element.HasTagName(html_names::kH6Tag);
}

std::optional<mojom::blink::AIPageContentAttributeType> GetAttributeType(
    const LayoutObject& object) {
  if (object.IsLayoutIFrame()) {
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

  if (element->HasTagName(html_names::kOlTag) ||
      element->HasTagName(html_names::kUlTag) ||
      element->HasTagName(html_names::kDlTag)) {
    return mojom::blink::AIPageContentAttributeType::kList;
  }

  if (element->HasTagName(html_names::kFigureTag)) {
    return mojom::blink::AIPageContentAttributeType::kFigure;
  }

  // TODO: Add FormData for attribute_type = FORM.
  return std::nullopt;
}

// Returns true if `object` should be skipped because it's a cross-process or
// cross-origin iframe.
//
// Returns false if `object` is not an iframe or it's a same-origin iframe.
//
// The second param is set to the child Document if it's a same-origin iframe.
std::tuple<bool, const LayoutView*> ShouldSkipNestedIFrame(
    const LayoutObject& object) {
  auto* frame = DynamicTo<LayoutEmbeddedContent>(object);
  if (!frame) {
    return std::make_tuple(false, nullptr);
  }

  auto* frame_view = DynamicTo<LocalFrameView>(frame->ChildFrameView());
  if (!frame_view) {
    return std::make_tuple(true, nullptr);
  }

  if (frame_view->GetFrame().IsCrossOriginToParentOrOuterDocument()) {
    return std::make_tuple(true, nullptr);
  }

  return std::make_tuple(false, frame_view->GetFrame().ContentLayoutObject());
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
  CHECK(frame->IsOutermostMainFrame());

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
  auto root_node = MaybeGenerateContentNode(*layout_view);
  CHECK(root_node);

  ProcessNode(*layout_view, *root_node);
  page_content->root_node = std::move(root_node);
  return page_content;
}

void AIPageContentAgent::ProcessNode(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node) const {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return;
  }

  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    auto [skip, child_layout_view] = ShouldSkipNestedIFrame(*child);
    if (skip) {
      continue;
    }

    auto child_content_node = MaybeGenerateContentNode(*child);
    auto& node_for_child =
        child_content_node ? *child_content_node : content_node;

    MaybeAddNodeContent(*child, *node_for_child.content_attributes);

    // We could generate a ContentNode for the child LayoutView but that seems
    // redundant given we already have one for the iframe.
    if (child_layout_view) {
      ProcessNode(*child_layout_view, node_for_child);
    } else {
      ProcessNode(*child, node_for_child);
    }

    if (child_content_node) {
      content_node.children_nodes.emplace_back(std::move(child_content_node));
    }
  }
}

mojom::blink::AIPageContentNodePtr AIPageContentAgent::MaybeGenerateContentNode(
    const LayoutObject& object) const {
  const auto attribute_type = GetAttributeType(object);
  if (!attribute_type) {
    return nullptr;
  }

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
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (object.Style()->Visibility() != EVisibility::kVisible) {
    return;
  }

  if (const auto* layout_text = DynamicTo<LayoutText>(object)) {
    AddNodeId(object, attributes);

    auto text_info = mojom::blink::AIPageContentTextInfo::New();
    text_info->text_content = layout_text->TransformedText();
    text_info->text_bounding_box =
        layout_text->AbsoluteBoundingBoxRect(kMapCoordinatesFlags);
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
      // TODO(khushalsagar): A11y stack generates alt text using image data
      // which could be reused for this.
      image_info->image_caption = image_element->AltText();
    }
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

}  // namespace blink
