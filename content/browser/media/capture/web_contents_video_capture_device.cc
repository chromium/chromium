// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
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

// Threading note: This is constructed on the device thread, while the
// destructor and the rest of the class will run exclusively on the UI thread.
class WebContentsVideoCaptureDevice::FrameTracker final
    : public WebContentsObserver,
      public base::SupportsWeakPtr<
          WebContentsVideoCaptureDevice::FrameTracker> {
 public:
  FrameTracker(base::WeakPtr<WebContentsVideoCaptureDevice> device,
               MouseCursorOverlayController* cursor_controller,
               int render_process_id,
               int main_render_frame_id)
      : device_(std::move(device)),
        device_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        cursor_controller_(cursor_controller) {
    DCHECK(device_task_runner_);
#if !defined(OS_ANDROID)
    DCHECK(cursor_controller_);
#else
    // On Android |cursor_controller_| must be used or get an unused private
    // variable compiler error.
    (void)cursor_controller_;
#endif

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<FrameTracker> self, int process_id, int frame_id) {
              if (self) {
                self->Observe(WebContents::FromRenderFrameHost(
                    RenderFrameHost::FromID(process_id, frame_id)));
                self->OnPossibleTargetChange();
              }
            },
            AsWeakPtr(), render_process_id, main_render_frame_id));
  }

  ~FrameTracker() final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (is_capturing_)
      DidStopCapturingWebContents();
  }

  void WillStartCapturingWebContents(const gfx::Size& capture_size) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!is_capturing_);

    auto* contents = web_contents();
    if (!contents) {
      return;
    }

    // Increment the WebContents's capturer count, providing WebContents with a
    // preferred size override during its capture. The preferred size is a
    // strong suggestion to UI layout code to size the view such that its
    // physical rendering size matches the exact capture size. This helps to
    // eliminate redundant scaling operations during capture. Note that if
    // there are multiple capturers, a "first past the post" system is used and
    // the first capturer's preferred size is set.
    //
    // The preferred size is the same as the capture size, as it
    // generally factors in the device scale factor of the view implicitly.
    gfx::Size preferred_size = capture_size;
    if (auto* view = GetCurrentView()) {
      // If we know the available size of the screen, we don't want to exceed
      // it as it may result in strange capture behavior in some cases.
      blink::ScreenInfo info;
      view->GetScreenInfo(&info);

      // The |rect| on ScreenInfo is the size of the display.
      preferred_size.SetToMin(info.rect.size());
    }
    contents->IncrementCapturerCount(preferred_size, /* stay_hidden */ false);
    is_capturing_ = true;
  }

  void DidStopCapturingWebContents() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* contents = web_contents()) {
      DCHECK(is_capturing_);
      contents->DecrementCapturerCount(/* stay_hidden */ false);
      is_capturing_ = false;
    }
    DCHECK(!is_capturing_);
  }

 private:
  // Find the view associated with the entirety of displayed content of the
  // current WebContents, whether that be a fullscreen view or the regular view.
  RenderWidgetHostView* GetCurrentView() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    WebContents* const contents = web_contents();
    if (!contents) {
      return nullptr;
    }

    RenderWidgetHostView* view = view = contents->GetRenderWidgetHostView();
    // Make sure the RWHV is still associated with a RWH before considering the
    // view "alive." This is because a null RWH indicates the RWHV has had its
    // Destroy() method called.
    if (!view || !view->GetRenderWidgetHost()) {
      return nullptr;
    }
    return view;
  }

  // WebContentsObserver overrides.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) final {
    OnPossibleTargetChange();
  }
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) final {
    OnPossibleTargetChange();
  }
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) final {
    OnPossibleTargetChange();
  }
  void WebContentsDestroyed() final {
    Observe(nullptr);
    is_capturing_ = false;
    OnPossibleTargetChange();
  }

  // Re-evaluates whether a new frame sink or native view should be targeted for
  // capture and notifies the device. If the WebContents instance is no longer
  // being observed, the device is notified that the capture target has been
  // permanently lost.
  void OnPossibleTargetChange() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (web_contents()) {
      viz::FrameSinkId frame_sink_id;
      gfx::NativeView native_view = gfx::NativeView();
      if (auto* const view = GetCurrentView()) {
        // Inside content, down-casting from the public interface class is
        // safe.
        auto* const view_impl = static_cast<RenderWidgetHostViewBase*>(view);
        frame_sink_id = view_impl->GetFrameSinkId();
        native_view = view_impl->GetNativeView();
      }

      if (frame_sink_id != target_frame_sink_id_) {
        target_frame_sink_id_ = frame_sink_id;
        device_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetChanged,
                           device_, frame_sink_id));
      }

      if (native_view != target_native_view_) {
        target_native_view_ = native_view;
        // Note: MouseCursorOverlayController runs on the UI thread. It's also
        // important that SetTargetView() be called in the current stack while
        // |native_view| is known to be a valid pointer. http://crbug.com/818679
#if !defined(OS_ANDROID)
        cursor_controller_->SetTargetView(native_view);
#endif
      }
    } else {
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &WebContentsVideoCaptureDevice::OnTargetPermanentlyLost,
              device_));
#if !defined(OS_ANDROID)
      cursor_controller_->SetTargetView(gfx::NativeView());
#endif
    }
  }

  // |device_| may be dereferenced only by tasks run by |device_task_runner_|.
  const base::WeakPtr<WebContentsVideoCaptureDevice> device_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Owned by FrameSinkVideoCaptureDevice. This will be valid for the life of
  // FrameTracker because the FrameTracker deleter task will be posted to the UI
  // thread before the MouseCursorOverlayController deleter task.
  MouseCursorOverlayController* const cursor_controller_;

  viz::FrameSinkId target_frame_sink_id_;
  gfx::NativeView target_native_view_ = gfx::NativeView();

  // Indicates whether the WebContents's capturer count needs to be decremented.
  bool is_capturing_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameTracker);
};

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    int render_process_id,
    int main_render_frame_id)
    : tracker_(new FrameTracker(AsWeakPtr(),
                                cursor_controller(),
                                render_process_id,
                                main_render_frame_id)) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() = default;

// static
std::unique_ptr<WebContentsVideoCaptureDevice>
WebContentsVideoCaptureDevice::Create(const std::string& device_id) {
  // Parse device_id into render_process_id and main_render_frame_id.
  WebContentsMediaCaptureId media_id;
  if (!WebContentsMediaCaptureId::Parse(device_id, &media_id)) {
    return nullptr;
  }
  return std::make_unique<WebContentsVideoCaptureDevice>(
      media_id.render_process_id, media_id.main_render_frame_id);
}

void WebContentsVideoCaptureDevice::WillStart() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FrameTracker::WillStartCapturingWebContents,
                     tracker_->AsWeakPtr(),
                     capture_params().SuggestConstraints().max_frame_size));
}

void WebContentsVideoCaptureDevice::DidStop() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FrameTracker::DidStopCapturingWebContents,
                                tracker_->AsWeakPtr()));
}

}  // namespace content
