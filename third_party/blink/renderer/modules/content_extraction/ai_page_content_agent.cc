// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include "base/containers/adapters.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-blink.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/option_list.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_media.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_debug_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {

// Coordinate mapping flags
// - Viewport mapping: positions relative to the window/viewport origin.
constexpr MapCoordinatesFlags kMapToViewportFlags =
    kTraverseDocumentBoundaries | kApplyRemoteViewportTransform;
constexpr VisualRectFlags kVisualRectFlags = static_cast<VisualRectFlags>(
    kUseGeometryMapper | kVisualRectApplyRemoteViewportTransform |
    kIgnoreFilters);

constexpr float kHeading1FontSizeMultiplier = 2;
constexpr float kHeading3FontSizeMultiplier = 1.17;
constexpr float kHeading5FontSizeMultiplier = 0.83;
constexpr float kHeading6FontSizeMultiplier = 0.67;

ListBasedHitTestBehavior CollectHitTestNodes(std::vector<DOMNodeId>& hit_nodes,
                                             const Node& node,
                                             DOMNodeId dom_node_id) {
  if (node.GetLayoutObject()) {
    hit_nodes.push_back(dom_node_id);
  }
  return kContinueHitTesting;
}

// Computes the visible portion of a LayoutObject's bounding box.
//
// This function calculates what part of the object is actually visible in the
// viewport, taking into account:
// - The object's local bounding box (its natural size and position)
// - Viewport clipping (objects outside the viewport are clipped)
// - Scroll offsets (objects scrolled out of view are clipped)
// - CSS overflow clipping from ancestor containers
//
// The returned rectangle is in viewport coordinates (relative to the top-left
// of the visible area), which is why coordinates are always >= 0.
gfx::Rect ComputeVisibleBoundingBox(const LayoutObject& object) {
  // Layout must be complete before computing bounding boxes.
  DCHECK(object.GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "ComputeVisibleBoundingBox only works when layout is complete";

  // Get the object's local bounding box before viewport clipping.
  gfx::RectF object_rect =
      ClipPathClipper::LocalClipPathBoundingBox(object).value_or(
          object.LocalBoundingBoxRectForAccessibility(
              LayoutObject::IncludeDescendants(false)));

  // Transform the local bounding box to viewport coordinates, applying:
  // 1. All CSS transforms (translate, scale, rotate, etc.)
  // 2. Scroll offsets from all ancestor scroll containers
  // 3. Clipping from overflow:hidden containers
  // 4. Viewport clipping (anything outside the viewport is clipped)
  //
  // The nullptr ancestor means "map to the root of the document". When used
  // with kVisualRectFlags, this gives us viewport-relative coordinates.
  // TODO(khushalsagar): It might be more optimal to derive this from output of
  // paint.
  object.MapToVisualRectInAncestorSpace(nullptr, object_rect, kVisualRectFlags);

  gfx::Rect visible_box_in_viewport_coords = ToEnclosingRect(object_rect);

#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::AIPageContentCheckGeometryEnabled()) {
    // The visible bounding box should always have non-negative coordinates
    // since it's relative to the viewport. Negative coordinates would indicate
    // a bug in the coordinate transformation.
    CHECK_GE(visible_box_in_viewport_coords.x(), 0)
        << "Visible bounding box should be viewport-relative with x >= 0, got: "
        << visible_box_in_viewport_coords.ToString()
        << " for object: " << object;
    CHECK_GE(visible_box_in_viewport_coords.y(), 0)
        << "Visible bounding box should be viewport-relative with y >= 0, got: "
        << visible_box_in_viewport_coords.ToString()
        << " for object: " << object;
  }
#endif

  return visible_box_in_viewport_coords;
}

gfx::Rect ComputeOuterBoundingBox(const LayoutObject& object) {
  const std::optional<gfx::RectF> clip_path_box =
      ClipPathClipper::LocalClipPathBoundingBox(object);

  if (clip_path_box.has_value()) {
    gfx::QuadF absolute_quad = object.LocalToAbsoluteQuad(
        gfx::QuadF(clip_path_box.value()), kMapToViewportFlags);
    return gfx::ToEnclosingRect(absolute_quad.BoundingBox());
  }

  gfx::Rect absolute_box = object.AbsoluteBoundingBoxRect(kMapToViewportFlags);
  // Normalize empty boxes to make test results easier to read.
  return absolute_box.IsEmpty() ? gfx::Rect() : absolute_box;
}

// Processes fragment bounding boxes for layout objects that can be split.
//
// Uses QuadsInAncestor() to retrieve quads for each object, then converts them
// to integer bounding rects.
//
// In CSS layout, some objects can be "fragmented" - split across multiple
// visual areas. This includes:
// - Text that wraps across multiple lines
// - Content that flows across CSS columns
//
// Each fragment represents a visual piece of the same logical object.
// We only store fragment boxes when there are multiple fragments (size > 1),
// as single fragments are redundant with the main bounding box.
void ComputeFragmentBoundingBoxes(
    const LayoutObject& object,
    mojom::blink::AIPageContentGeometry& geometry) {
  Vector<gfx::QuadF> fragment_quads_in_viewport_coords;
  object.QuadsInAncestor(fragment_quads_in_viewport_coords,
                         /*ancestor=*/nullptr, kMapToViewportFlags);

  Vector<gfx::Rect> fragment_rects_in_viewport_coords;
  for (const auto& fragment_quad_in_viewport_coords :
       fragment_quads_in_viewport_coords) {
    gfx::Rect fragment_enclosing_rect_in_viewport_coords =
        gfx::ToEnclosingRect(fragment_quad_in_viewport_coords.BoundingBox());
    // Clip to the viewport by intersecting with the element's visible bounding
    // box (viewport-relative).
    fragment_enclosing_rect_in_viewport_coords.Intersect(
        geometry.visible_bounding_box);
    if (!fragment_enclosing_rect_in_viewport_coords.IsEmpty()) {
      fragment_rects_in_viewport_coords.push_back(
          fragment_enclosing_rect_in_viewport_coords);
    }
  }

  if (fragment_rects_in_viewport_coords.size() > 1) {
    geometry.fragment_visible_bounding_boxes =
        std::move(fragment_rects_in_viewport_coords);
  }
}

// Validates the relationship between outer and visible bounding boxes.
//
// The visible bounding box should generally be contained within or equal to
// the outer bounding box, since it represents the visible portion of the
// object. However, there are some exceptions:
// 1. Inline elements can have different calculation methods that cause slight
// differences
// 2. Floating-point to integer conversions can introduce small rounding errors
// 3. CSS transforms can cause complex geometric relationships
#if DCHECK_IS_ON()
void ValidateBoundingBoxes(const gfx::Rect& outer_box_in_absolute_coords,
                           const gfx::Rect& visible_box_in_viewport_coords,
                           const LayoutObject& object) {
  // Visible box coordinates should always be viewport-relative (>= 0)
  DCHECK_GE(visible_box_in_viewport_coords.x(), 0)
      << "Visible box should have x >= 0, got: "
      << visible_box_in_viewport_coords.ToString() << " for object: " << object;
  DCHECK_GE(visible_box_in_viewport_coords.y(), 0)
      << "Visible box should have y >= 0, got: "
      << visible_box_in_viewport_coords.ToString() << " for object: " << object;

  // For block-level elements, the visible box should generally be no larger
  // than the outer box (with some tolerance for rounding errors).
  // Inline elements are exempt because they can have different calculation
  // methods that cause the visible box to be larger.
  // TODO(crbug.com/422588784): Fixinline element box sizing  and enable check.
  if (!object.IsInline()) {
    const int kTolerancePixels = 1;
    DCHECK_LE(visible_box_in_viewport_coords.width(),
              outer_box_in_absolute_coords.width() + kTolerancePixels)
        << "Visible box width should not exceed outer box width by more than "
        << kTolerancePixels
        << "px. Visible: " << visible_box_in_viewport_coords.ToString()
        << ", Outer: " << outer_box_in_absolute_coords.ToString()
        << " for object: " << object;
    DCHECK_LE(visible_box_in_viewport_coords.height(),
              outer_box_in_absolute_coords.height() + kTolerancePixels)
        << "Visible box height should not exceed outer box height by more than "
        << kTolerancePixels
        << "px. Visible: " << visible_box_in_viewport_coords.ToString()
        << ", Outer: " << outer_box_in_absolute_coords.ToString()
        << " for object: " << object;
  }
}
#endif  // DCHECK_IS_ON()

