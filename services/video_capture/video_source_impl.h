// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/broadcasting_receiver.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace video_capture {

class PushVideoStreamSubscriptionImpl;

class VideoSourceImpl : public mojom::VideoSource {
 public:
  VideoSourceImpl(mojom::DeviceFactory* device_factory,
                  const std::string& device_id,
                  base::RepeatingClosure on_last_binding_closed_cb);
  ~VideoSourceImpl() override;

  void AddToReceiverSet(mojo::PendingReceiver<VideoSource> receiver);

  // mojom::VideoSource implementation.
  void CreatePushSubscription(
      mojo::PendingRemote<mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<mojom::PushVideoStreamSubscription> subscription,
      CreatePushSubscriptionCallback callback) override;

 private:
  enum class DeviceStatus {
    kNotStarted,
    kStartingAsynchronously,
    kStarted,
    kStoppingAsynchronously
  };

  void OnClientDisconnected();
  void StartDeviceWithSettings(
      const media::VideoCaptureParams& requested_settings);
  void OnCreateDeviceResponse(mojom::DeviceAccessResultCode result_code);
  void OnPushSubscriptionClosedOrDisconnectedOrDiscarded(
      PushVideoStreamSubscriptionImpl* subscription,
      base::OnceClosure done_cb);
  void StopDeviceAsynchronously();
  void OnStopDeviceComplete();

  mojom::DeviceFactory* const device_factory_;
  const std::string device_id_;
  mojo::ReceiverSet<mojom::VideoSource> receivers_;
  base::RepeatingClosure on_last_binding_closed_cb_;

  // We use the address of each instance as keys to itself.
  std::map<PushVideoStreamSubscriptionImpl*,
           std::unique_ptr<PushVideoStreamSubscriptionImpl>>
      push_subscriptions_;
  BroadcastingReceiver broadcaster_;
  mojo::Receiver<mojom::VideoFrameHandler> broadcaster_video_frame_handler_{
      &broadcaster_};
  DeviceStatus device_status_;
  mojo::Remote<mojom::Device> device_;
  media::VideoCaptureParams device_start_settings_;
  bool restart_device_once_when_stop_complete_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoSourceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoSourceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
