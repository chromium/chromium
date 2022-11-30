// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_STREAM_VALIDATOR_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_STREAM_VALIDATOR_H_

#include <string>

#include "content/common/input/gesture_event_stream_validator.h"
#include "content/common/input/touch_event_stream_validator.h"

namespace blink {
class WebInputEvent;
}

namespace content {

// DCHECKs that the stream of WebInputEvents passed to OnEvent is
// valid. Currently only validates touch and touchscreen gesture events.
class InputEventStreamValidator {
 public:
  InputEventStreamValidator();

  InputEventStreamValidator(const InputEventStreamValidator&) = delete;
  InputEventStreamValidator& operator=(const InputEventStreamValidator&) =
      delete;

  ~InputEventStreamValidator();

  void Validate(const blink::WebInputEvent&);

 private:
  bool ValidateImpl(const blink::WebInputEvent&,
                    std::string* error_msg);

  GestureEventStreamValidator gesture_validator_;
  TouchEventStreamValidator touch_validator_;
  std::string error_msg_;
  const bool enabled_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_STREAM_VALIDATOR_H_