void ComputeScrollerInfo(
    const LayoutObject& object,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) {
  if (!object.IsBoxModelObject()) {
    return;
  }

  auto* scrollable_area = To<LayoutBoxModelObject>(object).GetScrollableArea();
  if (!scrollable_area) {
    return;
  }

  const auto scrolling_bounds = scrollable_area->ContentsSize();
  const auto visible_area = scrollable_area->VisibleContentRect();

  // If the visible area covers the scrollable area, scrolling this node will be
  // a no-op. Allow 1px of slop due to differences in rounding.
  constexpr int kTolerance = 1;
  if (scrolling_bounds.width() - visible_area.width() < kTolerance &&
      scrolling_bounds.height() - visible_area.height() < kTolerance) {
    return;
  }

  auto scroller_info = mojom::blink::AIPageContentScrollerInfo::New();
  scroller_info->scrolling_bounds = scrolling_bounds;
  scroller_info->visible_area = visible_area;
  scroller_info->user_scrollable_horizontal =
      scrollable_area->UserInputScrollable(kHorizontalScrollbar);
  scroller_info->user_scrollable_vertical =
      scrollable_area->UserInputScrollable(kVerticalScrollbar);
  interaction_info.scroller_info = std::move(scroller_info);
}

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

RGBA32 GetColor(const ComputedStyle& style) {
  return style.VisitedDependentColor(GetCSSPropertyColor()).Rgb();
}

const LayoutIFrame* GetIFrame(const LayoutObject& object) {
  return DynamicTo<LayoutIFrame>(object);
}

std::optional<DOMNodeId> GetDomNodeId(const LayoutObject& object) {
  auto* node = object.GetNode();
  if (object.IsLayoutView()) {
    node = &object.GetDocument();
  }

  if (!node) {
    return std::nullopt;
  }
  return DOMNodeIds::IdForNode(node);
}

bool IsVisible(const LayoutObject& object) {
  // Don't add content when node is invisible.
  return object.Style()->Visibility() == EVisibility::kVisible;
}

void AddClickabilityReasons(
    const Element& element,
    const ax::mojom::Role role,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) {
  using Reason = mojom::blink::AIPageContentClickabilityReason;

  if (element.IsClickableFormControlNode()) {
    interaction_info.clickability_reasons.push_back(Reason::kClickableControl);
  }

  if (element.HasJSBasedEventListeners(event_type_names::kClick)) {
    interaction_info.clickability_reasons.push_back(Reason::kClickEvents);
  }

  const bool has_mouse_hover =
      element.HasJSBasedEventListeners(event_type_names::kMouseover) ||
      element.HasJSBasedEventListeners(event_type_names::kMouseenter);
  const bool has_mouse_click =
      element.HasJSBasedEventListeners(event_type_names::kMouseup) ||
      element.HasJSBasedEventListeners(event_type_names::kMousedown);
  if (has_mouse_hover) {
    interaction_info.clickability_reasons.push_back(Reason::kMouseHover);
  }
  if (has_mouse_click) {
    interaction_info.clickability_reasons.push_back(Reason::kMouseClick);
  }

  if (element.HasJSBasedEventListeners(event_type_names::kKeydown) ||
      element.HasJSBasedEventListeners(event_type_names::kKeypress) ||
      element.HasJSBasedEventListeners(event_type_names::kKeyup)) {
    interaction_info.clickability_reasons.push_back(Reason::kKeyEvents);
  }

  if (IsEditable(element)) {
    interaction_info.clickability_reasons.push_back(Reason::kEditable);
  }

  const ComputedStyle& style = element.ComputedStyleRef();
  if (style.Cursor() == ECursor::kPointer && !style.CursorIsInherited()) {
    interaction_info.clickability_reasons.push_back(Reason::kCursorPointer);
  }

  if (style.AffectedByHover()) {
    interaction_info.clickability_reasons.push_back(Reason::kHoverPseudoClass);
  }

  if (ui::IsClickable(role)) {
    interaction_info.clickability_reasons.push_back(Reason::kAriaRole);
  }

  if (AXObject::HasPopupFromAttribute(element)) {
    interaction_info.clickability_reasons.push_back(Reason::kAriaHasPopup);
  }

  bool aria_expanded = false;
  if (AXObject::AriaBooleanAttribute(element, html_names::kAriaExpandedAttr,
                                     &aria_expanded)) {
    if (aria_expanded) {
      interaction_info.clickability_reasons.push_back(
          Reason::kAriaExpandedTrue);
    } else {
      interaction_info.clickability_reasons.push_back(
          Reason::kAriaExpandedFalse);
    }
  }

  const auto& autocomplete =
      element.FastGetAttribute(html_names::kAutocompleteAttr);
  const auto& aria_autocomplete =
      element.FastGetAttribute(html_names::kAriaAutocompleteAttr);
  if ((autocomplete && autocomplete != "off") ||
      (aria_autocomplete == "inline" || aria_autocomplete == "list" ||
       aria_autocomplete == "both")) {
    interaction_info.clickability_reasons.push_back(Reason::kAutocomplete);
  }

  if (element.HasTabIndexWasSetExplicitly()) {
    interaction_info.clickability_reasons.push_back(Reason::kTabIndex);
  }
}

// Returns whether interaction is determined to be disabled.
bool AddInteractionDisabledReasons(
    const Element& element,
    bool is_aria_disabled,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) {
  using Reason = mojom::blink::AIPageContentInteractionDisabledReason;

  bool is_disabled = false;

  if (is_aria_disabled) {
    interaction_info.interaction_disabled_reasons.push_back(
        Reason::kAriaDisabled);
    is_disabled = true;
  }

  if (auto* form_control_element = DynamicTo<HTMLFormControlElement>(&element);
      form_control_element && form_control_element->IsActuallyDisabled()) {
    interaction_info.interaction_disabled_reasons.push_back(Reason::kDisabled);
    is_disabled = true;
  }

  const ComputedStyle& style = element.ComputedStyleRef();
  if (style.Cursor() == ECursor::kNotAllowed) {
    interaction_info.interaction_disabled_reasons.push_back(
        Reason::kCursorNotAllowed);
  }

  return is_disabled;
}

bool ShouldSkipSubtree(const LayoutObject& object) {
  auto* layout_embedded_content = DynamicTo<LayoutEmbeddedContent>(object);
  if (layout_embedded_content) {
    auto* layout_iframe = GetIFrame(object);

    // Skip embedded content that is not an iframe.
    // TODO(crbug.com/381273397): Add content for embed and object.
    if (!layout_iframe) {
      return true;
    }

    // Skip iframe nodes which don't have a Document.
    if (!layout_iframe->ChildFrameView()) {
      return true;
    }
  }

  // List markers are communicated by the kOrderedList and kUnorderedList
  // annotated roles.
  if (object.IsListMarker()) {
    return true;
  }

  // Skip empty text.
  auto* layout_text = DynamicTo<LayoutText>(object);
  if (layout_text && layout_text->IsAllCollapsibleWhitespace()) {
    return true;
  }

  return false;
}

bool ShouldSkipDescendants(
    const mojom::blink::AIPageContentNodePtr& content_node) {
  if (!content_node) {
    return false;
  }
  // If the child is an iframe, it does its own tree walk.
  // TODO(crbug.com/405173553): Moving ProcessIframe here might simplify
  // tree construction and keep stack depth counting in one place.
  if (content_node->content_attributes->attribute_type ==
      mojom::blink::AIPageContentAttributeType::kIframe) {
    return true;
  }

  // If the feature is disabled, we don't capture the SVG layout internally so
  // there's no need to walk their tree.
  if (content_node->content_attributes->attribute_type ==
          mojom::blink::AIPageContentAttributeType::kSvgRoot &&
      !RuntimeEnabledFeatures::AIPageContentIncludeSVGSubtreeEnabled()) {
    return true;
  }

  // There's no layout nodes under a canvas, the content is just the
  // canvas buffer.
  if (content_node->content_attributes->attribute_type ==
      mojom::blink::AIPageContentAttributeType::kCanvas) {
    return true;
  }

  // Ensure that password editor subtrees are skipped even when the password
  // is revealed.
  if (content_node->content_attributes->form_control_data &&
      content_node->content_attributes->form_control_data->redaction_decision ==
          mojom::blink::AIPageContentRedactionDecision::
              kRedacted_HasBeenPassword) {
    return true;
  }

  return false;
}

