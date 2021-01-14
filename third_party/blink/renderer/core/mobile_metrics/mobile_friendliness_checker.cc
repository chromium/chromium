// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"

namespace blink {

const base::Feature kSmallTextRatio{"SmallTextRatio",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view),
      font_size_check_enabled_(base::FeatureList::IsEnabled(kSmallTextRatio) &&
                               frame_view_->GetFrame().GetWidgetForLocalRoot()),
      needs_report_mf_(false),
      viewport_scalar_(
          font_size_check_enabled_
              ? frame_view_->GetPage()
                    ->GetChromeClient()
                    .WindowToViewportScalar(&frame_view_->GetFrame(), 1)
              : 0) {}

MobileFriendlinessChecker::~MobileFriendlinessChecker() = default;

void MobileFriendlinessChecker::NotifyViewportUpdated(
    const ViewportDescription& viewport) {
  if (viewport.type != ViewportDescription::Type::kViewportMeta)
    return;

  mobile_friendliness_.viewport_device_width =
      viewport.max_width.IsDeviceWidth();
  if (viewport.max_width.IsFixed()) {
    mobile_friendliness_.viewport_hardcoded_width =
        viewport.max_width.GetFloatValue();
  }
  if (viewport.zoom_is_explicit)
    mobile_friendliness_.viewport_initial_scale = viewport.zoom;

  if (viewport.user_zoom_is_explicit)
    mobile_friendliness_.allow_user_zoom = viewport.user_zoom;
  frame_view_->DidChangeMobileFriendliness(mobile_friendliness_);
}

int MobileFriendlinessChecker::TextAreaWithFontSize::SmallTextRatio() const {
  if (total_text_area == 0)
    return 0;
  return small_font_area * 100 / total_text_area;
}

void MobileFriendlinessChecker::NotifyInvalidatePaint(
    const LayoutObject& object) {
  if (!font_size_check_enabled_)
    return;

  if (const auto* text = DynamicTo<LayoutText>(object)) {
    const ComputedStyle* style = text->Style();

    if (style->Visibility() != EVisibility::kVisible)
      return;

    double actual_font_size = style->FontSize();
    double initial_scale = frame_view_->GetPage()
                               ->GetPageScaleConstraintsSet()
                               .FinalConstraints()
                               .initial_scale;
    if (initial_scale > 0)
      actual_font_size *= initial_scale;
    actual_font_size /= viewport_scalar_;

    double area = text->PhysicalAreaSize();
    if (actual_font_size < MobileFriendlinessChecker::kSmallFontThreshold)
      text_area_sizes_.small_font_area += area;
    text_area_sizes_.total_text_area += area;

    const int previous_mfs = mobile_friendliness_.small_text_ratio * 100;
    mobile_friendliness_.small_text_ratio = text_area_sizes_.SmallTextRatio();
    const int current_mfs = mobile_friendliness_.small_text_ratio * 100;
    needs_report_mf_ = needs_report_mf_ || (previous_mfs != current_mfs);
  }
}

void MobileFriendlinessChecker::NotifyPrePaintFinished() {
  if (!font_size_check_enabled_ || !needs_report_mf_)
    return;
  DCHECK_EQ(frame_view_->GetFrame().GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  frame_view_->DidChangeMobileFriendliness(mobile_friendliness_);
  needs_report_mf_ = false;
}

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

}  // namespace blink
