// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

#include <algorithm>

#include "base/check.h"
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
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
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
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
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
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_debug_utils.h"
#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_redaction_heuristics.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
#if DCHECK_IS_ON()
bool IsVisualViewportAlignedWithLayoutViewport(Document& document,
                                               LocalFrameView& view) {
  VisualViewport& visual_viewport = document.GetPage()->GetVisualViewport();
  PaintLayerScrollableArea* layout_viewport = view.LayoutViewport();

  // "Layout viewport" vs "visual viewport" terminology:
  // `docs/website/site/developers/design-documents/blink-coordinate-spaces/`
  // "visual viewport" APIs:
  // `third_party/blink/renderer/core/frame/visual_viewport.h`

  // If the scroll offsets differ, the viewports are not aligned.
  if (visual_viewport.GetScrollOffset() != layout_viewport->GetScrollOffset()) {
    return false;
  }

  // A non-zero browser-controls adjustment (e.g. mobile toolbar show/hide)
  // shifts the visual-viewport coordinate space even if the scroll offsets
  // match, so treat that as misaligned for these DCHECKs.
  if (visual_viewport.BrowserControlsAdjustment() != 0.f) {
    return false;
  }

  // If pinch-zoom is active (scale != 1), the visual viewport is transformed
  // relative to the layout viewport.
  if (visual_viewport.Scale() != 1.f) {
    return false;
  }

  return true;
}

class AutoBuildHelper final : public NativeEventListener {
 public:
  explicit AutoBuildHelper(AIPageContentAgent& agent) : agent_(agent) {}

  void Trace(Visitor* visitor) const override {
    EventListener::Trace(visitor);
    visitor->Trace(agent_);
  }

  void StartListening() {
    AIPageContentAgent* agent = agent_.Get();
    if (!agent) {
      return;
    }
    Document* document = agent->GetSupplementable();
    if (!document || listener_registered_) {
      return;
    }
    document->addEventListener(event_type_names::kDOMContentLoaded, this,
                               false);
    listener_registered_ = true;
  }

  void Invoke(ExecutionContext* execution_context, Event* event) override {
    DCHECK(execution_context->IsWindow());

    LocalDOMWindow& window = *To<LocalDOMWindow>(execution_context);
    Document& document = *window.document();
    AIPageContentAgent* agent = agent_.Get();
    if (!agent || agent->GetSupplementable() != &document) {
      return;
    }
    RunAfterDOMContentLoaded();
  }

  void RunAfterDOMContentLoaded() {
    AIPageContentAgent* agent = agent_.Get();
    if (!agent) {
      return;
    }
    Document* document = agent->GetSupplementable();
    if (!document) {
      return;
    }
    // When tracing is active (e.g. inspector-protocol tracing tests like
    // http/tests/inspector-protocol/tracing/rendering.js), APC auto-build can
    // trigger the list-based hit test used for actionable extraction. Inspector
    // tracing records list-based hit tests with `listBased`/`rect` fields
    // instead of `nodeId`/`nodeName`, which changes rendering-expected.txt.
    // Skip auto- build to keep those tests stable.
    if (base::TrackEvent::IsEnabled()) {
      return;
    }

    // Reuse the common APC request path so lifecycle handling matches mojo
    // calls.
    LOG(INFO) << "\n" << agent->DumpContentNodeTreeForTest();
  }

 private:
  Member<AIPageContentAgent> agent_;
  bool listener_registered_ = false;
};
#endif  // DCHECK_IS_ON()