void ProcessTextNode(const LayoutText& layout_text,
                     mojom::blink::AIPageContentAttributes& attributes,
                     const ComputedStyle& document_style) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kText;
  CHECK(IsVisible(layout_text));

  auto text_style = mojom::blink::AIPageContentTextStyle::New();
  text_style->text_size = GetTextSize(*layout_text.Style(), document_style);
  text_style->has_emphasis = HasEmphasis(*layout_text.Style());
  text_style->color = GetColor(*layout_text.Style());

  auto text_info = mojom::blink::AIPageContentTextInfo::New();
  text_info->text_content = layout_text.TransformedText();
  text_info->text_style = std::move(text_style);
  attributes.text_info = std::move(text_info);
}

void ProcessImageNode(const LayoutObject& layout_image,
                      mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kImage;
  CHECK(IsVisible(layout_image));
  CHECK(layout_image.IsImage() || layout_image.IsSVGImage());

  // LayoutImage is a superclass of LayoutMedia, which is a superclass of
  // LayoutVideo and LayoutAudio. We only want to process images here, so
  // we enforce that the object is not a media object.
  CHECK(!layout_image.IsMedia());

  auto image_info = mojom::blink::AIPageContentImageInfo::New();

  // TODO(b/468126774): Set caption for SVG <images> based on <title> elements.
  if (auto* image_element =
          DynamicTo<HTMLImageElement>(layout_image.GetNode())) {
    // TODO(crbug.com/383127202): A11y stack generates alt text using image
    // data which could be reused for this.
    image_info->image_caption = image_element->AltText();
  }

  // TODO(crbug.com/382558422): Include image source origin.
  attributes.image_info = std::move(image_info);
}

void ProcessSVGRoot(const LayoutSVGRoot& layout_svg,
                    mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kSvgRoot;
  CHECK(IsVisible(layout_svg));

  auto* element = DynamicTo<Element>(layout_svg.GetNode());
  if (!element) {
    return;
  }

  auto svg_root_data = mojom::blink::AIPageContentSvgRootData::New();
  // TODO(b/452908424): Consider removing this given that the inner text is
  // available in the text nodes.
  svg_root_data->inner_text = element->GetInnerTextWithoutUpdate();
  attributes.svg_root_data = std::move(svg_root_data);
}

void ProcessCanvasNode(const LayoutHTMLCanvas& layout_canvas,
                       mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kCanvas;
  CHECK(IsVisible(layout_canvas));

  auto canvas_data = mojom::blink::AIPageContentCanvasData::New();
  canvas_data->layout_size = ToRoundedSize(layout_canvas.StitchedSize());
  attributes.canvas_data = std::move(canvas_data);
}

void ProcessVideoNode(const HTMLVideoElement& video_element,
                      mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kVideo;
  if (!IsVisible(*video_element.GetLayoutObject())) {
    return;
  }

  auto video_data = mojom::blink::AIPageContentVideoData::New();
  video_data->url = video_element.SourceURL();
  // TODO(crbug.com/382558422): Include video source origin.
  attributes.video_data = std::move(video_data);
}

void ProcessAnchorNode(const HTMLAnchorElement& anchor_element,
                       mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kAnchor;
  if (!IsVisible(*anchor_element.GetLayoutObject())) {
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
                      mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kTable;
  if (!IsVisible(layout_table)) {
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

void ProcessFormNode(const HTMLFormElement& form_element,
                     mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kForm;
  if (!IsVisible(*form_element.GetLayoutObject())) {
    return;
  }
  auto form_data = mojom::blink::AIPageContentFormData::New();
  if (const auto& name = form_element.GetName()) {
    form_data->form_name = name;
  }
  form_data->action_url = KURL(form_element.action());

  attributes.form_data = std::move(form_data);
}

void ProcessFormControlNode(const HTMLFormControlElement& form_control_element,
                            mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kFormControl;
  if (!IsVisible(*form_control_element.GetLayoutObject())) {
    return;
  }
  auto form_control_data = mojom::blink::AIPageContentFormControlData::New();
  form_control_data->form_control_type = form_control_element.FormControlType();
  form_control_data->field_name = form_control_element.GetName();
  form_control_data->is_required = form_control_element.IsRequired();

  // Set the default value for redaction, and override below as appropriate.
  form_control_data->redaction_decision =
      mojom::blink::AIPageContentRedactionDecision::kNoRedactionNecessary;

  if (const auto* text_control_element =
          DynamicTo<TextControlElement>(form_control_element)) {
    // Don't include password values as they are sensitive.
    if (const auto* input_element =
            DynamicTo<HTMLInputElement>(text_control_element)) {
      if (input_element->HasBeenPasswordField()) {
        form_control_data->redaction_decision =
            input_element->Value().empty()
                ? mojom::blink::AIPageContentRedactionDecision::
                      kUnredacted_EmptyPassword
                : mojom::blink::AIPageContentRedactionDecision::
                      kRedacted_HasBeenPassword;
      }
    }
    if (form_control_data->redaction_decision !=
        mojom::blink::AIPageContentRedactionDecision::
            kRedacted_HasBeenPassword) {
      form_control_data->field_value = text_control_element->Value();
    }
    form_control_data->placeholder =
        text_control_element->GetPlaceholderValue();
  }
  if (const auto* html_input_element =
          DynamicTo<HTMLInputElement>(form_control_element)) {
    form_control_data->is_checked = html_input_element->Checked();
  }
  if (const auto* select_element =
          DynamicTo<HTMLSelectElement>(form_control_element)) {
    for (auto& option_element : select_element->GetOptionList()) {
      auto select_option = mojom::blink::AIPageContentSelectOption::New();
      select_option->value = option_element.value();
      select_option->text = option_element.text();
      if (select_option->text.empty()) {
        select_option->text = option_element.DisplayLabel();
      }
      select_option->is_selected = option_element.Selected();
      select_option->disabled = option_element.IsDisabledFormControl();
      form_control_data->select_options.push_back(std::move(select_option));
    }
  }
  attributes.form_control_data = std::move(form_control_data);
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
                         mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kTableRow;
  if (!IsVisible(layout_table_row)) {
    return;
  }

  auto table_row_data = mojom::blink::AIPageContentTableRowData::New();
  table_row_data->row_type = GetTableRowType(layout_table_row);
  attributes.table_row_data = std::move(table_row_data);
}

// Records latency metrics for the given latency and total latency.
void RecordLatencyMetrics(base::TimeTicks start_time,
                          base::TimeTicks synchronous_execution_start_time,
                          base::TimeTicks end_time,
                          bool is_main_frame,
                          const mojom::blink::AIPageContentOptions& options) {
  const base::TimeDelta latency = end_time - synchronous_execution_start_time;
  const base::TimeDelta latency_with_scheduling_delay = end_time - start_time;

  const auto trace_track =
      perfetto::Track(base::trace_event::GetNextGlobalTraceId());

  if (is_main_frame) {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.MainFrame", latency);
    TRACE_EVENT_BEGIN("loading", "AIPageContentGenerationMainFrame",
                      trace_track, synchronous_execution_start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame",
        latency);
    TRACE_EVENT_BEGIN("loading", "AIPageContentGenerationRemoteSubFrame",
                      trace_track, synchronous_execution_start_time);
  }
  TRACE_EVENT_END("loading", trace_track, end_time);

  if (options.on_critical_path) {
    if (is_main_frame) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "Critical."
          "MainFrame",
          latency_with_scheduling_delay);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "Critical."
          "RemoteSubFrame",
          latency_with_scheduling_delay);
    }
  } else {
    if (is_main_frame) {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "NonCritical."
          "MainFrame",
          latency_with_scheduling_delay);
    } else {
      UMA_HISTOGRAM_TIMES(
          "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
          "NonCritical."
          "RemoteSubFrame",
          latency_with_scheduling_delay);
    }
  }
}

