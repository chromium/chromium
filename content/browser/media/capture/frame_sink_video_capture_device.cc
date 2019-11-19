// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/system_connector.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace content {

namespace {

constexpr int32_t kMouseCursorStackingIndex = 1;

// Transfers ownership of an object to a std::unique_ptr with a custom deleter
// that ensures the object is destroyed on the UI BrowserThread.
template <typename T>
std::unique_ptr<T, BrowserThread::DeleteOnUIThread> RescopeToUIThread(
    std::unique_ptr<T>&& ptr) {
  return std::unique_ptr<T, BrowserThread::DeleteOnUIThread>(ptr.release());
}

// Adapter for a VideoFrameReceiver to notify once frame consumption is
// complete. VideoFrameReceiver requires owning an object that it will destroy
// once consumption is complete. This class adapts between that scheme and
// running a "done callback" to notify that consumption is complete.
class ScopedFrameDoneHelper
    : public base::ScopedClosureRunner,
      public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  explicit ScopedFrameDoneHelper(base::OnceClosure done_callback)
      : base::ScopedClosureRunner(std::move(done_callback)) {}
  ~ScopedFrameDoneHelper() final = default;
};

std::unique_ptr<service_manager::Connector> MaybeGetServiceConnector() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // In some testing environments, the system Connector isn't initialized.
  if (auto* connector = GetSystemConnector())
    return connector->Clone();  // Clone for use on a different thread.
  return nullptr;
}

}  // namespace

FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice()
    : cursor_controller_(
          RescopeToUIThread(std::make_unique<MouseCursorOverlayController>())) {
  DCHECK(cursor_controller_);
}

FrameSinkVideoCaptureDevice::~FrameSinkVideoCaptureDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_) << "StopAndDeAllocate() was never called after start.";
}

void FrameSinkVideoCaptureDevice::AllocateAndStartWithReceiver(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  DCHECK(receiver);

  // If the device has already ended on a fatal error, abort immediately.
  if (fatal_error_message_) {
    receiver->OnLog(*fatal_error_message_);
    receiver->OnError(media::VideoCaptureError::
                          kFrameSinkVideoCaptureDeviceAleradyEndedOnFatalError);
    return;
  }

  capture_params_ = params;
  WillStart();
  DCHECK(!receiver_);
  receiver_ = std::move(receiver);

  // Shutdown the prior capturer, if any.
  MaybeStopConsuming();

  capturer_ = std::make_unique<viz::ClientFrameSinkVideoCapturer>(
      base::BindRepeating(&FrameSinkVideoCaptureDevice::CreateCapturer,
                          base::Unretained(this)));

  capturer_->SetFormat(capture_params_.requested_format.pixel_format,
                       gfx::ColorSpace::CreateREC709());
  capturer_->SetMinCapturePeriod(
      base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
          base::Time::kMicrosecondsPerSecond /
          capture_params_.requested_format.frame_rate)));
  const auto& constraints = capture_params_.SuggestConstraints();
  capturer_->SetResolutionConstraints(constraints.min_frame_size,
                                      constraints.max_frame_size,
                                      constraints.fixed_aspect_ratio);

  if (target_.is_valid()) {
    capturer_->ChangeTarget(target_);
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MouseCursorOverlayController::Start,
                     cursor_controller_->GetWeakPtr(),
                     capturer_->CreateOverlay(kMouseCursorStackingIndex),
                     base::ThreadTaskRunnerHandle::Get()));

  receiver_->OnStarted();

  if (!suspend_requested_) {
    MaybeStartConsuming();
  }

  DCHECK(!wake_lock_);
  // Gets a service_manager::Connector first, then request a wake lock.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI}, base::BindOnce(&MaybeGetServiceConnector),
      base::BindOnce(&FrameSinkVideoCaptureDevice::RequestWakeLock,
                     weak_factory_.GetWeakPtr()));
}

void FrameSinkVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // FrameSinkVideoCaptureDevice does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED();
}

void FrameSinkVideoCaptureDevice::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_ && !suspend_requested_) {
    capturer_->RequestRefreshFrame();
  }
}

void FrameSinkVideoCaptureDevice::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = true;
  MaybeStopConsuming();
}

void FrameSinkVideoCaptureDevice::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = false;
  MaybeStartConsuming();
}

void FrameSinkVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&MouseCursorOverlayController::Stop,
                                cursor_controller_->GetWeakPtr()));

  MaybeStopConsuming();
  capturer_.reset();
  if (receiver_) {
    receiver_.reset();
    DidStop();
  }
}

void FrameSinkVideoCaptureDevice::OnUtilizationReport(int frame_feedback_id,
                                                      double utilization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Assumption: The mojo InterfacePtr in |frame_callbacks_| should be valid at
  // this point because this method will always be called before the
  // VideoFrameReceiver signals it is done consuming the frame.
  const auto index = static_cast<size_t>(frame_feedback_id);
  DCHECK_LT(index, frame_callbacks_.size());
  frame_callbacks_[index]->ProvideFeedback(utilization);
}

