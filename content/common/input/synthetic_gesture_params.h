// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_

#include "content/common/content_export.h"
#include "content/common/input/input_injector.mojom-shared.h"

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

  content::mojom::GestureSourceType gesture_source_type;

  enum GestureType {
    SMOOTH_MOVE_GESTURE,
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

  static constexpr int kDefaultSpeedInPixelsPerSec = 800;

  virtual GestureType GetGestureType() const = 0;

  // Returns true if the specific gesture source type is supported on this
  // platform.
  static bool IsGestureSourceTypeSupported(
      content::mojom::GestureSourceType gesture_source_type);
  bool from_devtools_debugger = false;
  float vsync_offset_ms = 0.0f;
  content::mojom::InputEventPattern input_event_pattern =
      content::mojom::InputEventPattern::kDefaultPattern;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_GESTURE_PARAMS_H_