// Returns true if extracting the content can't be deferred until the next
// frame.
bool NeedsSyncExtraction(const mojom::blink::AIPageContentOptions& options) {
  return options.on_critical_path;
}

const mojom::blink::AIPageContentNode* FindContentNode(
    const mojom::blink::AIPageContentNode* current_node,
    DOMNodeId target_id) {
  if (!current_node) {
    return nullptr;
  }
  if (current_node->content_attributes &&
      current_node->content_attributes->dom_node_id == target_id) {
    return current_node;
  }
  for (const auto& child : current_node->children_nodes) {
    if (const mojom::blink::AIPageContentNode* found =
            FindContentNode(child.get(), target_id)) {
      return found;
    }
  }
  return nullptr;
}

// Recursively traverses the content node tree, applying the given `offset` to
// all geometry fields. This is used to translate the coordinates of a subtree
// from one coordinate space to another, for example, to adjust a popup's
// geometry to be relative to the main frame's viewport.
void OffsetNodeGeometry(mojom::blink::AIPageContentNode& node,
                        const gfx::Vector2d& offset) {
  if (node.content_attributes && node.content_attributes->geometry) {
    node.content_attributes->geometry->outer_bounding_box.Offset(offset);
    node.content_attributes->geometry->visible_bounding_box.Offset(offset);
    for (gfx::Rect& rect :
         node.content_attributes->geometry->fragment_visible_bounding_boxes) {
      rect.Offset(offset);
    }
  }
  for (mojom::blink::AIPageContentNodePtr& child : node.children_nodes) {
    OffsetNodeGeometry(*child, offset);
  }
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

// static
AIPageContentAgent* AIPageContentAgent::GetOrCreateForTesting(
    Document& document) {
  auto* agent = AIPageContentAgent::From(document);
  if (!agent) {
    DCHECK(document.GetFrame());
    agent = MakeGarbageCollected<AIPageContentAgent>(
        base::PassKey<AIPageContentAgent>(), *document.GetFrame());
    Supplement<Document>::ProvideTo(document, agent);
  }
  return agent;
}

#if DCHECK_IS_ON()
// static
void AIPageContentAgent::
    EnableAutomaticActionableExtractionOnPageLoadForTesting(LocalFrame& frame) {
  // The caller, InstallSupplements(), runs right after a LocalDOMWindow
  // installs its Document, so `frame` should always expose one here.
  Document* document = frame.GetDocument();
  DCHECK(document);
  auto* agent = AIPageContentAgent::GetOrCreateForTesting(*document);
  if (!agent->is_auto_actionable_extraction_pending_) {
    agent->is_auto_actionable_extraction_pending_ = true;
    agent->EnsureLifecycleObserverRegistered();
    // Ensure that we get a lifecycle update no matter what.
    LocalFrameView* frame_view = document->View();
    Page* page = document->GetPage();
    if (frame_view && page && !page->Animator().IsServicingAnimations()) {
      page->Animator().ScheduleVisualUpdate(&frame);
    }
  }
}
#endif

AIPageContentAgent::AIPageContentAgent(base::PassKey<AIPageContentAgent>,
                                       LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_set_(this, frame.DomWindow()) {
  DCHECK(frame.GetDocument());
}

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

void AIPageContentAgent::DidFinishPostLifecycleSteps(const LocalFrameView&) {
  for (auto& task : std::move(async_extraction_tasks_)) {
    std::move(task).Run();
  }
  async_extraction_tasks_.clear();
#if DCHECK_IS_ON()
  MaybeRunAutomaticActionableExtraction();
#endif
}

void AIPageContentAgent::GetAIPageContent(
    mojom::blink::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  LocalFrameView* view = GetSupplementable()->View();

  // If there's no lifecycle pending, we can't rely on post lifecycle
  // notifications and the layout is likely clean.
  const bool can_do_sync_extraction = !view || !view->LifecycleUpdatePending();

  // TODO(b/467336183): Remove VLOGs once resolved.
  if (can_do_sync_extraction || NeedsSyncExtraction(*options)) {
    VLOG(1) << "GetAIPageContent SYNC MainFrame: "
            << GetSupplementable()->IsInMainFrame();
    GetAIPageContentSync(std::move(options), std::move(callback), start_time);
    VLOG(1) << "GetAIPageContent SYNC DONE";
    return;
  }

  VLOG(1) << "GetAIPageContent ASYNC";

  EnsureLifecycleObserverRegistered();

  // We don't expect many overlapping calls to this service as the browser will
  // only issue one request at a time.
  async_extraction_tasks_.push_back(blink::BindOnce(
      &AIPageContentAgent::GetAIPageContentSync, WrapWeakPersistent(this),
      std::move(options), std::move(callback), start_time));
}

void AIPageContentAgent::GetAIPageContentSync(
    mojom::blink::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback,
    base::TimeTicks start_time) const {
  const auto sync_start_time = base::TimeTicks::Now();

  auto content = GetAIPageContentInternal(*options);
  if (!content) {
    std::move(callback).Run(nullptr);
    return;
  }

  const auto end_time = base::TimeTicks::Now();
  RecordLatencyMetrics(start_time, sync_start_time, end_time,
                       GetSupplementable()->GetFrame()->IsOutermostMainFrame(),
                       *options);
  std::move(callback).Run(std::move(content));
}

void AIPageContentAgent::EnsureLifecycleObserverRegistered() {
  if (is_lifecycle_observer_registered_) {
    return;
  }
  DCHECK(GetSupplementable());
  if (auto* view = GetSupplementable()->View()) {
    view->RegisterForLifecycleNotifications(this);
    is_lifecycle_observer_registered_ = true;
  }
}

#if DCHECK_IS_ON()
void AIPageContentAgent::MaybeRunAutomaticActionableExtraction() {
  if (!is_auto_actionable_extraction_pending_) {
    return;
  }

  if (!GetSupplementable()->LoadEventFinished()) {
    return;
  }
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame) {
    return;
  }

  is_auto_actionable_extraction_pending_ = false;

  mojom::blink::AIPageContentOptions options;
  options.on_critical_path = true;
  options.mode = mojom::blink::AIPageContentMode::kActionableElements;

  GetAIPageContentInternal(options);
}
#endif

String AIPageContentAgent::DumpContentNodeTreeForTest() {
  mojom::blink::AIPageContentOptions options;
  options.on_critical_path = true;
  options.mode = mojom::blink::AIPageContentMode::kActionableElements;
  auto content = GetAIPageContentInternal(options);
  CHECK(content);
  CHECK(content->root_node);

  return ContentNodeTreeToString(content->root_node.get());
}

String AIPageContentAgent::DumpContentNodeForTest(Node* node) {
  CHECK(node);

  mojom::blink::AIPageContentOptions options;
  options.on_critical_path = true;
  options.mode = mojom::blink::AIPageContentMode::kActionableElements;
  auto content = GetAIPageContentInternal(options);
  CHECK(content);
  CHECK(content->root_node);

  DOMNodeId target_id = node->GetDomNodeId();
  if (target_id == kInvalidDOMNodeId) {
    return "Error: node has no DOMNodeId";
  }

  const mojom::blink::AIPageContentNode* found_node =
      FindContentNode(content->root_node.get(), target_id);

  if (!found_node) {
    return "Error: content node not found for the given DOM node";
  }

  return ContentNodeToString(found_node, /*format_on_single_line=*/false);
}

mojom::blink::AIPageContentPtr AIPageContentAgent::GetAIPageContentInternal(
    const mojom::blink::AIPageContentOptions& options) const {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->View()) {
    return nullptr;
  }

  ContentBuilder builder(options);
  return builder.Build(*frame);
}

AIPageContentAgent::ContentBuilder::ContentBuilder(
    const mojom::blink::AIPageContentOptions& options)
    : options_(options) {}

AIPageContentAgent::ContentBuilder::~ContentBuilder() = default;

