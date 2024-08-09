// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"

#include <cmath>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_get_root_node_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/display/screen_info.h"

namespace blink {

namespace {

int32_t BucketWithOffsetAndUnit(int32_t num, int32_t offset, int32_t unit) {
  DCHECK_LT(0, unit);
  // Bucketing raw number with `offset` centered.
  const int32_t grid = (num - offset) / unit;
  const int32_t bucketed =
      grid == 0  ? 0
      : grid > 0 ? std::pow(2, static_cast<int32_t>(std::log2(grid)))
                 : -std::pow(2, static_cast<int32_t>(std::log2(-grid)));
  return bucketed * unit + offset;
}

// Viewport initial scale x10 metrics is exponentially bucketed by offset of 10
// (most common initial-scale=1.0 is the center) to preserve user's privacy.
int32_t GetBucketedViewportInitialScale(int32_t initial_scale_x10) {
  DCHECK_LE(0, initial_scale_x10);
  return BucketWithOffsetAndUnit(initial_scale_x10, 10, 2);
}

// Viewport hardcoded width metrics is exponentially bucketed by offset of 500
// to preserve user's privacy.
int32_t GetBucketedViewportHardcodedWidth(int32_t hardcoded_width) {
  DCHECK_LE(0, hardcoded_width);
  return BucketWithOffsetAndUnit(hardcoded_width, 500, 10);
}

}  // namespace

static constexpr int kSmallFontThresholdInDips = 9;

// Values of maximum-scale smaller than this threshold will be considered to
// prevent the user from scaling the page as if user-scalable=no was set.
static constexpr double kMaximumScalePreventsZoomingThreshold = 1.2;

static constexpr base::TimeDelta kEvaluationInterval = base::Minutes(1);
static constexpr base::TimeDelta kEvaluationDelay = base::Seconds(5);

// Basically MF evaluation invoked every |kEvaluationInterval|, but its first
// evaluation invoked |kEvaluationDelay| after initialization of this module.
// Time offsetting with their difference simplifies these requirements.
static constexpr base::TimeDelta kFirstEvaluationOffsetTime =
    kEvaluationInterval - kEvaluationDelay;

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view),
      viewport_scalar_(
          frame_view_->GetFrame().GetWidgetForLocalRoot()
              ? frame_view_->GetPage()
                    ->GetChromeClient()
                    .WindowToViewportScalar(&frame_view_->GetFrame(), 1)
              : 1.0),
      last_evaluated_(base::TimeTicks::Now() - kFirstEvaluationOffsetTime) {}

MobileFriendlinessChecker::~MobileFriendlinessChecker() = default;

void MobileFriendlinessChecker::NotifyPaintBegin() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());

  ignore_beyond_viewport_scope_count_ =
      frame_view_->LayoutViewport()->MaximumScrollOffset().x() == 0 &&
      frame_view_->GetPage()
              ->GetVisualViewport()
              .MaximumScrollOffsetAtScale(initial_scale_)
              .x() == 0;
  is_painting_ = true;
  viewport_transform_ = &frame_view_->GetLayoutView()
                             ->FirstFragment()
                             .ContentsProperties()
                             .Transform();
  previous_transform_ = viewport_transform_;
  current_x_offset_ = 0.0;

  const ViewportDescription& viewport = frame_view_->GetFrame()
                                            .GetDocument()
                                            ->GetViewportData()
                                            .GetViewportDescription();
  if (viewport.type == ViewportDescription::Type::kViewportMeta) {
    const double zoom = viewport.zoom_is_explicit ? viewport.zoom : 1.0;
    viewport_device_width_ = viewport.max_width.IsDeviceWidth();
    if (viewport.max_width.IsFixed()) {
      // Convert value from Blink space to device-independent pixels.
      viewport_hardcoded_width_ =
          viewport.max_width.GetFloatValue() / viewport_scalar_;
    }

    if (viewport.zoom_is_explicit)
      viewport_initial_scale_x10_ = std::round(viewport.zoom * 10);

    if (viewport.user_zoom_is_explicit) {
      allow_user_zoom_ = viewport.user_zoom;
      // If zooming is only allowed slightly.
      if (viewport.max_zoom / zoom < kMaximumScalePreventsZoomingThreshold)
        allow_user_zoom_ = false;
    }
  }

  initial_scale_ = frame_view_->GetPage()
                       ->GetPageScaleConstraintsSet()
                       .FinalConstraints()
                       .initial_scale;
  int frame_width = frame_view_->GetPage()->GetVisualViewport().Size().width();
  viewport_width_ = frame_width * viewport_scalar_ / initial_scale_;
}

void MobileFriendlinessChecker::NotifyPaintEnd() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());
  ignore_beyond_viewport_scope_count_ = 0;
  is_painting_ = false;
}

