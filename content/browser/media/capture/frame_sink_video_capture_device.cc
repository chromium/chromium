// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/device_service.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video_capture_types.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
#include "components/viz/common/gpu/context_provider.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/common/capabilities.h"
#endif

namespace content {

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr int32_t kMouseCursorStackingIndex = 1;
#endif

// Transfers ownership of an object to a std::unique_ptr with a custom deleter
// that ensures the object is destroyed on the UI BrowserThread.
template <typename T>
std::unique_ptr<T, BrowserThread::DeleteOnUIThread> RescopeToUIThread(
    std::unique_ptr<T>&& ptr) {
  return std::unique_ptr<T, BrowserThread::DeleteOnUIThread>(ptr.release());
}

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

scoped_refptr<viz::ContextProvider> GetContextProvider() {
#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
  auto* image_transport_factory = ImageTransportFactory::GetInstance();
  DCHECK(image_transport_factory);

  auto* ui_context_factory = image_transport_factory->GetContextFactory();
  if (!ui_context_factory) {
    return nullptr;
  }

  return ui_context_factory->SharedMainThreadContextProvider();
#else
  return nullptr;
#endif
}

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice()
    : cursor_controller_(
          RescopeToUIThread(std::make_unique<MouseCursorOverlayController>())) {
  DCHECK(cursor_controller_);
}
#else
FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice() = default;
#endif

FrameSinkVideoCaptureDevice::~FrameSinkVideoCaptureDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_) << "StopAndDeAllocate() was never called after start.";

  if (context_provider_) {
    context_provider_->RemoveObserver(this);
  }
}

bool FrameSinkVideoCaptureDevice::CanSupportNV12Format() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  if (!gpu_data_manager) {
    return false;
  }

  // If GPU compositing is disabled, we cannot use NV12 (software renderer does
  // not support returning results in textures, and FrameSinkVideoCapturerImpl
  // does not support NV12 otherwise):
  if (gpu_data_manager->IsGpuCompositingDisabled()) {
    return false;
  }

  // We only support NV12 if GL_EXT_texture_rg extension is available. GPU
  // capabilities need to be present in order to determine that.
  if (!gpu_capabilities_) {
    return false;
  }

  // If present, GPU capabilities should already be up to date (this is ensured
  // by subscribing to context lost events on the |context_provider_|):
  return gpu_capabilities_->texture_rg;
}

media::VideoPixelFormat
FrameSinkVideoCaptureDevice::GetDesiredVideoPixelFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capture_params_.requested_format.pixel_format !=
      media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN)
    return capture_params_.requested_format.pixel_format;

  return CanSupportNV12Format() ? media::VideoPixelFormat::PIXEL_FORMAT_NV12
                                : media::VideoPixelFormat::PIXEL_FORMAT_I420;
}

void FrameSinkVideoCaptureDevice::ObserveContextProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (context_provider_) {
    DCHECK(gpu_capabilities_.has_value());
    // If context provider is non-null, we already should have obtained the
    // GPU capabilities from it, and they will be kept up to date.
    return;
  }

  context_provider_ = GetContextProvider();
  if (!context_provider_) {
    // We couldn't get the context provider - either we're on a platform
    // where the method we use to try to get it is not supported, or something
    // else has failed. In any case, treat this as lack of GPU capabilities.
    gpu_capabilities_ = absl::nullopt;
    return;
  }

  // We have obtained an instance of context provider - start observing it for
  // changes and refresh stored GPU capabilities:
  context_provider_->AddObserver(this);
  gpu_capabilities_ = context_provider_->ContextCapabilities();
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
                          kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError);
    return;
  }

  capture_params_ = params;

  WillStart();
  DCHECK(!receiver_);
  receiver_ = std::move(receiver);

  // Shutdown the prior capturer, if any.
  MaybeStopConsuming();

  media::VideoPixelFormat pixel_format =
      capture_params_.requested_format.pixel_format;
  if (pixel_format == media::PIXEL_FORMAT_UNKNOWN) {
    // When there's a chance that we will use NV12 pixel format, we need to
    // start observing for changes in the context capabilities.
    ObserveContextProvider();

    // The caller opted into smart pixel format selection, see if we can support
    // NV12 & decide which format to use based on that.
    pixel_format = GetDesiredVideoPixelFormat();
  }

  AllocateCapturer(pixel_format);

  receiver_->OnStarted();

  if (!suspend_requested_) {
    MaybeStartConsuming();
  }

  DCHECK(!wake_lock_);
  RequestWakeLock();
}

void FrameSinkVideoCaptureDevice::AllocateCapturer(
    media::VideoPixelFormat pixel_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  capturer_ = std::make_unique<viz::ClientFrameSinkVideoCapturer>(
      base::BindRepeating(&FrameSinkVideoCaptureDevice::CreateCapturer,
                          base::Unretained(this)));

  capturer_->SetFormat(pixel_format);

  capturer_->SetMinCapturePeriod(
      base::Microseconds(base::saturated_cast<int64_t>(
          base::Time::kMicrosecondsPerSecond /
          capture_params_.requested_format.frame_rate)));
  const auto& constraints = capture_params_.SuggestConstraints();
  capturer_->SetResolutionConstraints(constraints.min_frame_size,
                                      constraints.max_frame_size,
                                      constraints.fixed_aspect_ratio);

  if (target_) {
    capturer_->ChangeTarget(target_, crop_version_);
  }

#if !BUILDFLAG(IS_ANDROID)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MouseCursorOverlayController::Start,
                     cursor_controller_->GetWeakPtr(),
                     capturer_->CreateOverlay(kMouseCursorStackingIndex),
                     base::ThreadTaskRunnerHandle::Get()));
