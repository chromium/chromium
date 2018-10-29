// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_device_launcher.h"

#include "base/task/post_task.h"
#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/public/cpp/receiver_media_to_mojo_adapter.h"

namespace content {

namespace {

void ConcludeLaunchDeviceWithSuccess(
    const media::VideoCaptureParams& params,
    video_capture::mojom::DevicePtr device,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  auto receiver_adapter =
      std::make_unique<video_capture::ReceiverMediaToMojoAdapter>(
          std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
              std::move(receiver), base::CreateSingleThreadTaskRunnerWithTraits(
                                       {BrowserThread::IO})));
  video_capture::mojom::ReceiverPtr receiver_proxy;
  mojo::MakeStrongBinding<video_capture::mojom::Receiver>(
      std::move(receiver_adapter), mojo::MakeRequest(&receiver_proxy));
  device->Start(params, std::move(receiver_proxy));
  callbacks->OnDeviceLaunched(
      std::make_unique<ServiceLaunchedVideoCaptureDevice>(
          std::move(device), std::move(connection_lost_cb)));
  base::ResetAndReturn(&done_cb).Run();
}

void ConcludeLaunchDeviceWithFailure(
    bool abort_requested,
    media::VideoCaptureError error,
    std::unique_ptr<VideoCaptureFactoryDelegate> device_factory,
    VideoCaptureDeviceLauncher::Callbacks* callbacks,
    base::OnceClosure done_cb) {
  device_factory.reset();
  if (abort_requested)
    callbacks->OnDeviceLaunchAborted();
  else
    callbacks->OnDeviceLaunchFailed(error);
  base::ResetAndReturn(&done_cb).Run();
}

}  // anonymous namespace

ServiceVideoCaptureDeviceLauncher::ServiceVideoCaptureDeviceLauncher(
    ConnectToDeviceFactoryCB connect_to_device_factory_cb)
    : connect_to_device_factory_cb_(std::move(connect_to_device_factory_cb)),
      state_(State::READY_TO_LAUNCH),
      callbacks_(nullptr) {}

ServiceVideoCaptureDeviceLauncher::~ServiceVideoCaptureDeviceLauncher() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);
}

void ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(state_ == State::READY_TO_LAUNCH);

  if (stream_type != content::MEDIA_DEVICE_VIDEO_CAPTURE) {
    // This launcher only supports MEDIA_DEVICE_VIDEO_CAPTURE.
    NOTREACHED();
    return;
  }

  connect_to_device_factory_cb_.Run(&device_factory_);
  if (!device_factory_->is_bound()) {
    // This can happen when the ServiceVideoCaptureProvider owning
    // |device_factory_| loses connection to the service process and resets
    // |device_factory_|.
    ConcludeLaunchDeviceWithFailure(
        false,
        media::VideoCaptureError::
            kServiceDeviceLauncherLostConnectionToDeviceFactoryDuringDeviceStart,
        std::move(device_factory_), callbacks, std::move(done_cb));
    return;
  }

  if (receiver) {
    std::ostringstream string_stream;
    string_stream
        << "ServiceVideoCaptureDeviceLauncher::LaunchDeviceAsync: Asking "
           "video capture service to create device for device_id = "
        << device_id;
    receiver->OnLog(string_stream.str());
  }

  video_capture::mojom::DevicePtr device;
  auto device_request = mojo::MakeRequest(&device);
  // Ownership of |done_cb| is moved to |this|. It is not sufficient to attach
  // it to the callback passed to |device_factory_->CreateDevice()|, because
  // |device_factory_| may get torn down before the callback is invoked.
  done_cb_ = std::move(done_cb);
  callbacks_ = callbacks;
  // Use of Unretained(this) is safe, because |done_cb_| guarantees that |this|
  // stays alive.
  device.set_connection_error_handler(
      base::BindOnce(&ServiceVideoCaptureDeviceLauncher::
                         OnConnectionLostWhileWaitingForCallback,
                     base::Unretained(this)));
  device_factory_->CreateDevice(
      device_id, std::move(device_request),
      base::BindOnce(
          // Use of Unretained |this| is safe, because |done_cb_| guarantees
          // that |this| stays alive.
          &ServiceVideoCaptureDeviceLauncher::OnCreateDeviceCallback,
          base::Unretained(this), params, std::move(device),
          std::move(receiver), std::move(connection_lost_cb)));
  state_ = State::DEVICE_START_IN_PROGRESS;
}

void ServiceVideoCaptureDeviceLauncher::AbortLaunch() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (state_ == State::DEVICE_START_IN_PROGRESS)
    state_ = State::DEVICE_START_ABORTING;
}

void ServiceVideoCaptureDeviceLauncher::OnCreateDeviceCallback(
    const media::VideoCaptureParams& params,
    video_capture::mojom::DevicePtr device,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    video_capture::mojom::DeviceAccessResultCode result_code) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(callbacks_);
  DCHECK(done_cb_);
  device.set_connection_error_handler(base::DoNothing());
  const bool abort_requested = (state_ == State::DEVICE_START_ABORTING);
  state_ = State::READY_TO_LAUNCH;
  Callbacks* callbacks = callbacks_;
  callbacks_ = nullptr;
  switch (result_code) {
    case video_capture::mojom::DeviceAccessResultCode::SUCCESS:
      if (abort_requested) {
        device.reset();
        device_factory_.reset();
        callbacks->OnDeviceLaunchAborted();
        base::ResetAndReturn(&done_cb_).Run();
        return;
      }
      ConcludeLaunchDeviceWithSuccess(
          params, std::move(device), std::move(receiver),
          std::move(connection_lost_cb), callbacks, std::move(done_cb_));
      return;
    case video_capture::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:
    case video_capture::mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      ConcludeLaunchDeviceWithFailure(
          abort_requested,
          media::VideoCaptureError::
              kServiceDeviceLauncherServiceRespondedWithDeviceNotFound,
          std::move(device_factory_), callbacks, std::move(done_cb_));
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
      std::move(device_factory_), callbacks, std::move(done_cb_));
}

}  // namespace content
