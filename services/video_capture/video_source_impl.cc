// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "media/capture/video/video_capture_device_client.h"
#include "services/video_capture/push_video_stream_subscription_impl.h"

namespace video_capture {

VideoSourceImpl::VideoSourceImpl(
    DeviceFactory* device_factory,
    const std::string& device_id,
    base::RepeatingClosure on_last_binding_closed_cb)
    : device_factory_(device_factory),
      device_id_(device_id),
      on_last_binding_closed_cb_(std::move(on_last_binding_closed_cb)),
      device_status_(DeviceStatus::kNotStarted) {
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
  if (device_status_ == DeviceStatus::kNotStarted) {
    device_startup_start_time_ = base::TimeTicks::Now();
  }

  auto subscription = std::make_unique<PushVideoStreamSubscriptionImpl>(
      std::move(subscription_receiver), std::move(subscriber),
      requested_settings, std::move(callback), &broadcaster_);
  auto* subscription_ptr = subscription.get();
  subscription->SetOnClosedHandler(base::BindOnce(
      &VideoSourceImpl::OnPushSubscriptionClosedOrDisconnectedOrDiscarded,
      weak_factory_.GetWeakPtr(), subscription_ptr));
  push_subscriptions_.insert(
      std::make_pair(subscription_ptr, std::move(subscription)));
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
      CHECK(device_);
      if (!force_reopen_with_new_settings ||
          requested_settings == device_start_settings_) {
        subscription_ptr->OnDeviceStartSucceededWithSettings(
            device_start_settings_, device_);
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

void VideoSourceImpl::RegisterVideoEffectsProcessor(
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor> remote) {
  pending_video_effects_processor_ = std::move(remote);
}

void VideoSourceImpl::OnClientDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receivers_.empty()) {
    return;
  }

  if (device_status_ != DeviceStatus::kStoppingAsynchronously &&
      device_status_ != DeviceStatus::kNotStarted) {
    // We need to stop devices when VideoSource remote discarded with active
    // subscription.
    // DeviceStatus::kNotStarted means no device has been created yet or
    // no device has been successfully created. Therefore, StopDevice() does
    // not need to be called in these two cases.
    device_factory_->StopDevice(device_id_);
  }

  // Note: Invoking this callback may synchronously trigger the destruction of
  // |this|, so no more member access should be done after it.
  on_last_binding_closed_cb_.Run();
}

void VideoSourceImpl::StartDeviceWithSettings(
    const media::VideoCaptureParams& requested_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto scoped_trace = ScopedCaptureTrace::CreateIfEnabled(
      "VideoSourceImpl::StartDeviceWithSettings");
  if (scoped_trace)
    scoped_trace->AddStep("CreateDevice");

  device_start_settings_ = requested_settings;
  device_status_ = DeviceStatus::kStartingAsynchronously;
  device_factory_->CreateDevice(
      device_id_,
      base::BindOnce(&VideoSourceImpl::OnCreateDeviceResponse,
                     weak_factory_.GetWeakPtr(), std::move(scoped_trace)));
}

void VideoSourceImpl::OnCreateDeviceResponse(
    std::unique_ptr<ScopedCaptureTrace> scoped_trace,
    DeviceInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!device_);

  if (info.result_code == media::VideoCaptureError::kNone) {
    UmaHistogramTimes("Media.VideoCapture.CreateDeviceSuccessLatency",
                      base::TimeTicks::Now() - device_startup_start_time_);
    device_ = info.device;

    if (scoped_trace)
      scoped_trace->AddStep("StartDevice");

    // Device was created successfully.
    info.device->StartInProcess(
        device_start_settings_, broadcaster_.GetWeakPtr(),
        media::VideoEffectsContext(
            std::move(pending_video_effects_processor_)));
    UmaHistogramTimes("Media.VideoCapture.StartSourceSuccessLatency",
                      base::TimeTicks::Now() - device_startup_start_time_);
    device_status_ = DeviceStatus::kStarted;
    if (push_subscriptions_.empty()) {
      StopDeviceAsynchronously();
      return;
    }
    for (auto& entry : push_subscriptions_) {
      auto& subscription = entry.second;
      subscription->OnDeviceStartSucceededWithSettings(device_start_settings_,
                                                       device_);
    }
    return;
  }
  for (auto& entry : push_subscriptions_) {
    auto& subscription = entry.second;
    subscription->OnDeviceStartFailed(info.result_code);
  }
  push_subscriptions_.clear();
  device_status_ = DeviceStatus::kNotStarted;
  return;
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
  device_factory_->StopDevice(device_id_);
  device_status_ = DeviceStatus::kStoppingAsynchronously;
}

void VideoSourceImpl::OnStopDeviceComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_status_ = DeviceStatus::kNotStarted;
  device_ = nullptr;
  if (!restart_device_once_when_stop_complete_)
    return;
  restart_device_once_when_stop_complete_ = false;
  StartDeviceWithSettings(device_start_settings_);
}

}  // namespace video_capture
