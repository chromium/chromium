// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device.mojom.h"
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
      BroadcastingReceiver* broadcaster,
      mojo::Remote<mojom::Device>* device);
  ~PushVideoStreamSubscriptionImpl() override;

  void SetOnClosedHandler(
      base::OnceCallback<void(base::OnceClosure done_cb)> handler);

  void OnDeviceStartSucceededWithSettings(
      const media::VideoCaptureParams& settings);
  void OnDeviceStartFailed();

  // mojom::PushVideoStreamSubscription implementation.
  void Activate() override;
  void Suspend(SuspendCallback callback) override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void Close(CloseCallback callback) override;

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
  BroadcastingReceiver* const broadcaster_;
  mojo::Remote<mojom::Device>* const device_;
  Status status_;

  // Client id handed out by |broadcaster_| when registering |this| as its
  // client.
  int32_t broadcaster_client_id_;

  // A callback that we invoke when this instance changes |status_| to
  // kClosed via a call to Close().
  base::OnceCallback<void(base::OnceClosure done_cb)> on_closed_handler_;

  base::WeakPtrFactory<PushVideoStreamSubscriptionImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PushVideoStreamSubscriptionImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUSH_VIDEO_STREAM_SUBSCRIPTION_IMPL_H_
