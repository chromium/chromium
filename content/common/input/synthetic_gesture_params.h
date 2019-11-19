// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

// Base class for storing parameters of synthetic gestures.
//
// The subclasses of this class only store data on synthetic gestures.
// The logic for dispatching input events that implement the gesture lives
// in separate classes in content/browser/renderer_host/input/.
//
struct CONTENT_EXPORT SyntheticGestureParams {
  SyntheticGestureParams();
  SyntheticGestureParams(const SyntheticGestureParams& other);
  virtual ~SyntheticGestureParams();

  // Describes which type of input events synthetic gesture objects should
  // generate. When specifying DEFAULT_INPUT the platform will be queried for
  // the preferred input event type.
  enum GestureSourceType {
    DEFAULT_INPUT,
    TOUCH_INPUT,
    MOUSE_INPUT,
    TOUCHPAD_INPUT = MOUSE_INPUT,
    PEN_INPUT,
    GESTURE_SOURCE_TYPE_MAX = PEN_INPUT
  };
  GestureSourceType gesture_source_type;

  enum GestureType {
    SMOOTH_SCROLL_GESTURE,
    SMOOTH_DRAG_GESTURE,
    PINCH_GESTURE,
    TAP_GESTURE,
    POINTER_ACTION_LIST,

    // Used to synchronize with the renderer to a known state. See
    // WaitForInputProcessed in input_handler.mojom.
    WAIT_FOR_INPUT_PROCESSED,
    SYNTHETIC_GESTURE_TYPE_MAX = POINTER_ACTION_LIST
  };

  virtual GestureType GetGestureType() const = 0;

  // Returns true if the specific gesture source type is supported on this
  // platform.
  static bool IsGestureSourceTypeSupported(
      GestureSourceType gesture_source_type);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