void FrameSinkVideoCaptureDevice::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks);

  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  if (!receiver_ || !data.IsValid()) {
    callbacks_remote->Done();
    return;
  }

  // Search for the next available element in |frame_callbacks_| and bind
  // |callbacks| there.
  size_t index = 0;
  for (;; ++index) {
    if (index == frame_callbacks_.size()) {
      // The growth of |frame_callbacks_| should be bounded because the
      // viz::mojom::FrameSinkVideoCapturer should enforce an upper-bound on the
      // number of frames in-flight.
      constexpr size_t kMaxInFlightFrames = 32;  // Arbitrarily-chosen limit.
      DCHECK_LT(frame_callbacks_.size(), kMaxInFlightFrames);
      frame_callbacks_.emplace_back(std::move(callbacks_remote));
      break;
    }
    if (!frame_callbacks_[index].is_bound()) {
      frame_callbacks_[index] = std::move(callbacks_remote);
      break;
    }
  }
  const BufferId buffer_id = static_cast<BufferId>(index);

  // Set the INTERACTIVE_CONTENT frame metadata.
  media::VideoFrameMetadata modified_metadata;
  modified_metadata.MergeInternalValuesFrom(info->metadata);
  modified_metadata.SetBoolean(media::VideoFrameMetadata::INTERACTIVE_CONTENT,
                               cursor_controller_->IsUserInteractingWithView());
  info->metadata = modified_metadata.GetInternalValues().Clone();

  // Pass the video frame to the VideoFrameReceiver. This is done by first
  // passing the shared memory buffer handle and then notifying it that a new
  // frame is ready to be read from the buffer.
  receiver_->OnNewBuffer(
      buffer_id,
      media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(std::move(data)));
  receiver_->OnFrameReadyInBuffer(
      buffer_id, buffer_id,
      std::make_unique<ScopedFrameDoneHelper>(
          media::BindToCurrentLoop(base::BindOnce(
              &FrameSinkVideoCaptureDevice::OnFramePropagationComplete,
              weak_factory_.GetWeakPtr(), buffer_id))),
      std::move(info));
}

void FrameSinkVideoCaptureDevice::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method would never be called if FrameSinkVideoCaptureDevice explicitly
  // called capturer_->StopAndResetConsumer(), because the binding is closed at
  // that time. Therefore, a call to this method means that the capturer cannot
  // continue; and that's a permanent failure.
  OnFatalError("Capturer service cannot continue.");
}

void FrameSinkVideoCaptureDevice::OnTargetChanged(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  target_ = frame_sink_id;
  if (capturer_) {
    if (target_.is_valid()) {
      capturer_->ChangeTarget(target_);
    } else {
      capturer_->ChangeTarget(base::nullopt);
    }
  }
}

void FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnTargetChanged(viz::FrameSinkId());
  OnFatalError("Capture target has been permanently lost.");
}

void FrameSinkVideoCaptureDevice::WillStart() {}

void FrameSinkVideoCaptureDevice::DidStop() {}

void FrameSinkVideoCaptureDevice::CreateCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  CreateCapturerViaGlobalManager(std::move(receiver));
}

// static
void FrameSinkVideoCaptureDevice::CreateCapturerViaGlobalManager(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  // Send the receiver to UI thread because that's where HostFrameSinkManager
  // lives.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          [](mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                 receiver) {
            viz::HostFrameSinkManager* const manager =
                GetHostFrameSinkManager();
            DCHECK(manager);
            manager->CreateVideoCapturer(std::move(receiver));
          },
          std::move(receiver)));
}

void FrameSinkVideoCaptureDevice::MaybeStartConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_ || !capturer_) {
    return;
  }

  capturer_->Start(this);
}

void FrameSinkVideoCaptureDevice::MaybeStopConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_)
    capturer_->StopAndResetConsumer();
}

void FrameSinkVideoCaptureDevice::OnFramePropagationComplete(
    BufferId buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Notify the VideoFrameReceiver that the buffer is no longer valid.
  if (receiver_) {
    receiver_->OnBufferRetired(buffer_id);
  }

  // Notify the capturer that consumption of the frame is complete.
  const size_t index = static_cast<size_t>(buffer_id);
  DCHECK_LT(index, frame_callbacks_.size());
  auto& callbacks_ptr = frame_callbacks_[index];
  callbacks_ptr->Done();
  callbacks_ptr.reset();
}

void FrameSinkVideoCaptureDevice::OnFatalError(std::string message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fatal_error_message_ = std::move(message);
  if (receiver_) {
    receiver_->OnLog(*fatal_error_message_);
    receiver_->OnError(media::VideoCaptureError::
                           kFrameSinkVideoCaptureDeviceEncounteredFatalError);
  }

  StopAndDeAllocate();
}

void FrameSinkVideoCaptureDevice::RequestWakeLock(
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connector) {
    return;
  }

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  connector->Connect(device::mojom::kServiceName,
                     wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "screen capture",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

}  // namespace content
