// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_source.h"

namespace video_capture {

MockVideoSource::MockVideoSource() = default;

MockVideoSource::~MockVideoSource() = default;

void MockVideoSource::CreatePushSubscription(
    mojo::PendingRemote<video_capture::mojom::VideoFrameHandler> subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    mojo::PendingReceiver<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    CreatePushSubscriptionCallback callback) {
  DoCreatePushSubscription(std::move(subscriber), requested_settings,
                           force_reopen_with_new_settings,
                           std::move(subscription), callback);
}

}  // namespace video_capture
