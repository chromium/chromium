// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_video_capture_device.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"

#if defined(OS_CHROMEOS)
#include "content/browser/media/capture/lame_window_capturer_chromeos.h"
#endif

namespace content {

// Threading note: This is constructed on the device thread, while the
// destructor and the rest of the class will run exclusively on the UI thread.
class AuraWindowVideoCaptureDevice::WindowTracker
    : public aura::WindowObserver,
      public base::SupportsWeakPtr<
          AuraWindowVideoCaptureDevice::WindowTracker> {
 public:
  WindowTracker(base::WeakPtr<AuraWindowVideoCaptureDevice> device,
                MouseCursorOverlayController* cursor_controller,
                const DesktopMediaID& source_id)
      : device_(std::move(device)),
        device_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        cursor_controller_(cursor_controller),
        target_type_(source_id.type) {
    DCHECK(device_task_runner_);
    DCHECK(cursor_controller_);

    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WindowTracker::ResolveTarget, AsWeakPtr(), source_id));
  }

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
    if (target_window_ &&
#if defined(OS_CHROMEOS)
        // See class comments for LameWindowCapturerChromeOS.
        (source_id.type == DesktopMediaID::TYPE_WINDOW ||
         target_window_->GetFrameSinkId().is_valid()) &&
#else
        target_window_->GetFrameSinkId().is_valid() &&
#endif
        true) {
#if defined(OS_CHROMEOS)
      force_visible_.emplace(target_window_);
#endif
      target_window_->AddObserver(this);
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device_,
                         target_window_->GetFrameSinkId()));
      // Note: The MouseCursorOverlayController runs on the UI thread. It's also
      // important that SetTargetView() be called in the current stack while
      // |target_window_| is known to be a valid pointer.
      // http://crbug.com/818679
      cursor_controller_->SetTargetView(target_window_);
    } else {
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                         device_));
    }
  }

  // aura::WindowObserver override.
  void OnWindowDestroying(aura::Window* window) final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(window, target_window_);

    target_window_->RemoveObserver(this);
    target_window_ = nullptr;
#if defined(OS_CHROMEOS)
    force_visible_.reset();
#endif

    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    cursor_controller_->SetTargetView(gfx::NativeView());
  }

 private:
  // |device_| may be dereferenced only by tasks run by |device_task_runner_|.
  const base::WeakPtr<FrameSinkVideoCaptureDevice> device_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Owned by FrameSinkVideoCaptureDevice. This will be valid for the life of
  // WindowTracker because the WindowTracker deleter task will be posted to the
  // UI thread before the MouseCursorOverlayController deleter task.
  MouseCursorOverlayController* const cursor_controller_;

  const DesktopMediaID::Type target_type_;

  aura::Window* target_window_ = nullptr;
#if defined(OS_CHROMEOS)
  base::Optional<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_visible_;
#endif

  DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

AuraWindowVideoCaptureDevice::AuraWindowVideoCaptureDevice(
    const DesktopMediaID& source_id)
    : tracker_(new WindowTracker(AsWeakPtr(), cursor_controller(), source_id)) {
}

AuraWindowVideoCaptureDevice::~AuraWindowVideoCaptureDevice() = default;

#if defined(OS_CHROMEOS)
void AuraWindowVideoCaptureDevice::CreateCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](base::WeakPtr<WindowTracker> tracker_ptr,
             mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                 receiver) {
            WindowTracker* const tracker = tracker_ptr.get();
            if (!tracker) {
              // WindowTracker was destroyed in the meantime, due to early
              // shutdown.
              return;
            }

            if (tracker->target_type() == DesktopMediaID::TYPE_WINDOW) {
              VLOG(1) << "AuraWindowVideoCaptureDevice is using the LAME "
                         "capturer. :(";
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<LameWindowCapturerChromeOS>(
                      tracker->target_window()),
                  std::move(receiver));
            } else {
              VLOG(1) << "AuraWindowVideoCaptureDevice is using the frame "
                         "sink capturer. :)";
              CreateCapturerViaGlobalManager(std::move(receiver));
            }
          },
          tracker_->AsWeakPtr(), std::move(receiver)));
}
#endif

}  // namespace content
