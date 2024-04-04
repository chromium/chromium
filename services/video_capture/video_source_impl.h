// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/scoped_async_trace.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/broadcasting_receiver.h"
#include "services/video_capture/device_factory_impl.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"

namespace video_capture {

class PushVideoStreamSubscriptionImpl;

class VideoSourceImpl : public mojom::VideoSource {
 public:
  VideoSourceImpl(DeviceFactory* device_factory,
                  const std::string& device_id,
                  base::RepeatingClosure on_last_binding_closed_cb);

  VideoSourceImpl(const VideoSourceImpl&) = delete;
  VideoSourceImpl& operator=(const VideoSourceImpl&) = delete;

  ~VideoSourceImpl() override;

  void AddToReceiverSet(mojo::PendingReceiver<VideoSource> receiver);

  // mojom::VideoSource implementation.
  void CreatePushSubscription(
      mojo::PendingRemote<mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<mojom::PushVideoStreamSubscription> subscription,
      CreatePushSubscriptionCallback callback) override;

  void RegisterVideoEffectsProcessor(
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor> remote)
      override;

 private:
  enum class DeviceStatus {
    kNotStarted,
    kStartingAsynchronously,
    kStarted,
    kStoppingAsynchronously
  };
  using DeviceInfo = DeviceFactory::DeviceInfo;

  using ScopedCaptureTrace =
      media::TypedScopedAsyncTrace<media::TraceCategory::kVideoAndImageCapture>;

  void OnClientDisconnected();
  void StartDeviceWithSettings(
      const media::VideoCaptureParams& requested_settings);
  void OnCreateDeviceResponse(std::unique_ptr<ScopedCaptureTrace> scoped_trace,
                              DeviceInfo info);
  void OnPushSubscriptionClosedOrDisconnectedOrDiscarded(
      PushVideoStreamSubscriptionImpl* subscription,
      base::OnceClosure done_cb);
  void StopDeviceAsynchronously();
  void OnStopDeviceComplete();

  const raw_ptr<DeviceFactory, DanglingUntriaged> device_factory_;
  const std::string device_id_;
  mojo::ReceiverSet<mojom::VideoSource> receivers_;
  base::RepeatingClosure on_last_binding_closed_cb_;

  // We use the address of each instance as keys to itself.
  std::map<PushVideoStreamSubscriptionImpl*,
           std::unique_ptr<PushVideoStreamSubscriptionImpl>>
      push_subscriptions_;
  BroadcastingReceiver broadcaster_;
  DeviceStatus device_status_;
  raw_ptr<Device, AcrossTasksDanglingUntriaged> device_{nullptr};
  media::VideoCaptureParams device_start_settings_;
  bool restart_device_once_when_stop_complete_ = false;

  // Video effects processor that will be used to start the capture on the
  // `device_`.
  mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
      pending_video_effects_processor_;

  base::TimeTicks device_startup_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoSourceImpl> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_IMPL_H_
