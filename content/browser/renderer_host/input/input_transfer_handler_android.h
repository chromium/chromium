// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/input/transfer_input_to_viz_result.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/events/android/motion_event_android.h"

namespace content {

class InputTransferHandlerAndroidClient {
 public:
  virtual gpu::SurfaceHandle GetRootSurfaceHandle() = 0;
  virtual void SendStateOnTouchTransfer(const ui::MotionEvent& event) = 0;
};

// The class assumes transfer input to viz is supported, so instantiate only
// when |input::IsTransferInputToVizSupported()| returns true.
// The class is responsible for coordinating with java side InputTransferHandler
// which may end up issuing |WindowManager.transferTouchGesture| Android API
// call. In case of a successful transfer, rest of the touch sequence is
// consumed until a TOUCH_CANCEL is seen. InputTransferHandlerAndroid is owned
// by RenderWidgetHostViewAndroid, instantiated if
// |input::IsTransferInputToVizSupported()| returns true.
class CONTENT_EXPORT InputTransferHandlerAndroid {
 public:
  class JniDelegate {
   public:
    virtual ~JniDelegate() = default;
    // `raw_x` is the point's x coordinate in pixels in coordinate space of the
    // device display similar to MotionEvent.getRawX.
    virtual int MaybeTransferInputToViz(int surface_id, float raw_x) = 0;
  };

  explicit InputTransferHandlerAndroid(
      InputTransferHandlerAndroidClient* client);
  virtual ~InputTransferHandlerAndroid();

  // Virtual for testing.
  virtual bool OnTouchEvent(const ui::MotionEventAndroid& event);

  void set_jni_delegate_for_testing(std::unique_ptr<JniDelegate> delegate) {
    jni_delegate_ = std::move(delegate);
  }

  static constexpr const char* kTouchMovesSeenHistogram =
      "Android.InputOnViz.Browser.TouchMovesSeenAfterTransfer";
  static constexpr const char* kEventsAfterTransferHistogram =
      "Android.InputOnViz.Browser.EventsAfterTransfer";
  static constexpr const char* kTransferInputToVizResultHistogram =
      "Android.InputOnViz.Browser.TransferInputToVizResult";

  bool touch_transferred() { return touch_transferred_; }
  bool FilterRedundantDownEvent(const ui::MotionEvent& event);

  void RequestInputBack();

 private:
  void Reset();

  friend class MockInputTransferHandler;
  InputTransferHandlerAndroid();

  raw_ptr<InputTransferHandlerAndroidClient> client_ = nullptr;
  bool touch_transferred_ = false;
  // Stores the event time of first down event of the most recent touch sequence
  // transferred to VizCompositor. See
  // (https://developer.android.com/reference/android/view/MotionEvent#getDownTime())
  base::TimeTicks cached_transferred_sequence_down_time_ms_;
  int touch_moves_seen_after_transfer_ = 0;
  std::unique_ptr<JniDelegate> jni_delegate_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_TRANSFER_HANDLER_ANDROID_H_
