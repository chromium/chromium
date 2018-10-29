// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/scheduled_navigation.h"

#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ScheduledNavigation::ScheduledNavigation(Reason reason,
                                         double delay,
                                         Document* origin_document,
                                         bool is_location_change,
                                         base::TimeTicks input_timestamp)
    : reason_(reason),
      delay_(delay),
      origin_document_(origin_document),
      is_location_change_(is_location_change),
      input_timestamp_(input_timestamp) {
  if (LocalFrame::HasTransientUserActivation(
          origin_document ? origin_document->GetFrame() : nullptr))
    user_gesture_token_ = UserGestureIndicator::CurrentToken();
}

ScheduledNavigation::~ScheduledNavigation() = default;

std::unique_ptr<UserGestureIndicator>
ScheduledNavigation::CreateUserGestureIndicator() {
  return std::make_unique<UserGestureIndicator>(user_gesture_token_);
}

}  // namespace blink
