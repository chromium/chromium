// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "services/video_capture/push_video_stream_subscription_impl.h"

namespace video_capture {

VideoSourceImpl::VideoSourceImpl(
    mojom::DeviceFactory* device_factory,
    const std::string& device_id,
    base::RepeatingClosure on_last_binding_closed_cb)
    : device_factory_(device_factory),
      device_id_(device_id),
      on_last_binding_closed_cb_(std::move(on_last_binding_closed_cb)),
      device_status_(DeviceStatus::kNotStarted),
      restart_device_once_when_stop_complete_(false) {
  // Unretained(this) is safe because |this| owns |receivers_|.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &VideoSourceImpl::OnClientDisconnected, base::Unretained(this)));
}

VideoSourceImpl::~VideoSourceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.set_disconnect_handler(base::DoNothing());
}

void VideoSourceImpl::AddToReceiverSet(
    mojo::PendingReceiver<VideoSource> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void VideoSourceImpl::CreatePushSubscription(
    mojo::PendingRemote<mojom::VideoFrameHandler> subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    mojo::PendingReceiver<mojom::PushVideoStreamSubscription>
        subscription_receiver,
    CreatePushSubscriptionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto subscription = std::make_unique<PushVideoStreamSubscriptionImpl>(
      std::move(subscription_receiver), std::move(subscriber),
      requested_settings, std::move(callback), &broadcaster_, &device_);
  subscription->SetOnClosedHandler(base::BindOnce(
      &VideoSourceImpl::OnPushSubscriptionClosedOrDisconnectedOrDiscarded,
      weak_factory_.GetWeakPtr(), subscription.get()));
  auto* subscription_ptr = subscription.get();
  push_subscriptions_.insert(
      std::make_pair(subscription.get(), std::move(subscription)));
  switch (device_status_) {
    case DeviceStatus::kNotStarted:
      StartDeviceWithSettings(requested_settings);
      return;
    case DeviceStatus::kStartingAsynchronously:
      if (force_reopen_with_new_settings)
        device_start_settings_ = requested_settings;
      // No need to do anything else. Response will be sent when
      // OnCreateDeviceResponse() gets called.
      return;
    case DeviceStatus::kStarted:
      if (!force_reopen_with_new_settings ||
          requested_settings == device_start_settings_) {
        subscription_ptr->OnDeviceStartSucceededWithSettings(
            device_start_settings_);
        return;
      }
      restart_device_once_when_stop_complete_ = true;
      device_start_settings_ = requested_settings;
      StopDeviceAsynchronously();
      return;
    case DeviceStatus::kStoppingAsynchronously:
      restart_device_once_when_stop_complete_ = true;
      device_start_settings_ = requested_settings;
      return;
  }
}

void VideoSourceImpl::OnClientDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (receivers_.empty()) {
    // Note: Invoking this callback may synchronously trigger the destruction of
    // |this|, so no more member access should be done after it.
    on_last_binding_closed_cb_.Run();
  }
}

void VideoSourceImpl::StartDeviceWithSettings(
    const media::VideoCaptureParams& requested_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_start_settings_ = requested_settings;
  device_status_ = DeviceStatus::kStartingAsynchronously;
  device_factory_->CreateDevice(
      device_id_, device_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&VideoSourceImpl::OnCreateDeviceResponse,
                     weak_factory_.GetWeakPtr()));
}

void VideoSourceImpl::OnCreateDeviceResponse(
    mojom::DeviceAccessResultCode result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (result_code) {
    case mojom::DeviceAccessResultCode::SUCCESS: {
      broadcaster_video_frame_handler_.reset();
      device_->Start(
          device_start_settings_,
          broadcaster_video_frame_handler_.BindNewPipeAndPassRemote());
      device_status_ = DeviceStatus::kStarted;
      if (push_subscriptions_.empty()) {
        StopDeviceAsynchronously();
        return;
      }
      for (auto& entry : push_subscriptions_) {
        auto& subscription = entry.second;
        subscription->OnDeviceStartSucceededWithSettings(
            device_start_settings_);
      }
      return;
    }
    case mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:  // Fall through
    case mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      for (auto& entry : push_subscriptions_) {
        auto& subscription = entry.second;
        subscription->OnDeviceStartFailed();
      }
      push_subscriptions_.clear();
      device_status_ = DeviceStatus::kNotStarted;
      return;
  }
}

void VideoSourceImpl::OnPushSubscriptionClosedOrDisconnectedOrDiscarded(
    PushVideoStreamSubscriptionImpl* subscription,
    base::OnceClosure done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We keep the subscription instance alive until after having called |done_cb|
  // in order to allow it to send out a callback before being destroyed.
  auto subscription_ownership = std::move(push_subscriptions_[subscription]);
  push_subscriptions_.erase(subscription);
  if (push_subscriptions_.empty()) {
    switch (device_status_) {
      case DeviceStatus::kNotStarted:
        // Nothing to do here.
        break;
      case DeviceStatus::kStartingAsynchronously:
        // We will check again in OnCreateDeviceResponse() whether or not there
        // are any subscriptions.
        break;
      case DeviceStatus::kStarted:
        StopDeviceAsynchronously();
        break;
      case DeviceStatus::kStoppingAsynchronously:
        // Nothing to do here.
        break;
    }
  }
  std::move(done_cb).Run();
}

void VideoSourceImpl::StopDeviceAsynchronously() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (restart_device_once_when_stop_complete_) {
    // We do not want to send out OnStopped() or OnStarted() to already
    // connected clients, to make this internal restart transparent to them.
    // The broadcaster already drops additional OnStarted() events for clients
    // who already received one. But for OnStopped() we need to explicitly tell
    // it to.
    // Unretained(this) is safe because |this| owns |broadcaster_|.
    broadcaster_.HideSourceRestartFromClients(base::BindOnce(
        &VideoSourceImpl::OnStopDeviceComplete, base::Unretained(this)));
  } else {
    broadcaster_.SetOnStoppedHandler(base::BindOnce(
        &VideoSourceImpl::OnStopDeviceComplete, base::Unretained(this)));
  }

  // Stop the device by closing the connection to it. Stopping is complete when
  // OnStopDeviceComplete() gets invoked.
  device_.reset();
  device_status_ = DeviceStatus::kStoppingAsynchronously;
}

void VideoSourceImpl::OnStopDeviceComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_status_ = DeviceStatus::kNotStarted;
  if (!restart_device_once_when_stop_complete_)
    return;
  restart_device_once_when_stop_complete_ = false;
  StartDeviceWithSettings(device_start_settings_);
}

}  // namespace video_capture
