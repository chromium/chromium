// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/key_frame_request_processor.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"

namespace blink {
KeyFrameRequestProcessor::KeyFrameRequestProcessor(Configuration config)
    : config_(config) {}

void KeyFrameRequestProcessor::OnKeyFrame(base::TimeTicks now) {
  last_key_frame_received_.frame_counter = frame_counter_;
  last_key_frame_received_.timestamp = now;
  consider_key_frame_request_ = true;
}

bool KeyFrameRequestProcessor::OnFrameAndShouldRequestKeyFrame(
    base::TimeTicks now) {
  frame_counter_++;
  bool request_keyframe = absl::visit(
      base::Overloaded{[&](uint64_t count) {
                         return frame_counter_ >
                                last_key_frame_received_.frame_counter + count;
                       },
                       [&](base::TimeDelta duration) {
                         return now >=
                                last_key_frame_received_.timestamp + duration;
                       },
                       [&](auto&) {
                         constexpr size_t kDefaultKeyIntervalCount = 100;
                         return frame_counter_ >
                                last_key_frame_received_.frame_counter +
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
