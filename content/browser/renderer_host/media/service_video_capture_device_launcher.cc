// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

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
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);

  if (stream_type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    // This launcher only supports MediaStreamType::DEVICE_VIDEO_CAPTURE.
    NOTREACHED();
    return;
  }

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

  auto receiver_adapter =
      std::make_unique<video_capture::ReceiverMediaToMojoAdapter>(
          std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
              std::move(receiver),
              base::CreateSingleThreadTaskRunner({BrowserThread::IO})));
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

  // TODO(crbug.com/925083)
  media::VideoCaptureParams new_params = params;
  new_params.power_line_frequency =
      media::VideoCaptureDevice::GetPowerLineFrequency(params);

  // GpuMemoryBuffer-based VideoCapture buffer works only on the Chrome OS
  // VideoCaptureDevice implementation.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoCaptureUseGpuMemoryBuffer)) {
    new_params.buffer_type = media::VideoCaptureBufferType::kGpuMemoryBuffer;
  }

  // Note that we set |force_reopen_with_new_settings| to true in order
  // to avoid the situation that a requests to open (or reopen) a device
  // that has just been closed with different settings ends up getting the old
  // settings, because from the perspective of the service, the device was still
  // in use. In order to be able to set |force_reopen_with_new_settings|, we
  // have to refactor code here and upstream to wait for a callback from the
  // service indicating that the device closing is complete.
  source->CreatePushSubscription(
      std::move(pending_remote_proxy), new_params,
      true /*force_reopen_with_new_settings*/, std::move(subscription_receiver),
      base::BindOnce(
          // Use of Unretained |this| is safe, because |done_cb_| guarantees
          // that |this| stays alive.
          &ServiceVideoCaptureDeviceLauncher::OnCreatePushSubscriptionCallback,
          base::Unretained(this), std::move(source), std::move(subscription),
          std::move(connection_lost_cb)));
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void ServiceVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

void ServiceVideoCaptureDeviceLauncher::OnCreatePushSubscriptionCallback(
    mojo::Remote<video_capture::mojom::VideoSource> source,
    mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    base::OnceClosure connection_lost_cb,
    video_capture::mojom::CreatePushSubscriptionResultCode result_code,
    const media::VideoCaptureParams& params) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(callbacks_);
  DCHECK(done_cb_);
  subscription.set_disconnect_handler(base::DoNothing());
  const bool abort_requested = (state_ == State::DEVICE_START_ABORTING);
  state_ = State::READY_TO_LAUNCH;
  Callbacks* callbacks = callbacks_;
  callbacks_ = nullptr;
  switch (result_code) {
    case video_capture::mojom::CreatePushSubscriptionResultCode::
        kCreatedWithRequestedSettings:  // Fall through.
    case video_capture::mojom::CreatePushSubscriptionResultCode::
        kCreatedWithDifferentSettings:
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
    case video_capture::mojom::CreatePushSubscriptionResultCode::kFailed:
      ConcludeLaunchDeviceWithFailure(
          abort_requested,
          media::VideoCaptureError::
              kServiceDeviceLauncherServiceRespondedWithDeviceNotFound,
          std::move(service_connection_), callbacks, std::move(done_cb_));
      return;
  }
}

void ServiceVideoCaptureDeviceLauncher::
    OnConnectionLostWhileWaitingForCallback() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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
