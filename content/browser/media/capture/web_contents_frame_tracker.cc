// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/capture/video_capture_types.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_ANDROID)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

namespace content {

namespace {

// Note on lifetime: this context should be deleted when the web contents
// is destroyed.
class WebContentsContext : public WebContentsFrameTracker::Context {
 public:
  explicit WebContentsContext(WebContents* contents) : contents_(contents) {}
  ~WebContentsContext() override = default;

  // WebContextFrameTracker::Context overrides.
  base::Optional<gfx::Rect> GetScreenBounds() override {
    if (auto* view = GetCurrentView()) {
      // If we know the available size of the screen, we don't want to exceed
      // it as it may result in strange capture behavior in some cases.
      blink::ScreenInfo info;
      view->GetScreenInfo(&info);
      return info.rect;
    }
    return base::nullopt;
  }

  viz::FrameSinkId GetFrameSinkIdForCapture() override {
    if (auto* view = GetCurrentView()) {
      return view->GetFrameSinkId();
    }
    return {};
  }

  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    capture_handle_ =
        contents_->IncrementCapturerCount(capture_size, /*stay_hidden=*/false,
                                          /*stay_awake=*/true);
  }

  void DecrementCapturerCount() override { capture_handle_.RunAndReset(); }

 private:
  RenderWidgetHostViewBase* GetCurrentView() const {
    RenderWidgetHostView* view = contents_->GetRenderWidgetHostView();
    // Make sure the RWHV is still associated with a RWH before considering the
    // view "alive." This is because a null RWH indicates the RWHV has had its
    // Destroy() method called.
    if (!view || !view->GetRenderWidgetHost()) {
      return nullptr;
    }
    // Inside content, down-casting from the public interface class is safe.
    return static_cast<RenderWidgetHostViewBase*>(view);
  }

  base::ScopedClosureRunner capture_handle_;

  // The backing web contents.
  WebContents* contents_;
};

}  // namespace

WebContentsFrameTracker::WebContentsFrameTracker(
    base::WeakPtr<WebContentsVideoCaptureDevice> device,
    MouseCursorOverlayController* cursor_controller)
    : device_(std::move(device)),
      device_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(device_task_runner_);

#if !defined(OS_ANDROID)
  cursor_controller_ = cursor_controller;
  DCHECK(cursor_controller_);
#endif
}

WebContentsFrameTracker::~WebContentsFrameTracker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_capturing_) {
    DidStopCapturingWebContents();
  }
}

void WebContentsFrameTracker::WillStartCapturingWebContents(
    const gfx::Size& capture_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_capturing_);
  if (!web_contents()) {
    return;
  }

  const gfx::Size preferred_size = CalculatePreferredSize(capture_size);
  context_->IncrementCapturerCount(preferred_size);
  is_capturing_ = true;
}

void WebContentsFrameTracker::DidStopCapturingWebContents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (web_contents()) {
    DCHECK(is_capturing_);
    context_->DecrementCapturerCount();
    is_capturing_ = false;
  }
  DCHECK(!is_capturing_);
}

// We provide the WebContents with a preferred size override during its capture.
// The preferred size is a strong suggestion to UI layout code to size the view
// such that its physical rendering size matches the exact capture size. This
// helps to eliminate redundant scaling operations during capture. Note that if
// there are multiple capturers, a "first past the post" system is used and
// the first capturer's preferred size is set.
gfx::Size WebContentsFrameTracker::CalculatePreferredSize(
    const gfx::Size& capture_size) {
  gfx::Size preferred_size = capture_size;

  // If we know the available size of the screen, we don't want to exceed
  // it as it may result in strange capture behavior in some cases.
  if (context_) {
    const base::Optional<gfx::Rect> screen_bounds = context_->GetScreenBounds();
    if (screen_bounds) {
      preferred_size.SetToMin(screen_bounds->size());
    }
  }
  return preferred_size;
}

void WebContentsFrameTracker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::WebContentsDestroyed() {
  is_capturing_ = false;
  Observe(nullptr);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId(
    const GlobalFrameRoutingId& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Observe(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(id)));
  context_ = std::make_unique<WebContentsContext>(web_contents());
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::SetWebContentsAndContextForTesting(
    WebContents* web_contents,
    std::unique_ptr<WebContentsFrameTracker::Context> context) {
  Observe(web_contents);
  context_ = std::move(context);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::OnPossibleTargetChange() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents()) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    SetTargetView({});
    return;
  }

  viz::FrameSinkId frame_sink_id;
  if (context_) {
    frame_sink_id = context_->GetFrameSinkIdForCapture();
  }
  if (frame_sink_id != target_frame_sink_id_) {
    target_frame_sink_id_ = frame_sink_id;
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetChanged, device_,
                       frame_sink_id));
  }

  SetTargetView(web_contents()->GetNativeView());
}

// Note: MouseCursorOverlayController runs on the UI thread. It's also
// important that SetTargetView() be called in the current stack while
// |view| is known to be a valid pointer. http://crbug.com/818679
void WebContentsFrameTracker::SetTargetView(gfx::NativeView view) {
  if (view == target_native_view_)
    return;
  target_native_view_ = view;
#if !defined(OS_ANDROID)
  cursor_controller_->SetTargetView(view);
#endif
}

}  // namespace content
