// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {
// Note that this is a factor to be multiplied by the frame dimensions, in
// contrast to s_legacy_max_child_frame_screen_rect_movement, which is in DIPS.
std::optional<double> max_child_frame_screen_rect_movement;
std::optional<int> min_screen_rect_stable_time_ms;

// These are the values that were in use prior to adding the feature flag
// kDiscardInputEventsToRecentlyMovedFrames; they applied only to cross-origin
// iframes that uses IntersectionObserver V2 features (i.e. occlusion tracking).
const int s_legacy_max_child_frame_screen_rect_movement = 30;
const int s_legacy_min_screen_rect_stable_time_ms = 500;
}  // namespace

FrameVisualProperties::FrameVisualProperties() = default;

FrameVisualProperties::FrameVisualProperties(
    const FrameVisualProperties& other) = default;

FrameVisualProperties::~FrameVisualProperties() = default;

FrameVisualProperties& FrameVisualProperties::operator=(
    const FrameVisualProperties& other) = default;

double FrameVisualProperties::MaxChildFrameScreenRectMovement() {
  if (!max_child_frame_screen_rect_movement.has_value()) {
    max_child_frame_screen_rect_movement.emplace(
        base::GetFieldTrialParamByFeatureAsDouble(
            features::kDiscardInputEventsToRecentlyMovedFrames,
            "distance_factor", std::numeric_limits<double>::max()));
  }
  return max_child_frame_screen_rect_movement.value();
}

int FrameVisualProperties::MinScreenRectStableTimeMs() {
  if (!min_screen_rect_stable_time_ms.has_value()) {
    min_screen_rect_stable_time_ms.emplace(
        base::GetFieldTrialParamByFeatureAsInt(
            features::kDiscardInputEventsToRecentlyMovedFrames, "time_ms", 0));
  }
  return min_screen_rect_stable_time_ms.value();
}

int FrameVisualProperties::MaxChildFrameScreenRectMovementForIOv2() {
  return s_legacy_max_child_frame_screen_rect_movement;
}

int FrameVisualProperties::MinScreenRectStableTimeMsForIOv2() {
  return s_legacy_min_screen_rect_stable_time_ms;
}
}  // namespace blink