#endif
}

void FrameSinkVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // FrameSinkVideoCaptureDevice does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED();
}

void FrameSinkVideoCaptureDevice::OnContextLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(context_provider_);

  context_provider_->RemoveObserver(this);
  context_provider_ = nullptr;

  ObserveContextProvider();
  RestartCapturerIfNeeded();
}

void FrameSinkVideoCaptureDevice::RestartCapturerIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media::VideoPixelFormat desired_format = GetDesiredVideoPixelFormat();

  if (capturer_ && capturer_->GetFormat().has_value() &&
      *capturer_->GetFormat() != desired_format) {
    MaybeStopConsuming();
    AllocateCapturer(desired_format);
    if (!suspend_requested_) {
      MaybeStartConsuming();
    }
  }
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

void FrameSinkVideoCaptureDevice::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(
      media::mojom::CropRequestResult::kUnsupportedCaptureDevice);
}

void FrameSinkVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }

#if !BUILDFLAG(IS_ANDROID)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorOverlayController::Stop,
                                cursor_controller_->GetWeakPtr()));
#endif

  MaybeStopConsuming();
  capturer_.reset();
  if (receiver_) {
    receiver_.reset();
    DidStop();
  }
}

void FrameSinkVideoCaptureDevice::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!feedback.frame_id.has_value())
    return;

  const auto index = static_cast<size_t>(feedback.frame_id.value());
  DCHECK_LT(index, frame_callbacks_.size());

  // In most cases, we expect that the mojo InterfacePtr in |frame_callbacks_|
  // should be valid because this method will always be called before the
  // VideoFrameReceiver signals that it is done consuming the frame. However,
  // some capturers (e.g. Lacros) involve some extra mojo hops that may mean
  // we got scheduled after the VideoFrameReceiver signaled it was done.
  const auto& callback = frame_callbacks_[index];
  if (callback.is_bound())
    callback->ProvideFeedback(feedback);
}

void FrameSinkVideoCaptureDevice::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks);

  mojo::Remote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_remote(std::move(callbacks));

  if (!receiver_ || !data) {
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

#if !BUILDFLAG(IS_ANDROID)
  info->metadata.interactive_content =
      cursor_controller_->IsUserInteractingWithView();
#else
  // Since we don't have a cursor controller, on Android we'll just always
  // assume the user is interacting with the view.
  info->metadata.interactive_content = true;
#endif

  // Pass the video frame to the VideoFrameReceiver. This is done by first
  // passing the shared memory buffer handle and then notifying it that a new
  // frame is ready to be read from the buffer.
  receiver_->OnNewBuffer(buffer_id, std::move(data));
  receiver_->OnFrameReadyInBuffer(
      media::ReadyFrameInBuffer(
          buffer_id, buffer_id,
          std::make_unique<media::ScopedFrameDoneHelper>(base::BindOnce(
              &FrameSinkVideoCaptureDevice::OnFramePropagationComplete,
              weak_factory_.GetWeakPtr(), buffer_id)),
          std::move(info)),
      {});
}

void FrameSinkVideoCaptureDevice::OnFrameWithEmptyRegionCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_) {
    return;
  }

  receiver_->OnFrameWithEmptyRegionCapture();
}

void FrameSinkVideoCaptureDevice::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method would never be called if FrameSinkVideoCaptureDevice explicitly
  // called capturer_->StopAndResetConsumer(), because the binding is closed at
  // that time. Therefore, a call to this method means that the capturer cannot
  // continue; and that's a permanent failure.
  OnFatalError("Capturer service cannot continue.");
}

void FrameSinkVideoCaptureDevice::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (receiver_) {
    receiver_->OnLog(message);
  }
}

void FrameSinkVideoCaptureDevice::OnTargetChanged(
    const absl::optional<viz::VideoCaptureTarget>& target,
    uint32_t crop_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(crop_version, crop_version_);

  target_ = target;
  crop_version_ = crop_version;

  if (capturer_) {
    capturer_->ChangeTarget(target_, crop_version_);
  }
}

void FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnTargetChanged(absl::nullopt, crop_version_);
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
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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

  capturer_->Start(this,
                   viz::mojom::BufferFormatPreference::kPreferGpuMemoryBuffer);
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

void FrameSinkVideoCaptureDevice::RequestWakeLock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  auto receiver = wake_lock_provider.BindNewPipeAndPassReceiver();
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BindWakeLockProvider, std::move(receiver)));
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "screen capture",
      wake_lock_.BindNewPipeAndPassReceiver());

  wake_lock_->RequestWakeLock();
}

}  // namespace content
