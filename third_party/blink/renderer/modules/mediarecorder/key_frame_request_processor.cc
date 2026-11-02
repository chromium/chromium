// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"

#include <variant>

#include "base/logging.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"

namespace blink {
KeyFrameRequestProcessor::KeyFrameRequestProcessor(
    Configuration config,
    bool cross_origin_isolated_capability)
    : config_(config),
      cross_origin_isolated_capability_(cross_origin_isolated_capability) {}

base::TimeTicks KeyFrameRequestProcessor::CoarsenTime(
    base::TimeTicks time) const {
  // Shared across instances so that parallel recorders can't average out the
  // jitter.
  static const TimeClamper time_clamper;
  return base::TimeTicks() +
         time_clamper.ClampTimeResolution(time.since_origin(),
                                          cross_origin_isolated_capability_);
}

void KeyFrameRequestProcessor::OnKeyFrame(base::TimeTicks now) {
  last_key_frame_received_.frame_counter = frame_counter_;
  last_key_frame_received_.timestamp = CoarsenTime(now);
  consider_key_frame_request_ = true;
}

bool KeyFrameRequestProcessor::OnFrameAndShouldRequestKeyFrame(
    base::TimeTicks now) {
  frame_counter_++;
  bool request_keyframe = std::visit(
      absl::Overload{
          [&](uint64_t count) {
            return frame_counter_ >
                   last_key_frame_received_.frame_counter + count;
          },
          [&](base::TimeDelta duration) {
            return CoarsenTime(now) >=
                   last_key_frame_received_.timestamp + duration;
          },
          [&](auto&) {
            constexpr size_t kDefaultKeyIntervalCount = 100;
            return frame_counter_ > last_key_frame_received_.frame_counter +
                                        kDefaultKeyIntervalCount;
          }},
      config_);
  if (request_keyframe && consider_key_frame_request_) {
    consider_key_frame_request_ = false;
    return true;
  }
  return false;
}
}  // namespace blink
