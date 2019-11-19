// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_frame_handler.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"

namespace video_capture {

MockVideoFrameHandler::MockVideoFrameHandler()
    : video_frame_handler_(this), should_store_access_permissions_(false) {}

MockVideoFrameHandler::MockVideoFrameHandler(
    mojo::PendingReceiver<mojom::VideoFrameHandler> handler)
    : video_frame_handler_(this, std::move(handler)),
      should_store_access_permissions_(false) {}

MockVideoFrameHandler::~MockVideoFrameHandler() = default;

void MockVideoFrameHandler::HoldAccessPermissions() {
  should_store_access_permissions_ = true;
}

void MockVideoFrameHandler::ReleaseAccessPermissions() {
  should_store_access_permissions_ = false;
  access_permissions_.clear();
}

void MockVideoFrameHandler::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  CHECK(!base::Contains(known_buffer_ids_, buffer_id));
  known_buffer_ids_.push_back(buffer_id);
  DoOnNewBuffer(buffer_id, &buffer_handle);
}

void MockVideoFrameHandler::OnFrameReadyInBuffer(
    int32_t buffer_id,
    int32_t frame_feedback_id,
    mojo::PendingRemote<mojom::ScopedAccessPermission> access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DoOnFrameReadyInBuffer(buffer_id, frame_feedback_id, access_permission,
                         &frame_info);
  if (should_store_access_permissions_)
    access_permissions_.emplace_back(std::move(access_permission));
}

void MockVideoFrameHandler::OnBufferRetired(int32_t buffer_id) {
  auto iter =
      std::find(known_buffer_ids_.begin(), known_buffer_ids_.end(), buffer_id);
  CHECK(iter != known_buffer_ids_.end());
  known_buffer_ids_.erase(iter);
  DoOnBufferRetired(buffer_id);
}

}  // namespace video_capture
