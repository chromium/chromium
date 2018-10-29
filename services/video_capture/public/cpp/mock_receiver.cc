// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_receiver.h"

namespace video_capture {

MockReceiver::MockReceiver() : binding_(this) {}

MockReceiver::MockReceiver(mojom::ReceiverRequest request)
    : binding_(this, std::move(request)) {}

MockReceiver::~MockReceiver() = default;

void MockReceiver::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DoOnNewBuffer(buffer_id, &buffer_handle);
}

void MockReceiver::OnFrameReadyInBuffer(
    int32_t buffer_id,
    int32_t frame_feedback_id,
    mojom::ScopedAccessPermissionPtr access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DoOnFrameReadyInBuffer(buffer_id, frame_feedback_id, &access_permission,
                         &frame_info);
}

}  // namespace video_capture
