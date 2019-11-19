// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_
#define CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

// Utility class for validating a stream of WebGestureEvents.
class CONTENT_EXPORT GestureEventStreamValidator {
 public:
  GestureEventStreamValidator();
  ~GestureEventStreamValidator();

  // If |event| is valid for the current stream, returns true.
  // Otherwise, returns false with a corresponding error message.
  bool Validate(const blink::WebGestureEvent& event, std::string* error_msg);

 private:
  bool scrolling_;
  bool pinching_;
  bool waiting_for_tap_end_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventStreamValidator);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR_H_
