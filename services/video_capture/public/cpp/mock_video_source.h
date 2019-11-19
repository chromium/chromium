// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoSource : public video_capture::mojom::VideoSource {
 public:
  MockVideoSource();
  ~MockVideoSource() override;

  void CreatePushSubscription(
      mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
      const media::VideoCaptureParams& requested_settings,
      bool force_reopen_with_new_settings,
      mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
          subscription,
      CreatePushSubscriptionCallback callback) override;

  MOCK_METHOD5(
      DoCreatePushSubscription,
      void(mojo::PendingRemote<video_capture::mojom::VideoFrameHandler>
               subscriber,
           const media::VideoCaptureParams& requested_settings,
           bool force_reopen_with_new_settings,
           mojo::PendingReceiver<
               video_capture::mojom::PushVideoStreamSubscription> subscription,
           CreatePushSubscriptionCallback& callback));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_H_
