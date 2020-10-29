// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"

namespace blink {

MobileFriendlinessChecker::MobileFriendlinessChecker(LocalFrameView& frame_view)
    : frame_view_(&frame_view) {}

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

void MobileFriendlinessChecker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

}  // namespace blink
