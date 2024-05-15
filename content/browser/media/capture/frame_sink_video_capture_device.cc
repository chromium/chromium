// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video_capture_types.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
#include "components/viz/common/gpu/context_provider.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/common/capabilities.h"
#endif

namespace content {

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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

scoped_refptr<viz::RasterContextProvider> GetContextProvider() {
#if BUILDFLAG(IS_MAC) || defined(USE_AURA)
  auto* image_transport_factory = ImageTransportFactory::GetInstance();
  DCHECK(image_transport_factory);

  auto* ui_context_factory = image_transport_factory->GetContextFactory();
  if (!ui_context_factory) {
    return nullptr;
  }

  return ui_context_factory->SharedMainThreadRasterContextProvider();
#else
  return nullptr;
#endif
}

}  // namespace

// Helper class that is used to observe the `viz::ContextProvider` for context
// loss events and communicate latest `gpu::Capabilities` after context loss.
class ContextProviderObserver : viz::ContextLostObserver {
 public:
  using OnGpuCapabilitiesFetched =
      base::RepeatingCallback<void(std::optional<gpu::Capabilities>)>;

  // Constructs the instance of the class. The construction can happen on any
  // thread, but the instance must be destroyed on the UI thread.
  // |on_gpu_capabilities_fetched_| will be invoked after they have been
  // obtained, and then each time they have been re-fetched after a context
  // loss. The callback will be invoked on the sequence that was used to
  // construct this instance.
  explicit ContextProviderObserver(
      OnGpuCapabilitiesFetched on_gpu_capabilities_fetched)
      : on_gpu_capabilities_fetched_(
            base::BindPostTaskToCurrentDefault(on_gpu_capabilities_fetched)) {
    DETACH_FROM_SEQUENCE(main_sequence_checker_);

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ContextProviderObserver::GetContextProviderOnMainSequence,
            weak_factory_.GetWeakPtr()));
  }

  ~ContextProviderObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    if (context_provider_) {
      context_provider_->RemoveObserver(this);
    }
  }

 protected:
  void OnContextLost() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;

    GetContextProviderOnMainSequence();
  }

 private:
  void GetContextProviderOnMainSequence() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

    context_provider_ = GetContextProvider();
    if (!context_provider_) {
      on_gpu_capabilities_fetched_.Run(std::nullopt);
      return;
    }

    context_provider_->AddObserver(this);
    on_gpu_capabilities_fetched_.Run(context_provider_->ContextCapabilities());
  }

  // Task runner on which this instance was created.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  const OnGpuCapabilitiesFetched on_gpu_capabilities_fetched_;

  // Context provider that was used to query the GPU capabilities. May be null.
  scoped_refptr<viz::RasterContextProvider> context_provider_
      GUARDED_BY_CONTEXT(main_sequence_checker_);

  SEQUENCE_CHECKER(main_sequence_checker_);

  // Must be last.
  base::WeakPtrFactory<ContextProviderObserver> weak_factory_{this};
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
}

bool FrameSinkVideoCaptureDevice::CanSupportNV12Format() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/41491504): Determine if we actually need to know the format
  // client-side (which is what this method is used for) beyond sending it over
  // to the service side. If not, compute this information service-side. If yes,
  // port all of the below code to be a check on SharedImageCapabilities once
  // the latter is fully fleshed out (crbug.com/1482371).
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

  // SwiftShader does not support copying directly to NV12.
  if (gpu_data_manager->GetGPUInfo().UsesSwiftShader()) {
    return false;
  }

  // We only support NV12 if GL_EXT_texture_rg extension is available. GPU
  // capabilities need to be present in order to determine that.
  if (!gpu_capabilities_) {
    return false;
  }

  // If present, GPU capabilities should already be up to date (this is ensured
  // by subscribing to context lost events via `context_provider_observer_`
  // helper):
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

  // Create an observer that will invoke `SetGpuCapabilitiesOnDevice()` every
  // time a context was lost:
  context_provider_observer_ = RescopeToUIThread(
      std::make_unique<ContextProviderObserver>(base::BindRepeating(
          &FrameSinkVideoCaptureDevice::SetGpuCapabilitiesOnDevice,
          weak_factory_.GetWeakPtr())));
}