MobileFriendlinessChecker* MobileFriendlinessChecker::Create(
    LocalFrameView& frame_view) {
  // Only run the mobile friendliness checker for the outermost main
  // frame. The checker will iterate through all local frames in the
  // current blink::Page. Also skip the mobile friendliness checks for
  // "non-ordinary" pages by checking IsLocalFrameClientImpl(), since
  // it's not useful to generate mobile friendliness metrics for
  // devtools, svg, etc.
  if (!frame_view.GetFrame().Client()->IsLocalFrameClientImpl() ||
      !frame_view.GetFrame().IsOutermostMainFrame() ||
      !frame_view.GetPage()->GetSettings().GetViewportEnabled() ||
      !frame_view.GetPage()->GetSettings().GetViewportMetaEnabled()) {
    return nullptr;
  }
  return MakeGarbageCollected<MobileFriendlinessChecker>(frame_view);
}

MobileFriendlinessChecker* MobileFriendlinessChecker::From(
    const Document& document) {
  DCHECK(document.GetFrame());

  auto* local_frame = DynamicTo<LocalFrame>(document.GetFrame()->Top());
  if (local_frame == nullptr)
    return nullptr;

  MobileFriendlinessChecker* mfc =
      local_frame->View()->GetMobileFriendlinessChecker();
  if (!mfc || !mfc->is_painting_)
    return nullptr;

  DCHECK_EQ(DocumentLifecycle::kInPaint, document.Lifecycle().GetState());
  DCHECK(!document.IsPrintingOrPaintingPreview());
  return mfc;
}

void MobileFriendlinessChecker::MaybeRecompute() {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());
  base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_evaluated_ < kEvaluationInterval)
    return;

  ComputeNow();
}

void MobileFriendlinessChecker::ComputeNow() {
  ukm::builders::MobileFriendliness builder(
      frame_view_->GetFrame().GetDocument()->UkmSourceID());

  builder.SetViewportDeviceWidth(viewport_device_width_);
  builder.SetAllowUserZoom(allow_user_zoom_);
  if (viewport_initial_scale_x10_) {
    builder.SetViewportInitialScaleX10(
        GetBucketedViewportInitialScale(*viewport_initial_scale_x10_));
  }
  if (viewport_hardcoded_width_) {
    builder.SetViewportHardcodedWidth(
        GetBucketedViewportHardcodedWidth(*viewport_hardcoded_width_));
  }
  builder.SetTextContentOutsideViewportPercentage(
      area_sizes_.TextContentsOutsideViewportPercentage(
          // Use SizeF when computing the area to avoid integer overflow.
          gfx::SizeF(frame_view_->GetPage()->GetVisualViewport().Size())
              .GetArea()));
  builder.SetSmallTextRatio(area_sizes_.SmallTextRatio());

  builder.Record(frame_view_->GetFrame().GetDocument()->UkmRecorder());
  last_evaluated_ = base::TimeTicks::Now();
}

int MobileFriendlinessChecker::AreaSizes::SmallTextRatio() const {
  if (total_text_area == 0)
    return 0;

  return small_font_area * 100 / total_text_area;
}

int MobileFriendlinessChecker::AreaSizes::TextContentsOutsideViewportPercentage(
    double viewport_area) const {
  return std::ceil(content_beyond_viewport_area * 100 / viewport_area);
}

void MobileFriendlinessChecker::UpdateTextAreaSizes(
    const PhysicalRect& text_rect,
    int font_size) {
  double actual_font_size = font_size * initial_scale_ / viewport_scalar_;
  double area = text_rect.Width() * text_rect.Height();
  if (std::round(actual_font_size) < kSmallFontThresholdInDips)
    area_sizes_.small_font_area += area;

  area_sizes_.total_text_area += area;
}

void MobileFriendlinessChecker::UpdateBeyondViewportAreaSizes(
    const PhysicalRect& paint_rect,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(is_painting_);
  if (ignore_beyond_viewport_scope_count_ != 0)
    return;

  if (previous_transform_ != &current_transform) {
    auto projection = GeometryMapper::SourceToDestinationProjection(
        current_transform, *viewport_transform_);
    if (projection.IsIdentityOr2dTranslation()) {
      current_x_offset_ = projection.To2dTranslation().x();
      previous_transform_ = &current_transform;
    } else {
      // For now we ignore offsets caused by non-2d-translation transforms.
      current_x_offset_ = 0;
    }
  }

  float right = paint_rect.Right() + current_x_offset_;
  float width = paint_rect.Width();
  float width_beyond_viewport =
      std::min(std::max(right - viewport_width_, 0.f), width);

  area_sizes_.content_beyond_viewport_area +=
      width_beyond_viewport * paint_rect.Height();
}

void MobileFriendlinessChecker::NotifyPaintTextFragment(
    const PhysicalRect& paint_rect,
    int font_size,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsOutermostMainFrame());

  UpdateTextAreaSizes(paint_rect, font_size);
  UpdateBeyondViewportAreaSizes(paint_rect, current_transform);
}

void MobileFriendlinessChecker::NotifyPaintReplaced(
    const PhysicalRect& paint_rect,
    const TransformPaintPropertyNodeOrAlias& current_transform) {
  DCHECK(frame_view_->GetFrame().Client()->IsLocalFrameClientImpl());
  DCHECK(frame_view_->GetFrame().IsLocalRoot());

  UpdateBeyondViewportAreaSizes(paint_rect, current_transform);
}

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  visitor->Trace(viewport_transform_);
  visitor->Trace(previous_transform_);
}

}  // namespace blink
