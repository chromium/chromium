// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/aura_window_video_capture_device.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_features.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/browser/media/capture/slow_window_capturer_chromeos.h"
#include "content/public/common/content_features.h"
#endif

namespace content {

// Threading note: This is constructed on the device thread, while the
// destructor and the rest of the class will run exclusively on the UI thread.
class AuraWindowVideoCaptureDevice::WindowTracker final
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

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
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
    FrameSinkVideoCaptureDevice::VideoCaptureTarget target;
    if (target_window_) {
      target.frame_sink_id = target_window_->GetRootWindow()->GetFrameSinkId();
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (base::FeatureList::IsEnabled(features::kAuraWindowSubtreeCapture)) {
        if (!target_window_->IsRootWindow()) {
          capture_request_ = target_window_->MakeWindowCapturable();
          target.subtree_capture_id = capture_request_.GetCaptureId();
        }
      }
#endif
    }

    if (target.frame_sink_id.is_valid()) {
      target_ = target;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      force_visible_.emplace(target_window_);
#endif
      target_window_->AddObserver(this);
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device_,
                         std::move(target)));
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    force_visible_.reset();
#endif

    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    cursor_controller_->SetTargetView(gfx::NativeView());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnWindowAddedToRootWindow(aura::Window* window) final {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(window, target_window_);

    // The legacy capture path doesn't need to track frame sink ID changes.
    if (!base::FeatureList::IsEnabled(features::kAuraWindowSubtreeCapture)) {
      return;
    }

    viz::FrameSinkId new_frame_sink_id =
        target_window_->GetRootWindow()->GetFrameSinkId();

    // Since the window is not destroyed, only re-parented, we can keep the
    // same subtree ID and only update the FrameSinkId of the target.
    if (new_frame_sink_id != target_.frame_sink_id) {
      target_.frame_sink_id = new_frame_sink_id;
      device_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FrameSinkVideoCaptureDevice::OnTargetChanged, device_,
                         target_));
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
  MouseCursorOverlayController* const cursor_controller_;

  const DesktopMediaID::Type target_type_;

  aura::Window* target_window_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_visible_;
#endif

  aura::ScopedWindowCaptureRequest capture_request_;
  FrameSinkVideoCaptureDevice::VideoCaptureTarget target_;

  DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

AuraWindowVideoCaptureDevice::AuraWindowVideoCaptureDevice(
    const DesktopMediaID& source_id)
    : tracker_(new WindowTracker(AsWeakPtr(), cursor_controller(), source_id)) {
}

AuraWindowVideoCaptureDevice::~AuraWindowVideoCaptureDevice() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AuraWindowVideoCaptureDevice::CreateCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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

            if (tracker->target_type() == DesktopMediaID::TYPE_WINDOW &&
                !base::FeatureList::IsEnabled(
                    features::kAuraWindowSubtreeCapture)) {
              VLOG(1) << "AuraWindowVideoCaptureDevice is using the legacy "
                         "slow capturer.";
              mojo::MakeSelfOwnedReceiver(
                  std::make_unique<SlowWindowCapturerChromeOS>(
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