mojom::blink::AIPageContentPtr AIPageContentAgent::ContentBuilder::Build(
    LocalFrame& frame) {
  TRACE_EVENT0("blink", "AIPageContentAgent::ContentBuilder::Build");
  auto& document = *frame.GetDocument();

  mojom::blink::AIPageContentPtr page_content =
      mojom::blink::AIPageContent::New();

  // Force activatable locks so content which is accessible via find-in-page is
  // styled/laid out and included when walking the tree below.
  //
  // TODO(crbug.com/387355768): Consider limiting the lock to nodes with
  // activation reason of FindInPage.
  std::vector<DisplayLockDocumentState::ScopedForceActivatableDisplayLocks>
      forced_activatable_locks;

  // If we're doing this extraction as a part of the document lifecycle, we
  // can't invalidate style/layout.
  if (!document.InvalidationDisallowed()) {
    forced_activatable_locks.emplace_back(
        document.GetDisplayLockDocumentState()
            .GetScopedForceActivatableLocks());
    document.View()->ForAllChildLocalFrameViews(
        [&](LocalFrameView& frame_view) {
          if (!frame_view.GetFrame().GetDocument()) {
            return;
          }

          forced_activatable_locks.emplace_back(
              frame_view.GetFrame()
                  .GetDocument()
                  ->GetDisplayLockDocumentState()
                  .GetScopedForceActivatableLocks());
        });
  }

  UpdateLifecycle(document);

  auto* layout_view = document.GetLayoutView();
  auto* document_style = layout_view->Style();

  // Add nodes which have a currently active user interaction (selection, focus
  // etc) before walking the tree to ensure we promote interactive DOM nodes to
  // ContentNodes.
  //
  // Note: This is different from `NodeInteractionInfo` which tracks whether a
  // node supports any interaction.
  AddPageInteractionInfo(document, *page_content);
  auto frame_data = mojom::blink::AIPageContentFrameData::New();
  AddFrameData(frame, *frame_data);
  page_content->frame_data = std::move(frame_data);

  auto root_node = MaybeGenerateContentNode(*layout_view, *document_style);
  CHECK(root_node);
  WalkChildren(*layout_view, *root_node, *document_style);
  page_content->root_node = std::move(root_node);
  page_content->visible_bounding_boxes_for_password_redaction =
      std::move(visible_bounding_box_for_passwords_);

  if (stack_depth_exceeded_) {
    ukm::builders::OptimizationGuide_AIPageContentAgent(document.UkmSourceID())
        .SetNodeDepthLimitExceeded(true)
        .Record(document.UkmRecorder());
  }

  return page_content;
}

void AIPageContentAgent::ContentBuilder::UpdateLifecycle(Document& document) {
  // Running lifecycle beyond layout is expensive and the information is only
  // needed to compute geometry. Limit the update to layout if we don't need
  // the geometry.
  if (actionable_mode()) {
    document.View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kUnknown);
  } else {
    document.View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kUnknown);
  }

  // If there's any popup opened, update the popup as well.
  WebViewImpl* web_view = document.GetPage()->GetChromeClient().GetWebView();
  WebPagePopupImpl* web_popup = web_view->GetPagePopup();
  if (!web_popup) {
    return;
  }
  LocalDOMWindow* popup_window = web_popup->Window();
  if (!popup_window) {
    return;
  }

  LocalFrame* popup_frame = popup_window->GetFrame();
  if (!popup_frame) {
    return;
  }

  Document* popup_document = popup_frame->GetDocument();
  if (!popup_document) {
    return;
  }

  if (actionable_mode()) {
    popup_document->View()->UpdateAllLifecyclePhasesExceptPaint(
        DocumentUpdateReason::kUnknown);
  } else {
    popup_document->View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kUnknown);
  }
}

void AIPageContentAgent::ContentBuilder::AddMetaData(
    const LocalFrame& frame,
    Vector<mojom::blink::AIPageContentMetaPtr>& meta_data) const {
  int max = options_->max_meta_elements;
  if (max == 0) {
    return;
  }

  int count = 0;
  const HTMLHeadElement* head = frame.GetDocument()->head();
  if (!head) {
    return;
  }
  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::ChildrenOf(*head)) {
    auto name = meta_element.GetName();
    if (name.empty()) {
      continue;
    }
    auto meta = mojom::blink::AIPageContentMeta::New();
    meta->name = name;
    auto content = meta_element.Content();
    if (content.empty()) {
      meta->content = "";
    } else {
      meta->content = content;
    }
    meta_data.push_back(std::move(meta));
    count++;
    if (count >= max) {
      break;
    }
  }
}

bool AIPageContentAgent::ContentBuilder::IsGenericContainer(
    const LayoutObject& object,
    const mojom::blink::AIPageContentAttributes& attributes) const {
  if (object.Style()->GetPosition() == EPosition::kFixed) {
    return true;
  }

  if (object.Style()->GetPosition() == EPosition::kSticky) {
    return true;
  }

  // This has some duplication with the scrollability in InteractionInfo but is
  // still required for 2 reasons:
  // 1. The interaction info is only computed when actionable elements are
  //    requested.
  // 2. The interaction info is meant to capture the current state (is the
  //    element scrollable given the current content). This is a heuristic to
  //    decide whether a node is likely to be a "container" based on the author
  //    making it scrollable.
  // TODO(khushalsagar): Consider removing this, no consumer relies on this
  // behaviour.
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

  if (!attributes.annotated_roles.empty()) {
    return true;
  }

  if (attributes.node_interaction_info) {
    return true;
  }

  if (attributes.label_for_dom_node_id) {
    return true;
  }

  // Use `ExistingIdForNode` since an Id should have already been generated if
  // this node is interactive.
  if (interactive_dom_node_ids_.contains(
          DOMNodeIds::ExistingIdForNode(object.GetNode()))) {
    return true;
  }

  return false;
}

void AIPageContentAgent::ContentBuilder::AddInteractiveNode(
    DOMNodeId dom_node_id) {
  CHECK_NE(dom_node_id, kInvalidDOMNodeId);
  interactive_dom_node_ids_.insert(dom_node_id);
}

bool AIPageContentAgent::ContentBuilder::WalkChildren(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const RecursionData& recursion_data) {
  if (object.ChildPrePaintBlockedByDisplayLock()) {
    return false;
  }

  // The max tree depth is the mojo kMaxRecursionDepth minus a buffer to leave
  // room for the root node, attributes of the final node, and mojo wrappers
  // used in message creation.
  static const int kMaxTreeDepth = kMaxRecursionDepth - 8;
  if (recursion_data.stack_depth > kMaxTreeDepth) {
    stack_depth_exceeded_ = true;
    return false;
  }

  bool has_visible_content = false;
  for (auto* child = object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (ShouldSkipSubtree(*child)) {
      continue;
    }

    RecursionData child_recursion_data(recursion_data);
    auto* child_element = DynamicTo<Element>(child->GetNode());
    if (!child_recursion_data.is_aria_disabled && child_element &&
        AXObject::IsAriaAttributeTrue(*child_element,
                                      html_names::kAriaDisabledAttr)) {
      child_recursion_data.is_aria_disabled = true;
    }

    has_visible_content |= IsVisible(*child);

    bool child_has_visible_content = false;
    auto child_content_node =
        MaybeGenerateContentNode(*child, child_recursion_data);
    if (!ShouldSkipDescendants(child_content_node)) {
      if (child_content_node) {
        child_recursion_data.stack_depth++;
      }

      auto& node_for_child =
          child_content_node ? *child_content_node : content_node;
      child_has_visible_content =
          WalkChildren(*child, node_for_child, child_recursion_data);
      has_visible_content |= child_has_visible_content;
    }

    const bool should_add_node_for_child =
        IsVisible(*child) || child_has_visible_content;
    if (should_add_node_for_child && child_content_node) {
      content_node.children_nodes.emplace_back(std::move(child_content_node));
    }
  }

  return has_visible_content;
}