namespace {

String ReplaceUnpairedSurrogates(const String& node_text) {
  if (!RuntimeEnabledFeatures::AIPageContentConvertNodeTextToUtf8Enabled()) {
    return node_text;
  }

  if (node_text.IsNull() || node_text.empty()) {
    return node_text;
  }

  // These strings need to be converted between formats (mojom, protobuf, etc)
  // that might have varying tolerances for encoding jank, so we should
  // sanitize them as much as possible. DOM strings may contain unpaired
  // surrogates, which are a UTF-16 construct and not technically valid
  // Unicode. Calling Utf8() here with kStrictReplacingErrors will replace
  // unpaired surrogates with the default Unicode replacement character, which
  // means that downstream consumers can correctly move between encodings
  // (UTF-8, etc).
  //
  // But Utf8() conversion returns a std::string, so we need to convert back
  // to a WTF::String for Blink usage.
  //
  // Strings that are already in 8 bit representation (like Latin1 encoding)
  // can't contain unpaired surrogates, so we can return those transparently.
  if (node_text.Is8Bit()) {
    return node_text;
  }

  return String::FromUTF8(
      node_text.Utf8(Utf8ConversionMode::kStrictReplacingErrors));
}

String ConvertNodeTextToUtf8(const AtomicString& node_text) {
  return ReplaceUnpairedSurrogates(node_text.GetString());
}

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
//
// When |local_bounding_box_out| is provided, populate it with the local-space
// bounding box used as input to the visual-rect mapping. This lets callers
// reuse the same local geometry for unclipped mapping without duplicating the
// clip-path/LocalBoundingBoxRectForAccessibility selection logic.
gfx::Rect ComputeVisibleBoundingBox(
    const LayoutObject& object,
    gfx::RectF* local_bounding_box_out = nullptr) {
  // Layout must be complete before computing bounding boxes.
  DCHECK(object.GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "ComputeVisibleBoundingBox only works when layout is complete";

  // Get the object's local bounding box before viewport clipping.
  gfx::RectF local_object_rect =
      ClipPathClipper::LocalClipPathBoundingBox(object).value_or(
          object.LocalBoundingBoxRectForAccessibility(
              LayoutObject::IncludeDescendants(false)));
  if (local_bounding_box_out) {
    *local_bounding_box_out = local_object_rect;
  }

  // Transform the local bounding box to viewport coordinates, applying:
  // 1. All CSS transforms (translate, scale, rotate, etc.)
  // 2. Scroll offsets from all ancestor scroll containers
  // 3. Clipping from overflow:hidden containers
  // 4. Viewport clipping (anything outside the viewport is clipped)
  //
  // The nullptr ancestor means "map to the root of the document". When used
  // with kVisualRectFlags, this gives us visual viewport-relative coordinates,
  // but clips to the layout viewport -- a potential source of confusion.
  // TODO(khushalsagar): It might be more optimal to derive this from output of
  // paint.
  gfx::RectF visual_viewport_relative_rect = local_object_rect;
  object.MapToVisualRectInAncestorSpace(nullptr, visual_viewport_relative_rect,
                                        kVisualRectFlags);

  // Why do we clamp/intersect after MapToVisualRectInAncestorSpace()?
  //
  // Visual vs layout viewport: the visual viewport can be offset relative to
  // the layout viewport (for example while browser controls animate during
  // scroll on mobile, or during pinch-zoom).
  //
  // In that situation, visual-rect mapping can legitimately return negative
  // coordinates even after viewport clipping. The visual viewport origin is no
  // longer at the layout viewport origin.
  //
  // APC's `visible_bounding_box` is defined to be viewport-relative with a
  // (0, 0) origin. Intersect with the local-root viewport to normalize into
  // that coordinate space and keep geometry DCHECKs from firing on valid
  // layouts.
  //
  // This is intentionally done here because MapToVisualRectInAncestorSpace()
  // clips in layout-viewport space before applying the visual-viewport
  // transform, so negative coordinates are a valid outcome for some viewport
  // configurations.
  //
  // For local subframes, the mapped rect is in local-root (main-frame)
  // coordinates, so clamp against the local-root viewport instead of the
  // subframe viewport.
  // TODO(crbug.com/474330989): Consider clamping to the visual viewport size
  // instead of the layout viewport size if those dimensions can diverge.
  if (RuntimeEnabledFeatures::AIPageContentVisualViewportClampEnabled()) {
    if (LocalFrameView* view = object.GetDocument().View()) {
      LocalFrame* frame = object.GetDocument().GetFrame();
      LocalFrameView* root_view =
          frame ? frame->LocalFrameRoot().View() : nullptr;
      LocalFrameView* viewport_view = root_view ? root_view : view;
      // LocalFrameView::ViewportWidth/Height are in CSS/layout pixels (only
      // adjusted for absolute zoom), while `visual_viewport_relative_rect` is
      // produced by MapToVisualRectInAncestorSpace() in
      // visual-viewport-relative BlinkSpace/device pixels. Convert the
      // viewport size into BlinkSpace to ensure we clamp in a consistent
      // coordinate space.
      float viewport_width_blink = viewport_view->ViewportWidth();
      float viewport_height_blink = viewport_view->ViewportHeight();
      if (frame) {
        if (FrameWidget* widget = frame->GetWidgetForLocalRoot()) {
          viewport_width_blink =
              widget->DIPsToBlinkSpace(viewport_view->ViewportWidth());
          viewport_height_blink =
              widget->DIPsToBlinkSpace(viewport_view->ViewportHeight());
        }
      }
      gfx::RectF local_root_viewport_rect_in_blink_space(
          0, 0, viewport_width_blink, viewport_height_blink);

#if DCHECK_IS_ON()
      gfx::RectF unclamped_rect = visual_viewport_relative_rect;

      const bool can_clamp_to_viewport =
          !frame || frame->LocalFrameRoot().IsMainFrame();
      if (can_clamp_to_viewport && frame && frame->IsMainFrame()) {
        const bool is_viewport_aligned =
            IsVisualViewportAlignedWithLayoutViewport(object.GetDocument(),
                                                      *viewport_view);
        if (is_viewport_aligned) {
          // When the visual viewport matches the layout viewport, the mapping
          // is expected to already be viewport-relative. Clamping should be a
          // no-op. We DCHECK this to detect any future cases where the mapping
          // still yields negative coordinates, which would indicate another
          // layout/visual-viewport discrepancy that APC should account for.
          DCHECK_GE(unclamped_rect.x(),
                    local_root_viewport_rect_in_blink_space.x());
          DCHECK_GE(unclamped_rect.y(),
                    local_root_viewport_rect_in_blink_space.y());
        }
      }
#endif
      // Skip clamping for out-of-process iframes because their geometry is
      // reported in main-frame coordinates, while the local viewport is
      // iframe-relative and would incorrectly zero out visible rects.
      if (!frame || frame->LocalFrameRoot().IsMainFrame()) {
        visual_viewport_relative_rect.Intersect(
            local_root_viewport_rect_in_blink_space);
      }
    }
  }

  gfx::Rect visible_box_in_viewport_coords =
      ToEnclosingRect(visual_viewport_relative_rect);

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

bool IsUnboundedOrSaturatedViewportRect(const gfx::Rect& rect) {
  // `InfiniteIntRect()` is a sentinel used for "unknown/unbounded" clip rects.
  if (rect == InfiniteIntRect()) {
    return true;
  }

  // When geometry is too large to represent in LayoutUnits, mapping paths that
  // go through PhysicalRect/LayoutUnit can clamp/saturate. Those saturated
  // rects are also "unknown/unbounded" for APC purposes.
  //
  // LayoutUnit uses a fixed-point int32 representation with 6 fractional bits.
  // Its maximum integer component is LayoutUnit::Max().ToInt().
  constexpr int kMaxLayoutInt = LayoutUnit::Max().ToInt();
  return rect.width() > kMaxLayoutInt || rect.height() > kMaxLayoutInt;
}

// Maps a local bounding box to an unclipped outer bounding box in viewport
// coordinates using the GeometryMapper pipeline.
gfx::Rect LocalToOuterBoundingBox(const LayoutObject& object,
                                  const gfx::RectF& local_bounding_box) {
  // This mapping intentionally skips ancestor and viewport clips while still
  // applying transforms, scroll offsets, and filters. It keeps the math aligned
  // with ComputeVisibleBoundingBox() without re-deriving local geometry.
  gfx::RectF unclipped_box = local_bounding_box;
  const bool mapped_outer = object.MapToVisualRectInAncestorSpace(
      nullptr, unclipped_box,
      static_cast<VisualRectFlags>(kVisualRectFlags |
                                   kSkipAncestorAndViewportClips));
  if (!mapped_outer || unclipped_box.IsEmpty()) {
    return gfx::Rect();
  }
  gfx::Rect outer_box = gfx::ToEnclosingRect(unclipped_box);

  // Under extreme transforms, MapToVisualRectInAncestorSpace() may saturate
  // and return blink::InfiniteIntRect() as a sentinel "unknown/unbounded"
  // rectangle. This is intentional and used by crash/fuzz-style tests (e.g.
  // WPT cases with scale(1e38)). The sentinel is not a meaningful "outer"
  // bounding box, and it may not contain the (clipped-to-viewport) visible
  // box because it ends at (0,0). Treat it as empty so callers can decide how
  // to fall back (e.g. clamp to the visible box in APC).
  if (IsUnboundedOrSaturatedViewportRect(outer_box)) {
    return gfx::Rect();
  }
  return outer_box;
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

#if DCHECK_IS_ON()
// Validates the relationship between the viewport, outer box and visible
// bounding boxes.
void ValidateBoundingBoxes(const gfx::Rect& outer_box_in_viewport_coords,
                           const gfx::Rect& visible_box_in_viewport_coords,
                           const LayoutObject& object) {
  if (visible_box_in_viewport_coords.IsEmpty()) {
    return;
  }

  // Skip validation when outer bounds are "unknown/unbounded" sentinels due to
  // extreme transforms or saturation. Containment is not meaningful then.
  if (IsUnboundedOrSaturatedViewportRect(outer_box_in_viewport_coords)) {
    return;
  }

  DCHECK(outer_box_in_viewport_coords.Contains(visible_box_in_viewport_coords))
      << "Visible box must lie within outer box. Visible: "
      << visible_box_in_viewport_coords.ToString()
      << ", Outer: " << outer_box_in_viewport_coords.ToString()
      << "\nFor: " << object;
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
bool IsHeadingTag(const Element& element) {
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

bool IsVisible(const LayoutObject& object) {
  // Don't add content when node is invisible.
  return object.Style()->Visibility() == EVisibility::kVisible;
}

bool AreChildrenBlockedByDisplayLock(const LayoutObject& object) {
  return object.ChildLayoutBlockedByDisplayLock() ||
         object.ChildPrePaintBlockedByDisplayLock();
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
  const auto* form_control_data =
      content_node->content_attributes->form_control_data.get();
  if (form_control_data) {
    const auto redaction_decision = form_control_data->redaction_decision;
    switch (redaction_decision) {
      case mojom::blink::AIPageContentRedactionDecision::kNoRedactionNecessary:
      case mojom::blink::AIPageContentRedactionDecision::
          kUnredacted_EmptyPassword:
      case mojom::blink::AIPageContentRedactionDecision::
          kUnredacted_EmptyCustomPassword:
        break;
      case mojom::blink::AIPageContentRedactionDecision::
          kRedacted_HasBeenPassword:
      case mojom::blink::AIPageContentRedactionDecision::
          kRedacted_CustomPassword_CSS:
      case mojom::blink::AIPageContentRedactionDecision::
          kRedacted_CustomPassword_JS:
        // Custom password-like inputs (e.g. `-webkit-text-security`) can also
        // have UA/editor subtrees which may contain sensitive text. Skip them
        // to avoid leaking into extracted text nodes.
        return true;
    }
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
  text_info->text_content =
      ReplaceUnpairedSurrogates(layout_text.TransformedText());
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
    image_info->image_caption =
        ReplaceUnpairedSurrogates(image_element->AltText());
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
  svg_root_data->inner_text =
      ReplaceUnpairedSurrogates(element->GetInnerTextWithoutUpdate());
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
      table_data->table_name = ReplaceUnpairedSurrogates(table_name.ToString());
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
    form_data->form_name = ReplaceUnpairedSurrogates(name);
  }
  form_data->action_url = KURL(form_element.action());

  attributes.form_data = std::move(form_data);
}

// Roles that accept aria-checked, per WAI-ARIA. We only surface aria-checked
// for these roles to avoid exposing unsupported states.
bool RoleSupportsAriaChecked(ax::mojom::blink::Role role) {
  switch (role) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kRadioButton:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTreeItem:
      return true;
    default:
      return false;
  }
}

// aria-placeholder is intended for text entry widgets, so scope to text fields
// and combobox-like roles.
bool RoleSupportsAriaPlaceholder(ax::mojom::blink::Role role) {
  return ui::IsTextField(role) || ui::IsComboBox(role);
}

// Maps ARIA roles to a FormControlType where the semantics are clear enough to
// surface APC form control data.
bool FormControlTypeForAriaRole(ax::mojom::blink::Role role,
                                mojom::blink::FormControlType* out_type) {
  switch (role) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kSwitch:
      *out_type = mojom::blink::FormControlType::kInputCheckbox;
      return true;
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kRadioButton:
      *out_type = mojom::blink::FormControlType::kInputRadio;
      return true;
    case ax::mojom::blink::Role::kSearchBox:
      *out_type = mojom::blink::FormControlType::kInputSearch;
      return true;
    case ax::mojom::blink::Role::kTextField:
    case ax::mojom::blink::Role::kTextFieldWithComboBox:
      *out_type = mojom::blink::FormControlType::kInputText;
      return true;
    default:
      return false;
  }
}

// Populates form control data for ARIA-based controls (e.g. role=checkbox).
// Returns true if ARIA form control data should be emitted for this element.
bool ProcessAriaFormControlNode(
    const LayoutObject& object,
    const Element& element,
    mojom::blink::AIPageContentAttributes& attributes) {
  // Use AXObject's raw role resolver so we only honor explicit ARIA roles and
  // ignore implicit/native roles for ARIA-only control semantics.
  ax::mojom::blink::Role aria_role = AXObject::DetermineRawAriaRole(element);
  if (aria_role == ax::mojom::blink::Role::kUnknown) {
    return false;
  }

  mojom::blink::FormControlType form_control_type;
  if (!FormControlTypeForAriaRole(aria_role, &form_control_type)) {
    return false;
  }

  bool aria_required =
      ui::SupportsRequired(aria_role) &&
      AXObject::IsAriaAttributeTrue(element, html_names::kAriaRequiredAttr);
  bool aria_readonly =
      ui::IsReadOnlySupported(aria_role) &&
      AXObject::IsAriaAttributeTrue(element, html_names::kAriaReadonlyAttr);

  String aria_placeholder;
  if (RoleSupportsAriaPlaceholder(aria_role)) {
    const AtomicString& aria_placeholder_attribute =
        AXObject::AriaAttribute(element, html_names::kAriaPlaceholderAttr);
    if (!aria_placeholder_attribute.empty()) {
      aria_placeholder = aria_placeholder_attribute;
    }
  }

  bool has_aria_checked = false;
  bool aria_checked = false;
  if (RoleSupportsAriaChecked(aria_role)) {
    // Keep aria-checked handling simple: require the attribute to be present,
    // and treat any non-false value (including "mixed") as true.
    has_aria_checked =
        AXObject::HasAriaAttribute(element, html_names::kAriaCheckedAttr);
    if (has_aria_checked) {
      aria_checked =
          AXObject::IsAriaAttributeTrue(element, html_names::kAriaCheckedAttr);
    }
  }

  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kFormControl;

  // Do not gate ARIA form control metadata on visibility. The node can still be
  // included when visible descendants keep the subtree in the APC tree, so we
  // preserve ARIA semantics for downstream consumers.
  attributes.form_control_data =
      mojom::blink::AIPageContentFormControlData::New();
  auto& form_control_data = *attributes.form_control_data;
  form_control_data.form_control_type = form_control_type;
  form_control_data.is_required = aria_required;
  form_control_data.is_readonly = aria_readonly;
  form_control_data.redaction_decision =
      mojom::blink::AIPageContentRedactionDecision::kNoRedactionNecessary;

  if (!aria_placeholder.empty()) {
    form_control_data.placeholder = ReplaceUnpairedSurrogates(aria_placeholder);
  }

  if (has_aria_checked) {
    form_control_data.is_checked = aria_checked;
  }
  return true;
}

void ProcessFormControlNode(const HTMLFormControlElement& form_control_element,
                            mojom::blink::AIPageContentAttributes& attributes) {
  attributes.attribute_type =
      mojom::blink::AIPageContentAttributeType::kFormControl;
  if (!IsVisible(*form_control_element.GetLayoutObject())) {
    return;
  }
  attributes.form_control_data =
      mojom::blink::AIPageContentFormControlData::New();
  auto* form_control_data = attributes.form_control_data.get();
  form_control_data->form_control_type = form_control_element.FormControlType();
  form_control_data->field_name =
      ConvertNodeTextToUtf8(form_control_element.GetName());
  form_control_data->is_required = form_control_element.IsRequired();
  form_control_data->is_readonly = form_control_element.IsReadOnly();

  // Honor aria-required for native controls (and aria-readonly for text
  // controls below) only when the native attributes are not set. This follows
  // WAI-ARIA host language conflict guidance by keeping native semantics
  // authoritative when present.
  if (!form_control_data->is_required &&
      AXObject::IsAriaAttributeTrue(form_control_element,
                                    html_names::kAriaRequiredAttr)) {
    form_control_data->is_required = true;
  }

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
      form_control_data->field_value =
          ReplaceUnpairedSurrogates(text_control_element->Value());
    }
    String placeholder_value = text_control_element->GetPlaceholderValue();
    if (placeholder_value.empty()) {
      const AtomicString& aria_placeholder = AXObject::AriaAttribute(
          form_control_element, html_names::kAriaPlaceholderAttr);
      if (!aria_placeholder.empty()) {
        placeholder_value = aria_placeholder;
      }
    }
    form_control_data->placeholder =
        ReplaceUnpairedSurrogates(placeholder_value);

    if (!form_control_data->is_readonly &&
        AXObject::IsAriaAttributeTrue(form_control_element,
                                      html_names::kAriaReadonlyAttr)) {
      form_control_data->is_readonly = true;
    }
  }
  if (const auto* html_input_element =
          DynamicTo<HTMLInputElement>(form_control_element)) {
    form_control_data->is_checked = html_input_element->Checked();
  }
  if (const auto* select_element =
          DynamicTo<HTMLSelectElement>(form_control_element)) {
    for (auto& option_element : select_element->GetOptionList()) {
      auto select_option = mojom::blink::AIPageContentSelectOption::New();
      select_option->value = ReplaceUnpairedSurrogates(option_element.value());
      select_option->text = ReplaceUnpairedSurrogates(option_element.text());
      if (select_option->text.empty()) {
        select_option->text =
            ReplaceUnpairedSurrogates(option_element.DisplayLabel());
      }
      select_option->is_selected = option_element.Selected();
      select_option->disabled = option_element.IsDisabledFormControl();
      form_control_data->select_options.push_back(std::move(select_option));
    }
  }
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

bool MayContainSensitvePayment(
    mojom::blink::FormControlType form_control_type) {
  switch (form_control_type) {
    case mojom::blink::FormControlType::kInputEmail:
    case mojom::blink::FormControlType::kInputMonth:
    case mojom::blink::FormControlType::kInputNumber:
    case mojom::blink::FormControlType::kInputPassword:
    case mojom::blink::FormControlType::kInputSearch:
    case mojom::blink::FormControlType::kInputTelephone:
    case mojom::blink::FormControlType::kInputText:
    case mojom::blink::FormControlType::kInputUrl:
    case mojom::blink::FormControlType::kSelectOne:
    case mojom::blink::FormControlType::kTextArea:
      return true;
    case mojom::blink::FormControlType::kInputCheckbox:
    case mojom::blink::FormControlType::kInputRadio:
    case mojom::blink::FormControlType::kInputDate:
    case mojom::blink::FormControlType::kButtonButton:
    case mojom::blink::FormControlType::kButtonSubmit:
    case mojom::blink::FormControlType::kButtonReset:
    case mojom::blink::FormControlType::kButtonPopover:
    case mojom::blink::FormControlType::kFieldset:
    case mojom::blink::FormControlType::kInputButton:
    case mojom::blink::FormControlType::kInputColor:
    case mojom::blink::FormControlType::kInputDatetimeLocal:
    case mojom::blink::FormControlType::kInputFile:
    case mojom::blink::FormControlType::kInputHidden:
    case mojom::blink::FormControlType::kInputImage:
    case mojom::blink::FormControlType::kInputRange:
    case mojom::blink::FormControlType::kInputReset:
    case mojom::blink::FormControlType::kInputSubmit:
    case mojom::blink::FormControlType::kInputTime:
    case mojom::blink::FormControlType::kInputWeek:
    case mojom::blink::FormControlType::kOutput:
    case mojom::blink::FormControlType::kSelectMultiple:
      return false;
  }

  return false;
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
  // Skip the initial empty document to avoid auto-build work before the real
  // navigation commits. Auto-builds for the initial empty document do not
  // provide useful coverage and can interfere with subsequent loads.
  if (document->IsInitialEmptyDocument()) {
    return;
  }
  auto* agent = AIPageContentAgent::GetOrCreateForTesting(*document);
  agent->ListenForDOMContentLoadedForAutoBuild();
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
#if DCHECK_IS_ON()
  visitor->Trace(auto_build_helper_);
#endif
}

void AIPageContentAgent::DidFinishPostLifecycleSteps(const LocalFrameView&) {
  for (auto& task : std::move(async_extraction_tasks_)) {
    std::move(task).Run();
  }
  async_extraction_tasks_.clear();
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
AutoBuildHelper* AIPageContentAgent::GetOrCreateAutoBuildHelper() {
  if (!auto_build_helper_) {
    auto_build_helper_ = MakeGarbageCollected<AutoBuildHelper>(*this);
  }
  return auto_build_helper_.Get();
}

void AIPageContentAgent::ListenForDOMContentLoadedForAutoBuild() {
  Document* document = GetSupplementable();
  if (!document) {
    return;
  }

  // Register the DOMContentLoaded listener and schedule the auto-build after
  // the event dispatch completes. This keeps auto-build out of document
  // installation/handler execution windows where frame state can be unstable.
  GetOrCreateAutoBuildHelper()->StartListening();
}

void AIPageContentAgent::RunAutoBuildAfterDOMContentLoadedForTesting() {
  GetOrCreateAutoBuildHelper()->RunAfterDOMContentLoaded();
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

  ContentBuilder builder(options, *this);
  return builder.Build(*frame);
}

AIPageContentAgent::ContentBuilder::ContentBuilder(
    const mojom::blink::AIPageContentOptions& options,
    const AIPageContentAgent& agent)
    : options_(options), agent_(agent) {}

AIPageContentAgent::ContentBuilder::~ContentBuilder() = default;

std::optional<AIPageContentAgent::CustomPasswordSource>
AIPageContentAgent::ExistingCustomPasswordReason(
    const LayoutObject& object) const {
  const DOMNodeId dom_node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (!dom_node_id) {
    return std::nullopt;
  }
  auto it = custom_password_decision_.find(dom_node_id);
  if (it == custom_password_decision_.end()) {
    return std::nullopt;
  }
  return it->value;
}

namespace {

DOMNodeId GetAccessibilityFocusedDOMNodeId(const LocalFrame& frame) {
  const Document* document = frame.GetDocument();
  if (!document) {
    return kInvalidDOMNodeId;
  }

  AXObjectCache* ax_object_cache = document->ExistingAXObjectCache();
  if (!ax_object_cache) {
    return kInvalidDOMNodeId;
  }

  Node* ax_focused_node = ax_object_cache->GetAccessibilityFocus();
  if (!ax_focused_node) {
    return kInvalidDOMNodeId;
  }

  return DOMNodeIds::IdForNode(ax_focused_node);
}

}  // namespace

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

  RecursionData recursion_data(*document_style);
  recursion_data.accessibility_focused_node_id =
      GetAccessibilityFocusedDOMNodeId(frame);

  auto root_node = MaybeGenerateContentNode(*layout_view, recursion_data);
  CHECK(root_node);
  WalkChildren(*layout_view, *root_node, recursion_data);
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
    meta->name = ConvertNodeTextToUtf8(name);
    auto content = meta_element.Content();
    if (content.empty()) {
      meta->content = "";
    } else {
      meta->content = ConvertNodeTextToUtf8(content);
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

  return false;
}

DOMNodeId AIPageContentAgent::ContentBuilder::AddInteractiveNode(Node& node) {
  // This intentionally creates a DOM node id when missing for metadata-linked
  // nodes (focus, selection, label-for, popup opener).
  const DOMNodeId dom_node_id = DOMNodeIds::IdForNode(&node);
  AddInteractiveNode(dom_node_id);
  return dom_node_id;
}

void AIPageContentAgent::ContentBuilder::AddInteractiveNode(
    DOMNodeId dom_node_id) {
  CHECK_NE(dom_node_id, kInvalidDOMNodeId);
  // Adding the node id to this set forces the node to be emitted with a node id
  // and a fallback attribute type of kContainer when nothing else applies.
  interactive_dom_node_ids_.insert(dom_node_id);
}

bool AIPageContentAgent::ContentBuilder::WalkChildren(
    const LayoutObject& object,
    mojom::blink::AIPageContentNode& content_node,
    const RecursionData& recursion_data) {
  if (AreChildrenBlockedByDisplayLock(object)) {
    // APC only includes content with layout objects; display-locked subtrees
    // skip child layout/prepaint, so they are not included in the layout tree.
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

  if (AreChildrenBlockedByDisplayLock(object)) {
    // Avoid forcing layout or hit-testing in display-locked iframe subtrees.
    return;
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
    child_recursion_data.accessibility_focused_node_id =
        GetAccessibilityFocusedDOMNodeId(*local_frame);

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
  PopulateLabelForDomNodeId(object, attributes);

  auto* element = DynamicTo<Element>(object.GetNode());
  if (actionable_mode() && element) {
    attributes.aria_role = AXObject::DetermineRawAriaRole(*element);
  }
  AddNodeInteractionInfo(object, attributes, recursion_data.is_aria_disabled);

  // Set the attribute type and add any special attributes if the attribute type
  // requires it.
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
    ApplyCustomPasswordRedactionHeuristicsIfNeeded(object, attributes);
  } else if (element &&
             ProcessAriaFormControlNode(object, *element, attributes)) {
    // ProcessAriaFormControlNode sets the attribute type and data.
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
  } else if (interactive_dom_node_ids_.Contains(
                 DOMNodeIds::ExistingIdForNode(object.GetNode()))) {
    // Fall back to a generic container when we need to emit this node for
    // dom_node_id purposes but no more specific type matched.
    attributes.attribute_type =
        mojom::blink::AIPageContentAttributeType::kContainer;
  } else {
    // If no attribute type was set, do not generate a content node.
    return nullptr;
  }

  // Resolve and allocate DOM ids only when output policy needs them.
  if (ShouldEmitNodeIdForOutput(object, attributes)) {
    if (const DOMNodeId dom_node_id = DOMNodeIds::IdForNode(object.GetNode())) {
      attributes.dom_node_id = dom_node_id;
    }
  }

  AddNodeGeometry(object, attributes,
                  recursion_data.accessibility_focused_node_id);
  AddLabel(object, attributes);
  attributes.is_ad_related = element && element->IsAdRelated();

  return content_node;
}

bool AIPageContentAgent::ContentBuilder::ShouldEmitNodeIdForOutput(
    const LayoutObject& object,
    const mojom::blink::AIPageContentAttributes& attributes) const {
  // Preserve existing behavior when node-id options are not provided.
  if (!options_->node_id_allowlist) {
    return true;
  }

  // Actionable nodes participate in tool execution and need an id.
  if (actionable_mode() && attributes.node_interaction_info) {
    return true;
  }

  // Metadata-linked nodes (focused element, accessibility focus, selection
  // endpoints, label-for targets, popup openers) need an id.
  if (interactive_dom_node_ids_.Contains(
          DOMNodeIds::ExistingIdForNode(object.GetNode()))) {
    return true;
  }

  // Otherwise, fall back to per-attribute options policy.
  return IsNodeIdAttributeTypeAllowlisted(attributes.attribute_type);
}

bool AIPageContentAgent::ContentBuilder::IsNodeIdAttributeTypeAllowlisted(
    mojom::blink::AIPageContentAttributeType attribute_type) const {
  if (!options_->node_id_allowlist) {
    return false;
  }
  // The policy allowlist is expected to be small. Prioritize direct readability
  // over auxiliary data structures here.
  return std::ranges::find(*options_->node_id_allowlist, attribute_type) !=
         options_->node_id_allowlist->end();
}

void AIPageContentAgent::ContentBuilder::
    ApplyCustomPasswordRedactionHeuristicsIfNeeded(
        const LayoutObject& object,
        mojom::blink::AIPageContentAttributes& attributes) const {
  // Only form controls have `form_control_data`. Keep this defensive because
  // callers may evolve and still call this helper.
  if (!attributes.form_control_data) {
    return;
  }

  // Only text controls can meaningfully contain sensitive freeform text.
  const auto* text_control_element =
      DynamicTo<TextControlElement>(object.GetNode());
  if (!text_control_element) {
    return;
  }

  // If this is already treated as a real password field, do not override the
  // existing decision. The built-in HTMLInputElement::HasBeenPasswordField()
  // logic should remain authoritative.
  switch (attributes.form_control_data->redaction_decision) {
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_HasBeenPassword:
    case mojom::blink::AIPageContentRedactionDecision::
        kUnredacted_EmptyPassword:
      return;
    case mojom::blink::AIPageContentRedactionDecision::kNoRedactionNecessary:
    case mojom::blink::AIPageContentRedactionDecision::
        kUnredacted_EmptyCustomPassword:
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_CustomPassword_CSS:
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_CustomPassword_JS:
      break;
  }

  const String value = text_control_element->Value();

  const std::optional<AIPageContentAgent::CustomPasswordSource>
      existing_custom_password_like_reason =
          agent_.ExistingCustomPasswordReason(object);
  bool is_custom_password = existing_custom_password_like_reason.has_value();
  std::optional<AIPageContentAgent::CustomPasswordSource>
      custom_password_like_reason = existing_custom_password_like_reason;
  if (!is_custom_password && !value.empty()) {
    if (IsCSSSecurityMaskingEnabled(object)) {
      custom_password_like_reason =
          AIPageContentAgent::CustomPasswordSource::kCSS;
    } else if (IsLikelyJSCustomPasswordField(value)) {
      custom_password_like_reason =
          AIPageContentAgent::CustomPasswordSource::kJavaScript;
    }
    if (custom_password_like_reason) {
      agent_.custom_password_decision_.Set(
          DOMNodeIds::IdForNode(object.GetNode()),
          *custom_password_like_reason);
      is_custom_password = true;
    }
  }

  if (!is_custom_password) {
    return;
  }

  // Preserve the classification even when empty, but only redact when there is
  // actual sensitive content present.
  if (value.empty()) {
    attributes.form_control_data->redaction_decision = mojom::blink::
        AIPageContentRedactionDecision::kUnredacted_EmptyCustomPassword;
    attributes.form_control_data->field_value =
        ReplaceUnpairedSurrogates(value);
    return;
  }

  // Clear any captured value from earlier processing and redact.
  attributes.form_control_data->field_value = String();
  CHECK(custom_password_like_reason);
  switch (*custom_password_like_reason) {
    case AIPageContentAgent::CustomPasswordSource::kCSS:
      attributes.form_control_data->redaction_decision = mojom::blink::
          AIPageContentRedactionDecision::kRedacted_CustomPassword_CSS;
      break;
    case AIPageContentAgent::CustomPasswordSource::kJavaScript:
      attributes.form_control_data->redaction_decision = mojom::blink::
          AIPageContentRedactionDecision::kRedacted_CustomPassword_JS;
      break;
  }
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
    attributes.label = ReplaceUnpairedSurrogates(accumulated_text.ToString());
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

  attributes.label = ReplaceUnpairedSurrogates(accumulated_text.ToString());
}

void AIPageContentAgent::ContentBuilder::PopulateLabelForDomNodeId(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes) {
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

  const DOMNodeId control_dom_node_id = AddInteractiveNode(*control);

  // Always emit the label target id regardless of node-id policy so
  // `label_for_dom_node_id` remains round-trippable and actions on the label
  // can resolve and trigger the associated control.
  attributes.label_for_dom_node_id = control_dom_node_id;
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
  // Use the ARIA role parser so role checks are case-insensitive and avoid
  // re-implementing token parsing in APC.
  const ax::mojom::blink::Role aria_role =
      AXObject::DetermineRawAriaRole(*element);
  if (element->HasTagName(html_names::kHeaderTag) ||
      aria_role == ax::mojom::blink::Role::kBanner) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kHeader);
  }
  if (element->HasTagName(html_names::kNavTag) ||
      aria_role == ax::mojom::blink::Role::kNavigation) {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kNav);
  }
  if (element->HasTagName(html_names::kSearchTag) ||
      aria_role == ax::mojom::blink::Role::kSearch) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSearch);
  }
  if (element->HasTagName(html_names::kMainTag) ||
      aria_role == ax::mojom::blink::Role::kMain) {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kMain);
  }
  if (element->HasTagName(html_names::kArticleTag) ||
      aria_role == ax::mojom::blink::Role::kArticle) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kArticle);
  }
  if (element->HasTagName(html_names::kSectionTag) ||
      aria_role == ax::mojom::blink::Role::kRegion) {
    annotated_roles.push_back(
        mojom::blink::AIPageContentAnnotatedRole::kSection);
  }
  if (element->HasTagName(html_names::kAsideTag) ||
      aria_role == ax::mojom::blink::Role::kComplementary) {
    annotated_roles.push_back(mojom::blink::AIPageContentAnnotatedRole::kAside);
  }
  if (element->HasTagName(html_names::kFooterTag) ||
      aria_role == ax::mojom::blink::Role::kContentInfo) {
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
    case mojom::blink::AIPageContentRedactionDecision::
        kUnredacted_EmptyCustomPassword:
      return;
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_HasBeenPassword:
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_CustomPassword_CSS:
    case mojom::blink::AIPageContentRedactionDecision::
        kRedacted_CustomPassword_JS:
      break;
  }

  visible_bounding_box_for_passwords_.push_back(
      visible_bounding_box.value_or(ComputeVisibleBoundingBox(object)));
}

