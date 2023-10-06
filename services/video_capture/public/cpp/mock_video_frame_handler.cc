// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_frame_handler.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

using testing::_;

namespace video_capture {

MockVideoFrameHandler::MockVideoFrameHandler(
    mojo::PendingReceiver<mojom::VideoFrameHandler> handler)
    : video_frame_handler_(this, std::move(handler)),
      should_store_access_permissions_(false) {}

MockVideoFrameHandler::~MockVideoFrameHandler() = default;

void MockVideoFrameHandler::HoldAccessPermissions() {
  should_store_access_permissions_ = true;
}

void MockVideoFrameHandler::ReleaseAccessedFrames() {
  should_store_access_permissions_ = false;
  for (int32_t buffer_id : accessed_frame_ids_) {
    frame_access_handler_->OnFinishedConsumingBuffer(buffer_id);
  }
  accessed_frame_ids_.clear();
}

void MockVideoFrameHandler::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  CHECK(!base::Contains(known_buffer_ids_, buffer_id));
  known_buffer_ids_.push_back(buffer_id);
  DoOnNewBuffer(buffer_id, &buffer_handle);
}

void MockVideoFrameHandler::OnFrameAccessHandlerReady(
    mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
        pending_frame_access_handler) {
  // The MockVideoFrameHandler should take care of frame access.
  frame_access_handler_.Bind(std::move(pending_frame_access_handler));
}

void MockVideoFrameHandler::OnFrameReadyInBuffer(
    mojom::ReadyFrameInBufferPtr buffer) {
  accessed_frame_ids_.push_back(buffer->buffer_id);
  DoOnFrameReadyInBuffer(buffer->buffer_id, buffer->frame_feedback_id,
                         &buffer->frame_info);
  if (!should_store_access_permissions_) {
    ReleaseAccessedFrames();
  }
}

void MockVideoFrameHandler::OnBufferRetired(int32_t buffer_id) {
  auto iter = base::ranges::find(known_buffer_ids_, buffer_id);
  CHECK(iter != known_buffer_ids_.end());
  known_buffer_ids_.erase(iter);
  DoOnBufferRetired(buffer_id);
}

FakeVideoFrameAccessHandler::FakeVideoFrameAccessHandler()
    : FakeVideoFrameAccessHandler(base::BindRepeating([](int32_t) {})) {}

FakeVideoFrameAccessHandler::FakeVideoFrameAccessHandler(
    base::RepeatingCallback<void(int32_t)> callback)
    : callback_(std::move(callback)) {}

FakeVideoFrameAccessHandler::~FakeVideoFrameAccessHandler() = default;

const std::vector<int32_t>& FakeVideoFrameAccessHandler::released_buffer_ids()
    const {
  return released_buffer_ids_;
}

void FakeVideoFrameAccessHandler::ClearReleasedBufferIds() {
  released_buffer_ids_.clear();
}

void FakeVideoFrameAccessHandler::OnFinishedConsumingBuffer(int32_t buffer_id) {
  released_buffer_ids_.push_back(buffer_id);
  callback_.Run(buffer_id);
}

}  // namespace video_capture