void AIPageContentAgent::ContentBuilder::ProcessIframe(
    const LayoutIFrame& object,
    mojom::blink::AIPageContentNode& content_node,
    const RecursionData& recursion_data) {
  CHECK(IsVisible(object));

  content_node.content_attributes->attribute_type =
      mojom::blink::AIPageContentAttributeType::kIframe;

  auto& frame = object.ChildFrameView()->GetFrame();

  auto iframe_data = mojom::blink::AIPageContentIframeData::New();
  iframe_data->frame_token = frame.GetFrameToken();

  content_node.content_attributes->iframe_data = std::move(iframe_data);

  auto* local_frame = DynamicTo<LocalFrame>(frame);
  if (!local_frame) {
    return;
  }

  if (options_->include_same_site_only && !frame.IsOutermostMainFrame()) {
    const SecurityOrigin* frame_origin =
        local_frame->GetSecurityContext()->GetSecurityOrigin();
    const SecurityOrigin* main_frame_origin =
        local_frame->Top()->GetSecurityContext()->GetSecurityOrigin();
    CHECK(frame_origin);
    CHECK(main_frame_origin);
    if (!frame_origin->IsSameSiteWith(main_frame_origin)) {
      content_node.content_attributes->iframe_data->content =
          mojom::blink::AIPageContentIframeContent::NewRedactedFrameMetadata(
              mojom::blink::RedactedFrameMetadata::New(
                  mojom::blink::RedactedFrameMetadata::Reason::kCrossSite));
      return;
    }
  }

  // Add interaction metadata before walking the tree to ensure we promote
  // interactive DOM nodes to ContentNodes.
  if (local_frame->GetDocument()) {
    auto frame_data = mojom::blink::AIPageContentFrameData::New();
    AddFrameData(*local_frame, *frame_data);
    content_node.content_attributes->iframe_data->content =
        mojom::blink::AIPageContentIframeContent::NewLocalFrameData(
            std::move(frame_data));
  }

  auto* child_layout_view = local_frame->ContentLayoutObject();
  if (child_layout_view) {
    RecursionData child_recursion_data(*child_layout_view->Style());
    // The aria attribute values don't pierce frame boundaries.
    child_recursion_data.is_aria_disabled = false;
    child_recursion_data.stack_depth = recursion_data.stack_depth + 1;

    // Add a node for the iframe's LayoutView for consistency with remote
    // frames.
    auto child_content_node =
        MaybeGenerateContentNode(*child_layout_view, child_recursion_data);
    CHECK(child_content_node);

    // We could consider removing an iframe with no visible content. But this
    // is likely not common and should be done in the browser so it's
    // consistently done for local and remote frames.
    WalkChildren(*child_layout_view, *child_content_node, child_recursion_data);
    content_node.children_nodes.emplace_back(std::move(child_content_node));
  }
}

mojom::blink::AIPageContentNodePtr
AIPageContentAgent::ContentBuilder::MaybeGenerateContentNode(
    const LayoutObject& object,
    const RecursionData& recursion_data) {
  auto content_node = mojom::blink::AIPageContentNode::New();
  content_node->content_attributes =
      mojom::blink::AIPageContentAttributes::New();
  mojom::blink::AIPageContentAttributes& attributes =
      *content_node->content_attributes;

  // Compute state that is used to decide whether this node generates a
  // ContentNode before making the decision below.
  AddAnnotatedRoles(object, attributes.annotated_roles);
  AddForDomNodeId(object, attributes);
  // Interaction info depends on aria role.
  AddAriaRole(object, attributes);
  AddNodeInteractionInfo(object, attributes, recursion_data.is_aria_disabled);

  // Set the attribute type and add any special attributes if the attribute type
  // requires it.
  auto* element = DynamicTo<HTMLElement>(object.GetNode());
  if (const auto* iframe = GetIFrame(object)) {
    // If the `iframe` is invisible, it's Document can't override this and must
    // also be invisible.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessIframe(*iframe, *content_node, recursion_data);
  } else if (object.IsLayoutView()) {
    attributes.attribute_type = mojom::blink::AIPageContentAttributeType::kRoot;
  } else if (object.IsText()) {
    // Since text is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessTextNode(To<LayoutText>(object), attributes,
                    recursion_data.document_style);
  } else if (object.IsImage() || object.IsSVGImage()) {
    // Since image is a leaf node, do not create a content node if should skip
    // content.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessImageNode(object, attributes);
  } else if (object.IsSVGRoot()) {
    // Since we add the full text under SVG directly, don't add anything if the
    // SVG is hidden.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessSVGRoot(To<LayoutSVGRoot>(object), attributes);
  } else if (object.IsCanvas()) {
    // No content will be rendered if the canvas is hidden.
    if (!IsVisible(object)) {
      return nullptr;
    }
    ProcessCanvasNode(To<LayoutHTMLCanvas>(object), attributes);
  } else if (const auto* video_element =
                 DynamicTo<HTMLVideoElement>(object.GetNode())) {
    ProcessVideoNode(*video_element, attributes);
  } else if (const auto* anchor_element =
                 DynamicTo<HTMLAnchorElement>(object.GetNode())) {
    ProcessAnchorNode(*anchor_element, attributes);
  } else if (object.IsTable()) {
    ProcessTableNode(To<LayoutTable>(object), attributes);
  } else if (object.IsTableRow()) {
    ProcessTableRowNode(To<LayoutTableRow>(object), attributes);
  } else if (object.IsTableCell()) {
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kTableCell;
  } else if (const auto* form_element =
                 DynamicTo<HTMLFormElement>(object.GetNode())) {
    ProcessFormNode(*form_element, attributes);
  } else if (const auto* form_control =
                 DynamicTo<HTMLFormControlElement>(object.GetNode())) {
    ProcessFormControlNode(*form_control, attributes);
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
  } else if (IsGenericContainer(object, attributes)) {
    // Be sure to set annotated roles before calling IsGenericContainer, as
    // IsGenericContainer will check for annotated roles.
    // Keep container at the bottom of the list as it is the least specific.
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kContainer;
  } else {
    // If no attribute type was set, do not generate a content node.
    return nullptr;
  }

  if (auto dom_node_id = GetDomNodeId(object)) {
    attributes.dom_node_id = *dom_node_id;
  }

  AddNodeGeometry(object, attributes);
  AddLabel(object, attributes);

  attributes.is_ad_related = element && element->IsAdRelated();

  return content_node;
}

void AIPageContentAgent::ContentBuilder::AddLabel(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (!actionable_mode()) {
    return;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    return;
  }

  // TODO(khushalsagar): Look at `AXNodeObject::TextAlternative` which has other
  // sources for this.
  StringBuilder accumulated_text;
  const auto& aria_label =
      element->FastGetAttribute(html_names::kAriaLabelAttr);
  if (!aria_label.GetString().ContainsOnlyWhitespaceOrEmpty()) {
    accumulated_text.Append(aria_label);
  }

  const GCedHeapVector<Member<Element>>* aria_labelledby_elements =
      element->ElementsFromAttributeOrInternals(
          html_names::kAriaLabelledbyAttr);
  if (!aria_labelledby_elements) {
    attributes.label = accumulated_text.ToString();
    return;
  }

  for (const auto& label_element : *aria_labelledby_elements) {
    // We need to use textContent instead of innerText since aria labelled by
    // nodes don't need to be in the layout.
    auto text_content = label_element->textContent(true);
    if (text_content.ContainsOnlyWhitespaceOrEmpty()) {
      continue;
    }

    if (!accumulated_text.empty()) {
      accumulated_text.Append(" ");
    }

    accumulated_text.Append(text_content);
  }

  attributes.label = accumulated_text.ToString();
}

void AIPageContentAgent::ContentBuilder::AddForDomNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) const {
  if (!actionable_mode()) {
    return;
  }

  auto* label = DynamicTo<HTMLLabelElement>(object.GetNode());
  if (!label) {
    return;
  }

  auto* control = label->Control();
  if (!control) {
    return;
  }

  attributes.label_for_dom_node_id = DOMNodeIds::IdForNode(control);
}

void AIPageContentAgent::ContentBuilder::AddAnnotatedRoles(
    const LayoutObject& object,
    Vector<mojom::blink::AIPageContentAnnotatedRole>& annotated_roles) const {
  const auto& style = object.StyleRef();
  if (style.ContentVisibility() == EContentVisibility::kHidden) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kContentHidden);
  }

  // Element specific roles below.
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
  if (paid_content_.IsPaidElement(element)) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kPaidContent);
  }
}

