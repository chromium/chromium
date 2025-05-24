// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/push_video_stream_subscription_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "services/video_capture/broadcasting_receiver.h"

namespace video_capture {

PushVideoStreamSubscriptionImpl::PushVideoStreamSubscriptionImpl(
    mojo::PendingReceiver<mojom::PushVideoStreamSubscription>
        subscription_receiver,
    mojo::PendingRemote<mojom::VideoFrameHandler> subscriber,
    const media::VideoCaptureParams& requested_settings,
    mojom::VideoSource::CreatePushSubscriptionCallback creation_callback,
    BroadcastingReceiver* broadcaster)
    : receiver_(this, std::move(subscription_receiver)),
      subscriber_(std::move(subscriber)),
      requested_settings_(requested_settings),
      creation_callback_(std::move(creation_callback)),
      broadcaster_(broadcaster),
      broadcaster_client_id_(0) {
  DCHECK(broadcaster_);
}

PushVideoStreamSubscriptionImpl::~PushVideoStreamSubscriptionImpl() = default;

void PushVideoStreamSubscriptionImpl::SetOnClosedHandler(
    base::OnceCallback<void(base::OnceClosure done_cb)> handler) {
  on_closed_handler_ = std::move(handler);
  receiver_.set_disconnect_handler(
      base::BindOnce(&PushVideoStreamSubscriptionImpl::OnConnectionLost,
                     weak_factory_.GetWeakPtr()));
}

void PushVideoStreamSubscriptionImpl::OnDeviceStartSucceededWithSettings(
    const media::VideoCaptureParams& settings,
    Device* device) {
  device_ = device;
  if (status_ != Status::kCreationCallbackNotYetRun) {
    // Creation callback has already been run from a previous device start.
    return;
  }
  mojom::CreatePushSubscriptionSuccessCode success_code =
      settings == requested_settings_
          ? mojom::CreatePushSubscriptionSuccessCode::
                kCreatedWithRequestedSettings
          : mojom::CreatePushSubscriptionSuccessCode::
                kCreatedWithDifferentSettings;
  std::move(creation_callback_)
      .Run(
          mojom::CreatePushSubscriptionResultCode::NewSuccessCode(success_code),
          settings);
  status_ = Status::kNotYetActivated;
}

void PushVideoStreamSubscriptionImpl::OnDeviceStartFailed(
    media::VideoCaptureError error) {
  DCHECK_NE(error, media::VideoCaptureError::kNone);

  if (status_ != Status::kCreationCallbackNotYetRun) {
    // Creation callback has already been run from a previous device start.
    return;
  }
  std::move(creation_callback_)
      .Run(mojom::CreatePushSubscriptionResultCode::NewErrorCode(error),
           requested_settings_);
  status_ = Status::kClosed;
}

void PushVideoStreamSubscriptionImpl::Activate() {
  if (status_ != Status::kNotYetActivated)
    return;
  broadcaster_client_id_ = broadcaster_->AddClient(
      std::move(subscriber_), requested_settings_.buffer_type);
  status_ = Status::kActive;
}

void PushVideoStreamSubscriptionImpl::Suspend(SuspendCallback callback) {
  if (status_ != Status::kActive) {
    std::move(callback).Run();
    return;
  }
  broadcaster_->SuspendClient(broadcaster_client_id_);
  status_ = Status::kSuspended;
  std::move(callback).Run();
}

void PushVideoStreamSubscriptionImpl::Resume() {
  if (status_ != Status::kSuspended)
    return;
  broadcaster_->ResumeClient(broadcaster_client_id_);
  status_ = Status::kActive;
}

void PushVideoStreamSubscriptionImpl::GetPhotoState(
    GetPhotoStateCallback callback) {
  switch (status_) {
    case Status::kCreationCallbackNotYetRun:  // Fall through.
    case Status::kClosed:
      // Ignore the call.
      return;
    case Status::kNotYetActivated:  // Fall through.
    case Status::kActive:           // Fall through.
    case Status::kSuspended:
      device_->GetPhotoState(std::move(callback));
      return;
  }
}

void PushVideoStreamSubscriptionImpl::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  switch (status_) {
    case Status::kCreationCallbackNotYetRun:  // Fall through.
    case Status::kClosed:
      // Ignore the call.
      return;
    case Status::kNotYetActivated:  // Fall through.
    case Status::kActive:           // Fall through.
    case Status::kSuspended:
      device_->SetPhotoOptions(std::move(settings), std::move(callback));
      return;
  }
}

void PushVideoStreamSubscriptionImpl::TakePhoto(TakePhotoCallback callback) {
  switch (status_) {
    case Status::kCreationCallbackNotYetRun:  // Fall through.
    case Status::kClosed:
      // Ignore the call.
      return;
    case Status::kNotYetActivated:  // Fall through.
    case Status::kActive:           // Fall through.
    case Status::kSuspended:
      device_->TakePhoto(std::move(callback));
      return;
  }
}

void PushVideoStreamSubscriptionImpl::Close(CloseCallback callback) {
  switch (status_) {
    case Status::kCreationCallbackNotYetRun:
    case Status::kClosed:
      std::move(callback).Run();
      return;
    case Status::kActive:  // Fall through.
    case Status::kSuspended:
      broadcaster_->RemoveClient(broadcaster_client_id_);
      status_ = Status::kClosed;
      if (on_closed_handler_)
        std::move(on_closed_handler_).Run(std::move(callback));
      return;
    case Status::kNotYetActivated:
      status_ = Status::kClosed;
      if (on_closed_handler_)
        std::move(on_closed_handler_).Run(std::move(callback));
      return;
  }
}

void PushVideoStreamSubscriptionImpl::OnConnectionLost() {
  if (on_closed_handler_)
    std::move(on_closed_handler_).Run(base::DoNothing());
}

void PushVideoStreamSubscriptionImpl::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  switch (status_) {
    case Status::kCreationCallbackNotYetRun:  // Fall through.
    case Status::kClosed:
      // Ignore the call.
      return;
    case Status::kNotYetActivated:  // Fall through.
    case Status::kActive:           // Fall through.
    case Status::kSuspended:
      device_->ProcessFeedback(feedback);
      return;
  }
}

}  // namespace video_capture
