// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/media_switches.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "content/browser/gpu/gpu_data_manager_impl.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "media/capture/video/apple/video_capture_device_factory_apple.h"
#endif

namespace content {

namespace {

void ConcludeLaunchDeviceWithSuccess(
    mojo::Remote<video_capture::mojom::VideoSource> source,
    mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    base::OnceClosure connection_lost_cb,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  subscription->Activate();
  callbacks->OnDeviceLaunched(
      std::make_unique<ServiceLaunchedVideoCaptureDevice>(
          std::move(source), std::move(subscription),
          std::move(connection_lost_cb)));
  std::move(done_cb).Run();
}

void ConcludeLaunchDeviceWithFailure(
    bool abort_requested,
    media::VideoCaptureError error,
    scoped_refptr<RefCountedVideoSourceProvider> service_connection,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  service_connection.reset();
  if (abort_requested)
    callbacks->OnDeviceLaunchAborted();
  else
    callbacks->OnDeviceLaunchFailed(error);
  std::move(done_cb).Run();
}

}  // anonymous namespace

ServiceVideoCaptureDeviceLauncher::ServiceVideoCaptureDeviceLauncher(
    ConnectToDeviceFactoryCB connect_to_source_provider_cb)
    : connect_to_source_provider_cb_(std::move(connect_to_source_provider_cb)),
      state_(State::READY_TO_LAUNCH),
      callbacks_(nullptr) {}

ServiceVideoCaptureDeviceLauncher::~ServiceVideoCaptureDeviceLauncher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    Callbacks* callbacks,
    base::OnceClosure done_cb,
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::READY_TO_LAUNCH);

  auto scoped_trace = ScopedCaptureTrace::CreateIfEnabled(
      "ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync");

  // This launcher only supports MediaStreamType::DEVICE_VIDEO_CAPTURE.
  CHECK_EQ(stream_type, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);

  connect_to_source_provider_cb_.Run(&service_connection_);
  if (!service_connection_->source_provider().is_bound()) {
    // This can happen when the connection to the service was lost.
    ConcludeLaunchDeviceWithFailure(
        false,
        media::VideoCaptureError::
            kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart,
        std::move(service_connection_), callbacks, std::move(done_cb));
    return;
  }

  if (receiver) {
    std::ostringstream string_stream;
    string_stream
        << "ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync: Asking "
           "video capture service to create source for device_id = "
        << device_id;
    receiver->OnLog(string_stream.str());
  }

  // Ownership of |done_cb| is moved to |this|. It is not sufficient to attach
  // it to the callback passed to CreatePushSubscription(), because the
  // connection to the service may get torn down before |callbacks| are
  // invoked.
  done_cb_ = std::move(done_cb);
  callbacks_ = callbacks;
  mojo::Remote<video_capture::mojom::VideoSource> source;
  service_connection_->source_provider()->GetVideoSource(
      device_id, source.BindNewPipeAndPassReceiver());

  if (video_effects_processor) {
    source->RegisterVideoEffectsProcessor(std::move(video_effects_processor));
  }

  auto receiver_adapter =
      std::make_unique<video_capture::ReceiverMediaToMojoAdapter>(
          std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
              std::move(receiver), GetIOThreadTaskRunner({})));
  mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
      pending_remote_proxy;
  mojo::MakeSelfOwnedReceiver(
      std::move(receiver_adapter),
      pending_remote_proxy.InitWithNewPipeAndPassReceiver());

  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription> subscription;
  // Create message pipe so that we can subsequently call
  // subscription.set_disconnect_handler().
  auto subscription_receiver = subscription.BindNewPipeAndPassReceiver();
  // Use of Unretained(this) is safe, because |done_cb_| guarantees that |this|
  // stays alive.
  subscription.set_disconnect_handler(
      base::BindOnce(&ServiceVideoCaptureDeviceLauncher::
                         OnConnectionLostWhileWaitingForCallback,
                     base::Unretained(this)));

  // TODO(crbug.com/40610987)
  media::VideoCaptureParams new_params = params;
  new_params.power_line_frequency =
      media::VideoCaptureDevice::GetPowerLineFrequency(params);

  // GpuMemoryBuffer-based VideoCapture buffer works only on the Chrome OS,
  // Windows and Linux VideoCaptureDevice implementations.
