// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

namespace video_capture {

class BroadcastingReceiver;

class PushVideoStreamSubscriptionImpl
    : public mojom::PushVideoStreamSubscription {
 public:
  PushVideoStreamSubscriptionImpl(
      mojo::PendingReceiver<mojom::PushVideoStreamSubscription>
          subscription_receiver,
      mojo::PendingRemote<mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      mojom::VideoSource::CreatePushSubscriptionCallback creation_callback,
      BroadcastingReceiver* broadcaster);

  PushVideoStreamSubscriptionImpl(const PushVideoStreamSubscriptionImpl&) =
      delete;
  PushVideoStreamSubscriptionImpl& operator=(
      const PushVideoStreamSubscriptionImpl&) = delete;

  ~PushVideoStreamSubscriptionImpl() override;

  void SetOnClosedHandler(
      base::OnceCallback<void(base::OnceClosure done_cb)> handler);

  void OnDeviceStartSucceededWithSettings(
      const media::VideoCaptureParams& settings,
      Device* device);
  void OnDeviceStartFailed(media::VideoCaptureError error);

  // mojom::PushVideoStreamSubscription implementation.
  void Activate() override;
  void Suspend(SuspendCallback callback) override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void Close(CloseCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;

 private:
  enum class Status {
    kCreationCallbackNotYetRun,
    kNotYetActivated,
    kActive,
    kSuspended,
    kClosed
  };

  void OnConnectionLost();

  mojo::Receiver<mojom::PushVideoStreamSubscription> receiver_;
  mojo::PendingRemote<mojom::VideoFrameHandler> subscriber_;
  const media::VideoCaptureParams requested_settings_;
  mojom::VideoSource::CreatePushSubscriptionCallback creation_callback_;
  const raw_ptr<BroadcastingReceiver> broadcaster_;
  raw_ptr<Device, AcrossTasksDanglingUntriaged> device_;
  Status status_{Status::kCreationCallbackNotYetRun};

  // Client id handed out by |broadcaster_| when registering |this| as its
  // client.
  int32_t broadcaster_client_id_;

  // A callback that we invoke when this instance changes |status_| to
  // kClosed via a call to Close().
  base::OnceCallback<void(base::OnceClosure done_cb)> on_closed_handler_;

  base::WeakPtrFactory<PushVideoStreamSubscriptionImpl> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_
