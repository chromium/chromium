// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_video_capture_device.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"

namespace content {

// Threading note: This is constructed on the device thread, while the
// destructor and the rest of the class will run exclusively on the UI thread.
class AuraWindowVideoCaptureDevice::WindowTracker final
    : public aura::WindowObserver {
 public:
  WindowTracker(base::WeakPtr<AuraWindowVideoCaptureDevice> device,
                MouseCursorOverlayController* cursor_controller,
                const DesktopMediaID& source_id)
      : device_(std::move(device)),
        device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        cursor_controller_(cursor_controller),
        target_type_(source_id.type) {
    DCHECK(device_task_runner_);
    DCHECK(cursor_controller_);

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&WindowTracker::ResolveTarget,
                                  weak_ptr_factory_.GetWeakPtr(), source_id));
  }

  WindowTracker(const WindowTracker&) = delete;
  WindowTracker& operator=(const WindowTracker&) = delete;

  ~WindowTracker() final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (target_window_) {
      target_window_->RemoveObserver(this);
    }
  }

  DesktopMediaID::Type target_type() const { return target_type_; }

  aura::Window* target_window() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    return target_window_;
  }

 private:
  // Determines which frame sink and aura::Window should be targeted for capture
  // and notifies the device.
  void ResolveTarget(const DesktopMediaID& source_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Since ResolveTarget() should only ever be called once, expect
    // |target_window_| to be null at this point.
    DCHECK(!target_window_);

    target_window_ = DesktopMediaID::GetNativeWindowById(source_id);
    aura::Window* const root_window =
        target_window_ ? target_window_->GetRootWindow() : nullptr;
    if (!target_window_ || !root_window->GetFrameSinkId().is_valid()) {
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                         device_));
      return;
    }

    target_ = viz::VideoCaptureTarget(root_window->GetFrameSinkId());
    if (!target_window_->IsRootWindow()) {
      capture_request_ = target_window_->MakeWindowCapturable();
      target_->sub_target = capture_request_.GetCaptureId();
    }

    video_capture_lock_ = target_window_->GetHost()->CreateVideoCaptureLock();
#if BUILDFLAG(IS_CHROMEOS)
    force_visible_.emplace(target_window_);
#endif
    target_window_->AddObserver(this);
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device_,
                       target_, /*sub_capture_target_version=*/0));

    // Note: The MouseCursorOverlayController runs on the UI thread. It's also
    // important that SetTargetView() be called in the current stack while
    // |target_window_| is known to be a valid pointer.
    // http://crbug.com/818679
    //
    // NOTE: for Aura capture, the cursor controller's view should always be
    // the root compositor frame sink.
    cursor_controller_->SetTargetView(root_window);
  }

  // aura::WindowObserver override.
  void OnWindowDestroying(aura::Window* window) final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(window, target_window_);

    video_capture_lock_.reset();
    target_window_->RemoveObserver(this);
    target_window_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS)
    force_visible_.reset();
#endif

    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    cursor_controller_->SetTargetView(gfx::NativeView());
  }

#if BUILDFLAG(IS_CHROMEOS)
  void OnWindowAddedToRootWindow(aura::Window* window) final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(window, target_window_);

    viz::FrameSinkId new_frame_sink_id =
        target_window_->GetRootWindow()->GetFrameSinkId();

    // Since the window is not destroyed, only re-parented, we can keep the
    // same subtree ID and only update the FrameSinkId of the target.
    DCHECK(target_);
    if (new_frame_sink_id != target_->frame_sink_id) {
      target_->frame_sink_id = new_frame_sink_id;
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device_,
                         target_.value(), /*sub_capture_target_version=*/0));
    }
  }
#endif

 private:
  // |device_| may be dereferenced only by tasks run by |device_task_runner_|.
  const base::WeakPtr<FrameSinkVideoCaptureDevice> device_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Owned by FrameSinkVideoCaptureDevice. This will be valid for the life of
  // WindowTracker because the WindowTracker deleter task will be posted to the
  // UI thread before the MouseCursorOverlayController deleter task.
  const raw_ptr<MouseCursorOverlayController> cursor_controller_;

  const DesktopMediaID::Type target_type_;

  raw_ptr<aura::Window> target_window_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  std::optional<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_visible_;
#endif

  aura::ScopedWindowCaptureRequest capture_request_;
  std::optional<viz::VideoCaptureTarget> target_;

  std::unique_ptr<aura::WindowTreeHost::VideoCaptureLock> video_capture_lock_;
  base::WeakPtrFactory<AuraWindowVideoCaptureDevice::WindowTracker>
      weak_ptr_factory_{this};
};

AuraWindowVideoCaptureDevice::AuraWindowVideoCaptureDevice(
    const DesktopMediaID& source_id) {
  tracker_.reset(new WindowTracker(weak_ptr_factory_.GetWeakPtr(),
                                   cursor_controller(), source_id));
}

AuraWindowVideoCaptureDevice::~AuraWindowVideoCaptureDevice() = default;

}  // namespace content