#if BUILDFLAG(IS_WIN)
  if (media::IsMediaFoundationD3D11VideoCaptureEnabled() &&
      params.requested_format.pixel_format == media::PIXEL_FORMAT_NV12) {
    new_params.buffer_type = media::VideoCaptureBufferType::kGpuMemoryBuffer;
  }
#elif BUILDFLAG(IS_MAC)
  // For mac(https://crbug.com/1175142), zero-copy is always enabled unless the
  // user explicitly asks to disable it.
  if (media::ShouldEnableGpuMemoryBuffer(device_id)) {
    new_params.buffer_type = media::VideoCaptureBufferType::kGpuMemoryBuffer;
  }
#else
  if (switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
#if BUILDFLAG(IS_LINUX)
    // On Linux, additionally check whether the NV12 GPU memory buffer is
    // supported.
    if (GpuDataManagerImpl::GetInstance()->IsGpuMemoryBufferNV12Supported())
#endif
      new_params.buffer_type = media::VideoCaptureBufferType::kGpuMemoryBuffer;
  }
#endif

  // Note that we set |force_reopen_with_new_settings| to true in order
  // to avoid the situation that a requests to open (or reopen) a device
  // that has just been closed with different settings ends up getting the old
  // settings, because from the perspective of the service, the device was still
  // in use. In order to be able to set |force_reopen_with_new_settings|, we
  // have to refactor code here and upstream to wait for a callback from the
  // service indicating that the device closing is complete.
  video_capture::mojom::VideoSource* source_ptr = source.get();
  source_ptr->CreatePushSubscription(
      std::move(pending_remote_proxy), new_params,
      true /*force_reopen_with_new_settings*/, std::move(subscription_receiver),
      base::BindOnce(
          // Use of Unretained |this| is safe, because |done_cb_| guarantees
          // that |this| stays alive.
          &ServiceVideoCaptureDeviceLauncher::OnCreatePushSubscriptionCallback,
          base::Unretained(this), std::move(source), std::move(subscription),
          std::move(connection_lost_cb), std::move(scoped_trace)));
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void ServiceVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

void ServiceVideoCaptureDeviceLauncher::OnCreatePushSubscriptionCallback(
    mojo::Remote<video_capture::mojom::VideoSource> source,
    mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    base::OnceClosure connection_lost_cb,
    std::unique_ptr<ScopedCaptureTrace> scoped_trace,
    video_capture::mojom::CreatePushSubscriptionResultCodePtr result_code,
    const media::VideoCaptureParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks_);
  DCHECK(done_cb_);
  subscription.set_disconnect_handler(base::DoNothing());
  const bool abort_requested = (state_ == State::DEVICE_START_ABORTING);
  state_ = State::READY_TO_LAUNCH;
  Callbacks* callbacks = callbacks_;
  callbacks_ = nullptr;
  switch (result_code->which()) {
    case video_capture::mojom::CreatePushSubscriptionResultCode::Tag::
        kSuccessCode:
      if (abort_requested) {
        subscription.reset();
        source.reset();
        service_connection_.reset();
        callbacks->OnDeviceLaunchAborted();
        std::move(done_cb_).Run();
        return;
      }
      ConcludeLaunchDeviceWithSuccess(
          std::move(source), std::move(subscription),
          std::move(connection_lost_cb), callbacks, std::move(done_cb_));
      return;
    case video_capture::mojom::CreatePushSubscriptionResultCode::Tag::
        kErrorCode:
      media::VideoCaptureError error = result_code->get_error_code();
      DCHECK_NE(error, media::VideoCaptureError::kNone);
      ConcludeLaunchDeviceWithFailure(abort_requested, error,
                                      std::move(service_connection_), callbacks,
                                      std::move(done_cb_));
      return;
  }
}

void ServiceVideoCaptureDeviceLauncher::
    OnConnectionLostWhileWaitingForCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks_);
  const bool abort_requested = (state_ == State::DEVICE_START_ABORTING);
  state_ = State::READY_TO_LAUNCH;
  Callbacks* callbacks = callbacks_;
  callbacks_ = nullptr;
  ConcludeLaunchDeviceWithFailure(
      abort_requested,
      media::VideoCaptureError::
          kServiceDeviceLauncherConnectionLostWhileWaitingForCallback,
      std::move(service_connection_), callbacks, std::move(done_cb_));
}

}  // namespace content
