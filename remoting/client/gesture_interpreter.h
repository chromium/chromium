// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_GESTURE_INTERPRETER_H_
#define REMOTING_CLIENT_GESTURE_INTERPRETER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/input/touch_input_strategy.h"
#include "remoting/client/ui/desktop_viewport.h"
#include "remoting/client/ui/fling_animation.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class ChromotingSession;
class RendererProxy;

// This is a class for interpreting a raw touch input into actions like moving
// the viewport and injecting mouse clicks.
class GestureInterpreter {
 public:
  enum GestureState { GESTURE_BEGAN, GESTURE_CHANGED, GESTURE_ENDED };

  enum InputMode {
    UNDEFINED_INPUT_MODE,
    DIRECT_INPUT_MODE,
    TRACKPAD_INPUT_MODE
  };

  GestureInterpreter();
  ~GestureInterpreter();

  // Sets the context for the interpreter. Both arguments are nullable. If both
  // are nullptr then methods below will have no effect.
  void SetContext(RendererProxy* renderer, ChromotingSession* input_stub);

  // Must be called right after the renderer is ready.
  void SetInputMode(InputMode mode);

  // Returns the current input mode.
  InputMode GetInputMode() const;

  // Coordinates of the OpenGL view surface will be used.

  // Called during a two-finger pinching gesture. This can happen in conjunction
  // with Pan().
  void Zoom(float pivot_x, float pivot_y, float scale, GestureState state);

  // Called whenever the user did a pan gesture. It can be one-finger pan, no
  // matter dragging in on or not, or two-finger pan in conjunction with zoom.
  // Two-finger pan without zoom is consider a scroll gesture.
  void Pan(float translation_x, float translation_y);

  // Called when the user did a one-finger tap.
  void Tap(float x, float y);

  void TwoFingerTap(float x, float y);

  void ThreeFingerTap(float x, float y);

  // Caller is expected to call both Pan() and Drag() when dragging is in
  // progress.
  void Drag(float x, float y, GestureState state);

  // Called when the user has just done a one-finger pan (no dragging or
  // zooming) and the pan gesture still has some final velocity.
  void OneFingerFling(float velocity_x, float velocity_y);

  // Called during a two-finger scroll (panning without zooming) gesture.
  void Scroll(float x, float y, float dx, float dy);

  // Called when the user has just done a scroll gesture and the scroll gesture
  // still has some final velocity.
  void ScrollWithVelocity(float velocity_x, float velocity_y);

  // Called to process one animation frame.
  void ProcessAnimations();

  void OnSurfaceSizeChanged(int width, int height);
  void OnDesktopSizeChanged(int width, int height);
  void OnSafeInsetsChanged(int left, int top, int right, int bottom);

  base::WeakPtr<GestureInterpreter> GetWeakPtr();

 private:
  void PanWithoutAbortAnimations(float translation_x, float translation_y);

  void ScrollWithoutAbortAnimations(float dx, float dy);

  void AbortAnimations();

  // Injects the mouse click event and shows the touch feedback.
  void InjectMouseClick(float touch_x,
                        float touch_y,
                        protocol::MouseEvent_MouseButton button);

  void InjectCursorPosition(float x, float y);

  void SetGestureInProgress(TouchInputStrategy::Gesture gesture,
                            bool is_in_progress);

  // Starts the given feedback at (cursor_x, cursor_y) if the feedback radius
  // is non-zero.
  void StartInputFeedback(float cursor_x,
                          float cursor_y,
                          TouchInputStrategy::TouchFeedbackType feedback_type);

  InputMode input_mode_ = UNDEFINED_INPUT_MODE;
  std::unique_ptr<TouchInputStrategy> input_strategy_;
  DesktopViewport viewport_;
  RendererProxy* renderer_ = nullptr;
  ChromotingSession* input_stub_ = nullptr;
  TouchInputStrategy::Gesture gesture_in_progress_;

  FlingAnimation pan_animation_;
  FlingAnimation scroll_animation_;

  base::WeakPtrFactory<GestureInterpreter> weak_factory_{this};

  // GestureInterpreter is neither copyable nor movable.
  DISALLOW_COPY_AND_ASSIGN(GestureInterpreter);
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_GESTURE_INTERPRETER_H_
