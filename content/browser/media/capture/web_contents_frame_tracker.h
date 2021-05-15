// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContentsVideoCaptureDevice;
class MouseCursorOverlayController;
class RenderFrameHost;

// Monitors the WebContents instance and notifies the parent
// WebContentsVideoCaptureDevice |device| class any time the frame sink or
// main render frame's view changes.
class CONTENT_EXPORT WebContentsFrameTracker
    : public WebContentsObserver,
      public base::SupportsWeakPtr<WebContentsFrameTracker> {
 public:
  // We generally retrieve certain properties by accessing fields on the
  // WebContents object, however these properties may come from a different
  // context in some circumstances, such as testing.
  class Context {
   public:
    virtual ~Context() = default;

    // Get bounds of the attached screen, if any.
    virtual absl::optional<gfx::Rect> GetScreenBounds() = 0;

    // While the DOM always has a FrameSinkId, we may want to capture
    // a different frame sink ID overlaying the DOM content that represents
    // what we actually want to capture.
    virtual viz::FrameSinkId GetFrameSinkIdForCapture() = 0;

    // Capturer count handling is tricky in testing, since setting it
    // on the web contents uses a view even though the view may not be
    // initialized in the test harness.
    virtual void IncrementCapturerCount(const gfx::Size& capture_size) = 0;
    virtual void DecrementCapturerCount() = 0;
  };

  // NOTE on lifetime: |device| should outlive the WebContentsFrameTracker. The
  // |device| will be exclusively accessed on the sequence that is used to
  // construct |this| (which must not be the UI thread).
  WebContentsFrameTracker(base::WeakPtr<WebContentsVideoCaptureDevice> device,
                          MouseCursorOverlayController* cursor_controller);

  WebContentsFrameTracker(WebContentsFrameTracker&&) = delete;
  WebContentsFrameTracker(const WebContentsFrameTracker&) = delete;
  WebContentsFrameTracker& operator=(const WebContentsFrameTracker&&) = delete;
  WebContentsFrameTracker& operator=(const WebContentsFrameTracker&) = delete;

  ~WebContentsFrameTracker() final;

  void WillStartCapturingWebContents(const gfx::Size& capture_size);
  void DidStopCapturingWebContents();

  // The preferred size calculated here is a strong suggestion to UI
  // layout code to size the viewport such that physical rendering matches the
  // exact capture size. This helps to eliminate redundant scaling operations
  // during capture. Note that if there are multiple capturers, a "first past
  // the post" system is used and the first capturer's preferred size is set.
  gfx::Size CalculatePreferredSize(const gfx::Size& capture_size);

  // WebContentsObserver overrides.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) final;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) final;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) final;
  void WebContentsDestroyed() final;
  void CaptureTargetChanged() final;

  void SetWebContentsAndContextFromRoutingId(const GlobalFrameRoutingId& id);

  // WebContents are retrieved on the UI thread normally, from the render IDs,
  // so this method is provided for tests to set the web contents directly.
  void SetWebContentsAndContextForTesting(WebContents* web_contents,
                                          std::unique_ptr<Context> context);

 private:
  // Re-evaluates whether a new frame sink should be targeted for capture and
  // notifies the device. If the WebContents instance is no longer being
  // observed, the device is notified that the capture target has been
  // permanently lost.
  void OnPossibleTargetChange();

  // Sets the target view for the cursor controller on non-Android platforms.
  // Noop on Android.
  void SetTargetView(gfx::NativeView view);

  // |device_| may be dereferenced only by tasks run by |device_task_runner_|.
  const base::WeakPtr<WebContentsVideoCaptureDevice> device_;

  // The task runner to be used for device callbacks.
  const scoped_refptr<base::SequencedTaskRunner> device_task_runner_;

  // Owned by FrameSinkVideoCaptureDevice. This will be valid for the life of
  // WebContentsFrameTracker because the WebContentsFrameTracker deleter task
  // will be posted to the UI thread before the MouseCursorOverlayController
  // deleter task.
#if !defined(OS_ANDROID)
  MouseCursorOverlayController* cursor_controller_ = nullptr;
#endif

  // We may not have a frame sink ID target at all times.
  std::unique_ptr<Context> context_;
  absl::optional<viz::FrameSinkId> target_frame_sink_id_;
  gfx::NativeView target_native_view_ = gfx::NativeView();

  // Indicates whether the WebContents's capturer count needs to be
  // decremented.
  bool is_capturing_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_FRAME_TRACKER_H_