void AIPageContentAgent::ContentBuilder::TrackPasswordRedactionIfNeeded(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes,
    std::optional<gfx::Rect> visible_bounding_box) {
  if (!options_->include_passwords_for_redaction) {
    return;
  }

  if (!attributes.form_control_data) {
    return;
  }

  switch (attributes.form_control_data->redaction_decision) {
    case mojom::blink::AIPageContentRedactionDecision::kNoRedactionNecessary:
    case mojom::blink::AIPageContentRedactionDecision::
        kUnredacted_EmptyPassword:
      return;
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_HasBeenPassword:
      break;
  }

  visible_bounding_box_for_passwords_.push_back(
      visible_bounding_box.value_or(ComputeVisibleBoundingBox(object)));
}

void AIPageContentAgent::ContentBuilder::AddNodeGeometry(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) {
  // When in non-actionable mode, we only want to add geometry for the
  // accessibility focused node.
  if (!actionable_mode() &&
      attributes.dom_node_id != accessibility_focused_node_id_) {
    TrackPasswordRedactionIfNeeded(object, attributes);
    return;
  }

  // Layout must be complete before computing geometry.
  DCHECK(object.GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "AddNodeGeometry only works when layout is complete for object: "
      << object;

  attributes.geometry = mojom::blink::AIPageContentGeometry::New();
  mojom::blink::AIPageContentGeometry& geometry = *attributes.geometry;

  // Compute the two fundamental bounding boxes:
  //
  // 1. outer_bounding_box: The object's full bounding box in viewport
  //    coordinates, ignoring all ancestor clipping (including the viewport
  //    clip). This includes the entire object regardless of viewport
  //    visibility. The origin is relative to the viewport; negative values
  //    indicate the object begins above/left of the viewport.
  //
  // 2. visible_bounding_box: The portion visible in the viewport, expressed in
  //    viewport coordinates after applying all ancestor and viewport clipping.
  //
  // These boxes serve different purposes:
  // - outer_bounding_box: Used for hit-testing semantics and determining the
  //   object’s overall size and position relative to the viewport.
  // - visible_bounding_box: Used for determining what is actually visible to
  //   users and immediately hit-testable without scrolling.
  geometry.outer_bounding_box = ComputeOuterBoundingBox(object);
  geometry.visible_bounding_box = ComputeVisibleBoundingBox(object);
  TrackPasswordRedactionIfNeeded(object, attributes,
                                 geometry.visible_bounding_box);

  // Validate the relationship between outer and visible bounding boxes
  // TODO(aleventhal): restore for Canary builds.
#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::AIPageContentCheckGeometryEnabled()) {
    ValidateBoundingBoxes(geometry.outer_bounding_box,
                          geometry.visible_bounding_box, object);
  }
#endif

  // Compute fragment bounding boxes for objects that split across multiple
  // lines or containers (fragmentation). This happens when:
  // - Text wraps across multiple lines
  // - Content splits across columns (CSS multi-column layout)
  //
  // Fragment boxes help understand the visual layout of split content.
  ComputeFragmentBoundingBoxes(object, geometry);
}

void AIPageContentAgent::ContentBuilder::ComputeHitTestableNodesInViewport(
    const LocalFrame& frame) {
  if (!actionable_mode()) {
    return;
  }

  const Document& document = *frame.GetDocument();
  if (!document.GetLayoutView()) {
    return;
  }

  const auto viewport_rect =
      ComputeVisibleBoundingBox(*document.GetLayoutView());
  if (viewport_rect.IsEmpty()) {
    return;
  }

  const auto local_visible_viewport_rect =
      document.GetLayoutView()->AbsoluteToLocalRect(PhysicalRect(viewport_rect),
                                                    kMapToViewportFlags);
  HitTestLocation location(local_visible_viewport_rect);

  std::vector<DOMNodeId> hit_nodes;
  HitTestRequest::HitNodeCb hit_node_cb =
      BindRepeating(&CollectHitTestNodes, std::ref(hit_nodes));
  HitTestRequest request(
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
          HitTestRequest::kListBased | HitTestRequest::kPenetratingList |
          HitTestRequest::kAvoidCache | HitTestRequest::kHitNodeCbWithId,
      nullptr, std::move(hit_node_cb));
  HitTestResult result(request, location);
  document.GetLayoutView()->HitTest(location, result);

  // TODO(averge): At this point, hit_nodes may contain duplicates due to
  // multiple passes over the same node while hit testing. These need to
  // be filtered out. The most correct approach is probably to keep the first
  // occurrence of each node, because it's more likely it was added in a later
  // paint phase, which is more representative of what the page actually looks
  // like to the user (or actor).
  //
  // result.ListBasedTestResult() already returns a NodeSet with predictable
  // iteration order based on order of insertion, which is a fancy way of saying
  // it already handles duplicates in exactly the way we need. We should eval
  // using the NodeSet result directly, and if we see improvement, remove
  // hit_nodes and the associated callback entirely.
  if (base::FeatureList::IsEnabled(
          blink::features::kAIPageContentZOrderEarlyFiltering)) {
    std::vector<DOMNodeId> nodes_from_result;
    for (auto& gc_member : result.ListBasedTestResult()) {
      Node& node = *gc_member;
      if (node.GetLayoutObject()) {
        nodes_from_result.push_back(DOMNodeIds::IdForNode(&node));
      }
    }

    hit_nodes = nodes_from_result;
  }

  int32_t next_z_order = 1;
  for (DOMNodeId node_id : base::Reversed(hit_nodes)) {
    if (dom_node_to_z_order_.Contains(node_id)) {
      continue;
    }

    auto* node = DOMNodeIds::NodeForId(node_id);
    CHECK(node);

    if (!node->IsDocumentNode() &&
        !document.ElementForHitTest(node,
                                    TreeScope::HitTestPointType::kInternal)) {
      continue;
    }
    dom_node_to_z_order_.insert(node_id, next_z_order++);
  }
}

void AIPageContentAgent::ContentBuilder::AddPageInteractionInfo(
    const Document& document,
    mojom::blink::AIPageContent& page_content) {
  page_content.page_interaction_info =
      mojom::blink::AIPageContentPageInteractionInfo::New();
  mojom::blink::AIPageContentPageInteractionInfo& page_interaction_info =
      *page_content.page_interaction_info;

  // Focused element
  if (Element* element = document.FocusedElement()) {
    page_interaction_info.focused_dom_node_id = DOMNodeIds::IdForNode(element);
    AddInteractiveNode(*page_interaction_info.focused_dom_node_id);
  }

  // Accessibility focus
  if (AXObjectCache* ax_object_cache = document.ExistingAXObjectCache()) {
    if (Node* ax_focused_node = ax_object_cache->GetAccessibilityFocus()) {
      accessibility_focused_node_id_ = DOMNodeIds::IdForNode(ax_focused_node);
      page_interaction_info.accessibility_focused_dom_node_id =
          accessibility_focused_node_id_;
      AddInteractiveNode(
          *page_interaction_info.accessibility_focused_dom_node_id);
    }
  }

  // Mouse location
  LocalFrame* frame = document.GetFrame();
  CHECK(frame);
  EventHandler& event_handler = frame->GetEventHandler();
  page_interaction_info.mouse_position =
      gfx::ToRoundedPoint(event_handler.LastKnownMousePositionInRootFrame());
}

void AIPageContentAgent::ContentBuilder::AddFrameData(
    LocalFrame& frame,
    mojom::blink::AIPageContentFrameData& frame_data) {
  frame_data.frame_interaction_info =
      mojom::blink::AIPageContentFrameInteractionInfo::New();
  frame_data.title = frame.GetDocument()->title();
  AddFrameInteractionInfo(frame, *frame_data.frame_interaction_info);
  AddMetaData(frame, frame_data.meta_data);

  if (RuntimeEnabledFeatures::AIPageContentPaidContentAnnotationEnabled()) {
    if (paid_content_.QueryPaidElements(*frame.GetDocument())) {
      frame_data.contains_paid_content = true;
    }
  }

  ComputeHitTestableNodesInViewport(frame);

  if (auto* model_context = ModelContextSupplement::GetIfExists(
          *frame.DomWindow()->navigator())) {
    model_context->ForEachScriptTool([&](const mojom::blink::ScriptTool& tool) {
      frame_data.script_tools.push_back(tool.Clone());
    });
  }

  MaybeAddPopupData(frame, frame_data);
}