bool AIPageContentAgent::ContentBuilder::ShouldAddNodeGeometry(
    const LayoutObject& object,
    const mojom::blink::AIPageContentAttributes& attributes,
    DOMNodeId accessibility_focused_node_id) const {
  if (actionable_mode()) {
    return true;
  }

  // When in non-actionable mode, we only want to add geometry for the
  // accessibility focused node.
  if (attributes.dom_node_id == accessibility_focused_node_id) {
    return true;
  }

  // When sensitive payment redaction is enabled,  the redaction decision is
  // made in the browser using Autofill data. We must provide the geometry for
  // form control elements that may contain sensitive payments here so the
  // browser can record their bounding boxes for client-side screenshot
  // redaction.
  if (options_->include_sensitive_payments_for_redaction) {
    if (const auto* form_control_element =
            DynamicTo<HTMLFormControlElement>(object.GetNode());
        form_control_element &&
        MayContainSensitvePayment(form_control_element->FormControlType())) {
      return true;
    }
  }

  return false;
}

void AIPageContentAgent::ContentBuilder::AddNodeGeometry(
    const LayoutObject& object,
    mojom::blink::AIPageContentAttributes& attributes,
    DOMNodeId accessibility_focused_node_id) {
  if (!ShouldAddNodeGeometry(object, attributes,
                             accessibility_focused_node_id)) {
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
  //   object’s overall size and position relative to the viewport once scrolled
  //   into view (ignoring ancestor clips).
  // - visible_bounding_box: Used for determining what is actually visible to
  //   users and immediately hit-testable without scrolling.
  // Compute the visible bounding box and capture the local box so the outer
  // box can reuse identical geometry inputs when the feature flag is enabled.
  gfx::RectF local_bounding_box;
  geometry.visible_bounding_box =
      ComputeVisibleBoundingBox(object, &local_bounding_box);
  const bool map_to_outer =
      RuntimeEnabledFeatures::AIPageContentOuterBoxMapToAncestorSpaceEnabled();
  geometry.outer_bounding_box =
      map_to_outer ? LocalToOuterBoundingBox(object, local_bounding_box)
                   : ComputeOuterBoundingBox(object);

  // For APC, the most useful fallback is to clamp the outer box to the visible
  // box when the mapping fails or saturates:
  // - It preserves the key invariant checked by ValidateBoundingBoxes().
  // - It avoids emitting enormous/sentinel geometry to consumers.
  // - It does not change visible_bounding_box semantics.
  if (map_to_outer && geometry.outer_bounding_box.IsEmpty() &&
      !geometry.visible_bounding_box.IsEmpty()) {
    geometry.outer_bounding_box = geometry.visible_bounding_box;
  }

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

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                             HitTestRequest::kListBased |
                             HitTestRequest::kPenetratingList |
                             HitTestRequest::kAvoidCache,
                         nullptr);
  HitTestResult result(request, location);
  document.GetLayoutView()->HitTest(location, result);

  int32_t next_z_order = 1;
  // Use the list-based NodeSet directly to preserve insertion-order semantics
  // and duplicate filtering while avoiding intermediate storage.
  for (const auto& member_node : base::Reversed(result.ListBasedTestResult())) {
    Node* node = member_node.Get();
    CHECK(node);
    if (!node->GetLayoutObject()) {
      continue;
    }
    if (dom_node_to_z_order_.Contains(node)) {
      continue;
    }

    if (!node->IsDocumentNode() &&
        !document.ElementForHitTest(node,
                                    TreeScope::HitTestPointType::kInternal)) {
      continue;
    }
    dom_node_to_z_order_.insert(node, next_z_order++);
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
  //
  // TODO(crbug.com/415778689): Remove when consumers move to the frame data.
  if (Element* element = document.FocusedElement()) {
    page_interaction_info.focused_dom_node_id = AddInteractiveNode(*element);
  }

  LocalFrame* frame = document.GetFrame();
  CHECK(frame);

  // Accessibility focus
  //
  // TODO(crbug.com/415778689): Remove when consumers move to the frame data.
  if (DOMNodeId accessibility_focused_node_id =
          GetAccessibilityFocusedDOMNodeId(*frame);
      accessibility_focused_node_id != kInvalidDOMNodeId) {
    page_interaction_info.accessibility_focused_dom_node_id =
        accessibility_focused_node_id;
    AddInteractiveNode(accessibility_focused_node_id);
  }

  // Mouse location
  EventHandler& event_handler = frame->GetEventHandler();
  page_interaction_info.mouse_position =
      gfx::ToRoundedPoint(event_handler.LastKnownMousePositionInRootFrame());
}

void AIPageContentAgent::ContentBuilder::AddFrameData(
    LocalFrame& frame,
    mojom::blink::AIPageContentFrameData& frame_data) {
  frame_data.frame_interaction_info =
      mojom::blink::AIPageContentFrameInteractionInfo::New();
  frame_data.title = ReplaceUnpairedSurrogates(frame.GetDocument()->title());
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
    selection.selected_text = ReplaceUnpairedSurrogates(frame.SelectedText());

    const SelectionInDOMTree& frame_selection =
        frame.Selection().GetSelectionInDOMTree();
    const Position& start_position = frame_selection.ComputeStartPosition();
    const Position& end_position = frame_selection.ComputeEndPosition();
    Node* start_node = start_position.ComputeContainerNode();
    Node* end_node = end_position.ComputeContainerNode();

    if (start_node) {
      selection.start_dom_node_id = AddInteractiveNode(*start_node);

      selection.start_offset = start_position.ComputeOffsetInContainerNode();
    }

    if (end_node) {
      selection.end_dom_node_id = AddInteractiveNode(*end_node);

      selection.end_offset = end_position.ComputeOffsetInContainerNode();
    }
  }

  // Focused element
  if (Element* element = frame.GetDocument()->FocusedElement()) {
    frame_interaction_info.focused_dom_node_id = DOMNodeIds::IdForNode(element);
    AddInteractiveNode(*frame_interaction_info.focused_dom_node_id);
  }

  // Accessibility focus
  if (DOMNodeId accessibility_focused_node_id =
          GetAccessibilityFocusedDOMNodeId(frame);
      accessibility_focused_node_id != kInvalidDOMNodeId) {
    frame_interaction_info.accessibility_focused_dom_node_id =
        accessibility_focused_node_id;
    AddInteractiveNode(accessibility_focused_node_id);
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
  // Build the ContentNode tree. We don't set accessibility_focused_node_id for
  // popups because focus within transient popup windows is not tracked for
  // frame-level interactions.
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
  DCHECK_NE(mojom_popup->opener_dom_node_id, kInvalidDOMNodeId);
  // Always keep the popup opener id regardless of node-id policy so popup
  // metadata can round-trip and browser-side actions can toggle the popup.
  AddInteractiveNode(mojom_popup->opener_dom_node_id);

  frame_data.popup = std::move(mojom_popup);
}

void AIPageContentAgent::ContentBuilder::AddInteractionInfoForHitTesting(
    const Node* node,
    mojom::blink::AIPageContentNodeInteractionInfo& interaction_info) const {
  if (!actionable_mode()) {
    return;
  }

  auto it = dom_node_to_z_order_.find(node);
  if (it != dom_node_to_z_order_.end()) {
    interaction_info.document_scoped_z_order = it->value;
  }
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
