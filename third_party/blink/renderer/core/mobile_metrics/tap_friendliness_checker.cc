// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mobile_metrics/tap_friendliness_checker.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
namespace {
// Considers the |target| is a tap-able element which TapFriendlinessChecker
// focus.
bool ShouldRegister(Element* target) {
  // TODO(crbug.com/369219144): Should this be DynamicTo<HTMLAnchorElementBase>?
  if (const auto* anchor = DynamicTo<HTMLAnchorElement>(target)) {
    return !anchor->Href().IsEmpty();
  } else if (auto* element = DynamicTo<HTMLElement>(target);
             element && element->WillRespondToMouseClickEvents()) {
    return true;
  } else {
    return IsA<HTMLFormControlElement>(target);
  }
}

int ViewportWidthInDIPS(LocalFrameView& view) {
  int width = view.ViewportWidth();
  return view.FrameToScreen(gfx::Rect(0, 0, width, 0)).width();
}
}  // namespace

TapFriendlinessChecker* TapFriendlinessChecker::CreateIfMobile(
    LocalFrameView& view) {
  if (!view.GetPage()->GetSettings().GetViewportEnabled() ||
      !view.GetPage()->GetSettings().GetViewportMetaEnabled()) {
    return nullptr;
  }
  return MakeGarbageCollected<TapFriendlinessChecker>(view, PassKey());
}

void TapFriendlinessChecker::RegisterTapEvent(Element* target) {
  if (!ShouldRegister(target))
    return;
  auto* node = DynamicTo<HTMLElement>(target);
  if (!node)
    return;

  // Here we use definition of Android for DIPS.
  // See: https://en.wikipedia.org/wiki/Device-independent_pixel
  constexpr float kOneDipInMm = 0.15875;
  constexpr float kTooSmallThresholdInMm = 7.0;
  constexpr float kTooCloseDisplayEdgeThresholdInMm = 5.0;
  constexpr float kZoomThreshold = 1.2;
  constexpr float kHighlyZoomThreshold = 2.0;
  const float zoom_factor = view_->GetPage()->GetVisualViewport().Scale() /
                            view_->GetPage()
                                ->GetPageScaleConstraintsSet()
                                .FinalConstraints()
                                .initial_scale;
  const gfx::Rect dip_rect = view_->FrameToScreen(
      gfx::ToEnclosingRect(node->GetBoundingClientRectNoLifecycleUpdate()));

  ukm::builders::MobileFriendliness_TappedBadTargets builder(
      view_->GetFrame().GetDocument()->UkmSourceID());

  if (dip_rect.width() * kOneDipInMm / zoom_factor <= kTooSmallThresholdInMm &&
      dip_rect.height() * kOneDipInMm / zoom_factor <= kTooSmallThresholdInMm) {
    builder.SetTooSmall(true);
  }
  const float display_width =
      ViewportWidthInDIPS(*view_) * kOneDipInMm / zoom_factor;
  const double center_x =
      (dip_rect.x() + dip_rect.width()) * kOneDipInMm / zoom_factor / 2.0;
  const double center_y =
      (dip_rect.y() + dip_rect.height()) * kOneDipInMm / zoom_factor / 2.0;
  if (center_x <= kTooCloseDisplayEdgeThresholdInMm ||
      display_width - kTooCloseDisplayEdgeThresholdInMm <= center_x ||
      center_y <= kTooCloseDisplayEdgeThresholdInMm) {
    builder.SetCloseDisplayEdge(true);
  }
  if (kZoomThreshold <= zoom_factor) {
    builder.SetZoomed(true);
    if (kHighlyZoomThreshold <= zoom_factor)
      builder.SetHighlyZoomed(true);
  }

  builder.Record(view_->GetFrame().GetDocument()->UkmRecorder());
}

void TapFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(view_);
}

}  // namespace blink