void AIPageContentAgent::ContentBuilder::AddFrameInteractionInfo(
    const LocalFrame& frame,
    mojom::blink::AIPageContentFrameInteractionInfo& frame_interaction_info) {
  // Selection
  if (!frame.SelectedText().empty()) {
    frame_interaction_info.selection =
        mojom::blink::AIPageContentSelection::New();
    mojom::blink::AIPageContentSelection& selection =
        *frame_interaction_info.selection;
    selection.selected_text = frame.SelectedText();

    const SelectionInDOMTree& frame_selection =
        frame.Selection().GetSelectionInDOMTree();
    const Position& start_position = frame_selection.ComputeStartPosition();
    const Position& end_position = frame_selection.ComputeEndPosition();
    Node* start_node = start_position.ComputeContainerNode();
    Node* end_node = end_position.ComputeContainerNode();

    if (start_node) {
      selection.start_dom_node_id = DOMNodeIds::IdForNode(start_node);
      AddInteractiveNode(selection.start_dom_node_id);

      selection.start_offset = start_position.ComputeOffsetInContainerNode();
    }

    if (end_node) {
      selection.end_dom_node_id = DOMNodeIds::IdForNode(end_node);
      AddInteractiveNode(selection.end_dom_node_id);

      selection.end_offset = end_position.ComputeOffsetInContainerNode();
    }
  }
}

void AIPageContentAgent::ContentBuilder::MaybeAddPopupData(
    LocalFrame& frame,
    mojom::blink::AIPageContentFrameData& frame_data) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kAIPageContentIncludePopupWindows)) {
    return;
  }

  // Check for an open popup window.
  WebViewImpl* web_view = frame.GetPage()->GetChromeClient().GetWebView();
  if (!web_view->HasOpenedPopup()) {
    return;
  }

  // Fetch the popup window and the element that opened it.
  WebPagePopupImpl* web_popup = web_view->GetPagePopup();
  Element& opener = web_popup->OwnerElement();

  // Only fill AIPageContentPopup if this frame owns the popup.
  if (opener.GetDocument() != frame.GetDocument()) {
    return;
  }
  LocalDOMWindow* popup_window = web_popup->Window();
  if (!popup_window) {
    return;
  }
  LocalFrame* popup_frame = popup_window->GetFrame();
  if (!popup_frame) {
    return;
  }
  LayoutView* web_popup_layout_view = popup_frame->ContentLayoutObject();
  if (!web_popup_layout_view) {
    return;
  }

  ComputeHitTestableNodesInViewport(*popup_frame);

  auto mojom_popup = mojom::blink::AIPageContentPopup::New();
  // Build the ContentNode tree.
  auto web_popup_root_node = MaybeGenerateContentNode(
      *web_popup_layout_view, *web_popup_layout_view->Style());
  CHECK(web_popup_root_node);
  WalkChildren(*web_popup_layout_view, *web_popup_root_node,
               *web_popup_layout_view->Style());

  // Currently the geometry for popup nodes is relative to the popup, offset to
  // relative to the main frame.
  gfx::Rect main_frame_view_rect_in_dips =
      options_->main_frame_view_rect_in_dips;
  gfx::Rect popup_view_rect_in_dips =
      static_cast<WebPagePopup*>(web_popup)->ViewRect();
  gfx::Vector2d offset_in_dips =
      popup_view_rect_in_dips.OffsetFromOrigin() -
      main_frame_view_rect_in_dips.OffsetFromOrigin();

  FrameWidget* local_frame_widget = frame.GetWidgetForLocalRoot();
  CHECK(local_frame_widget);
  gfx::Point offset_in_pixels =
      gfx::ToFlooredPoint(local_frame_widget->DIPsToBlinkSpace(
          gfx::PointF(gfx::Point() + offset_in_dips)));
  OffsetNodeGeometry(*web_popup_root_node, offset_in_pixels.OffsetFromOrigin());

  // The view_rect is relative to the screen while geometry in APC is relative
  // to the web content's viewport, i.e, the origin where the main frame's
  // content starts rendering. Therefore offsetting to be relative to the main
  // frame.
  popup_view_rect_in_dips.Offset(
      -main_frame_view_rect_in_dips.OffsetFromOrigin());
  mojom_popup->visible_bounding_box =
      ToEnclosingRect(local_frame_widget->DIPsToBlinkSpace(
          gfx::RectF(popup_view_rect_in_dips)));

  mojom_popup->root_node = std::move(web_popup_root_node);

  // Add identifier for the node which opened the popup.
  mojom_popup->opener_dom_node_id = opener.GetDomNodeId();

  frame_data.popup = std::move(mojom_popup);
}

void AIPageContentAgent::ContentBuilder::AddInteractionInfoForHitTesting(
    const Node* node,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) const {
  if (!actionable_mode()) {
    return;
  }

  DOMNodeId dom_node_id = DOMNodeIds::ExistingIdForNode(node);
  if (dom_node_id <= kInvalidDOMNodeId) {
    return;
  }

  auto it = dom_node_to_z_order_.find(dom_node_id);
  if (it != dom_node_to_z_order_.end()) {
    interaction_info.document_scoped_z_order = it->value;
  }
}

void AIPageContentAgent::ContentBuilder::AddAriaRole(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) {
  if (!actionable_mode()) {
    return;
  }

  auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    attributes.aria_role = ax::mojom::blink::Role::kUnknown;
    return;
  }

  auto aria_role = AXObject::AriaAttribute(*element, html_names::kRoleAttr);
  if (aria_role.empty()) {
    attributes.aria_role = ax::mojom::blink::Role::kUnknown;
    return;
  }

  attributes.aria_role = AXObject::FirstValidRoleInRoleString(aria_role);
}

void AIPageContentAgent::ContentBuilder::AddNodeInteractionInfo(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes,
    bool is_aria_disabled) const {
  // The node is not hit-testable which also means no interaction is supported.
  const ComputedStyle& style = *object.Style();
  if (style.UsedPointerEvents() == EPointerEvents::kNone) {
    return;
  }

  const auto* node = object.GetNode();
  if (!node) {
    return;
  }

  // Nodes which are not interactive can still consume events if they are
  // hit-testable.
  auto node_interaction_info =
      mojom::blink::AIPageContentNodeInteractionInfo::New();
  AddInteractionInfoForHitTesting(node, *node_interaction_info);

  auto* element = DynamicTo<Element>(object.GetNode());
  bool is_disabled = false;
  if (element) {
    is_disabled = AddInteractionDisabledReasons(*element, is_aria_disabled,
                                                *node_interaction_info);
  }

  // TODO(linnan): Remove `is_disabled` when consumers move to use
  // `interaction_disabled_reasons`.
  if (is_disabled) {
    if (node_interaction_info->document_scoped_z_order) {
      attributes.node_interaction_info = std::move(node_interaction_info);
      // `is_disabled` is only set for nodes with `document_scoped_z_order`.
      // This implies offscreen nodes will not be marked as disabled.
      attributes.node_interaction_info->is_disabled = true;
    }

    return;
  }

  ComputeScrollerInfo(object, *node_interaction_info);

  // If experimental data is disabled, only scrollable nodes are included.
  if (!actionable_mode()) {
    if (node_interaction_info->scroller_info) {
      attributes.node_interaction_info = std::move(node_interaction_info);
    }

    return;
  }

  if (element) {
    AddClickabilityReasons(*element, *attributes.aria_role,
                           *node_interaction_info);
    node_interaction_info->is_focusable =
        element->IsFocusable(Element::UpdateBehavior::kAssertNoLayoutUpdates);
  }

  const bool needs_interaction_info =
      node_interaction_info->scroller_info ||
      node_interaction_info->is_focusable ||
      node_interaction_info->document_scoped_z_order ||
      !node_interaction_info->clickability_reasons.empty();

  if (!needs_interaction_info) {
    return;
  }

  attributes.node_interaction_info = std::move(node_interaction_info);
}

AIPageContentAgent::ContentBuilder::RecursionData::RecursionData(
    const ComputedStyle& document_style)
    : document_style(document_style) {}

}  // namespace blink