void FrameSinkVideoCaptureDevice::SetGpuCapabilitiesOnDevice(
    std::optional<gpu::Capabilities> capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gpu_capabilities_ = capabilities;

  if (!receiver_) {
    // It seems that we're being called after the receiver was already reset in
    // `StopAndDeAllocate()` but before the dtor ran. If that's the case, we
    // don't need to do anything here since either the dtor will be called
    // shortly, or the observer will be re-initialized to a new instance in
    // `AllocateAndStartWithReceiver()`.
    return;
  }

  if (capturer_) {
    RestartCapturerIfNeeded();
  } else {
    AllocateAndStartWithReceiverInternal();
  }
}

void FrameSinkVideoCaptureDevice::AllocateAndStartWithReceiver(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  DCHECK(receiver);
  DCHECK(!receiver_);

  receiver_ = std::move(receiver);
  capture_params_ = params;

  // If the device has already ended on a fatal error, abort immediately.
  if (fatal_error_message_) {
    receiver_->OnLog(*fatal_error_message_);
    receiver_->OnError(
        media::VideoCaptureError::
            kFrameSinkVideoCaptureDeviceAlreadyEndedOnFatalError);
    receiver_ = nullptr;
    return;
  }

  if (params.requested_format.pixel_format == media::PIXEL_FORMAT_UNKNOWN) {
    // Kick off a task to query GPU context capabilities. The flow will continue
    // in `SetGpuCapabilitiesOnDevice()`, which will then call into
    //  `AllocateAndStartWithReceiverInternal()` because the `capturer_` isn't
    // set yet.
    ObserveContextProvider();
    return;
  }

  AllocateAndStartWithReceiverInternal();
}

void FrameSinkVideoCaptureDevice::AllocateAndStartWithReceiverInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(receiver_);

  WillStart();

  // Shutdown the prior capturer, if any.
  MaybeStopConsuming();

  media::VideoPixelFormat pixel_format =
      capture_params_.requested_format.pixel_format;
  if (pixel_format == media::PIXEL_FORMAT_UNKNOWN) {
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
  DCHECK_NE(pixel_format, media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN);
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
    capturer_->ChangeTarget(target_, sub_capture_target_version_);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MouseCursorOverlayController::Start,
                     cursor_controller_->GetWeakPtr(),
                     capturer_->CreateOverlay(kMouseCursorStackingIndex),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
#endif
}

void FrameSinkVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // FrameSinkVideoCaptureDevice does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED_IN_MIGRATION();
}

void FrameSinkVideoCaptureDevice::RestartCapturerIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Desired pixel format may have changed due to the change in
  // `gpu_capabilities_` - we need to recompute it and determine if the capturer
  // needs to be restarted based on the new desired format:
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

void FrameSinkVideoCaptureDevice::ApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  std::move(callback).Run(
      media::mojom::ApplySubCaptureTargetResult::kUnsupportedCaptureDevice);
}

void FrameSinkVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
    wake_lock_.reset();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorOverlayController::Stop,
                                cursor_controller_->GetWeakPtr()));
#endif

  MaybeStopConsuming();
  capturer_.reset();
  context_provider_observer_.reset();
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
  receiver_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
      buffer_id, buffer_id,
      std::make_unique<media::ScopedFrameDoneHelper>(base::BindOnce(
          &FrameSinkVideoCaptureDevice::OnFramePropagationComplete,
          weak_factory_.GetWeakPtr(), buffer_id)),
      std::move(info)));
}

void FrameSinkVideoCaptureDevice::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_) {
    return;
  }

  receiver_->OnNewSubCaptureTargetVersion(sub_capture_target_version);
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
    const std::optional<viz::VideoCaptureTarget>& target,
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(sub_capture_target_version, sub_capture_target_version_);

  target_ = target;
  sub_capture_target_version_ = sub_capture_target_version;

  if (capturer_) {
    capturer_->ChangeTarget(target_, sub_capture_target_version_);
  }
}

void FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnTargetChanged(std::nullopt, sub_capture_target_version_);
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
