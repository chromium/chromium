// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_TEXT_SELECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_TEXT_SELECTOR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/events/gesture_detection/gesture_listeners.h"

namespace ui {
class GestureDetector;
class MotionEvent;
}

namespace content {
class StylusTextSelectorTest;

// Interface with which the StylusTextSelector conveys drag and tap gestures
// when the activating button is pressed.
// selection handles, or long press.
class CONTENT_EXPORT StylusTextSelectorClient {
 public:
  virtual ~StylusTextSelectorClient() {}

  // (x0, y0) and (x1, y1) indicate the bounds of the initial selection.
  virtual void OnStylusSelectBegin(float x0, float y0, float x1, float y1) = 0;
  virtual void OnStylusSelectUpdate(float x, float y) = 0;
  virtual void OnStylusSelectEnd(float x, float y) = 0;
  virtual void OnStylusSelectTap(base::TimeTicks time, float x, float y) = 0;
};

// Provides stylus-based text selection and interaction, including:
//   * Selection manipulation when an activating stylus button is pressed and
//     the stylus is dragged.
//   * Word selection and context menu activation when the when an activating
//     stylus button is pressed and the stylus is tapped.
class CONTENT_EXPORT StylusTextSelector : public ui::SimpleGestureListener {
 public:
  explicit StylusTextSelector(StylusTextSelectorClient* client);

  StylusTextSelector(const StylusTextSelector&) = delete;
  StylusTextSelector& operator=(const StylusTextSelector&) = delete;

  ~StylusTextSelector() override;

  // This should be called before |event| is seen by the platform gesture
  // detector or forwarded to web content.
  bool OnTouchEvent(const ui::MotionEvent& event);

 private:
  enum DragState {
    NO_DRAG,
    DRAGGING_WITH_BUTTON_PRESSED,
    DRAGGING_WITH_BUTTON_RELEASED,
  };
  friend class StylusTextSelectorTest;
  FRIEND_TEST_ALL_PREFIXES(StylusTextSelectorTest, ShouldStartTextSelection);

  // SimpleGestureListener implementation.
  bool OnSingleTapUp(const ui::MotionEvent& e, int tap_count) override;
  bool OnScroll(const ui::MotionEvent& e1,
                const ui::MotionEvent& e2,
                const ui::MotionEvent& secondary_pointer_down,
                float distance_x,
                float distance_y) override;

  static bool ShouldStartTextSelection(const ui::MotionEvent& event);

  raw_ptr<StylusTextSelectorClient> client_;
  bool text_selection_triggered_;
  bool secondary_button_pressed_;
  DragState drag_state_;
  float anchor_x_;
  float anchor_y_;
  std::unique_ptr<ui::GestureDetector> gesture_detector_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_TEXT_SELECTOR_H_
