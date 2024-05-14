// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/video_frame_access_handler.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace video_capture {

// static
scoped_refptr<ScopedAccessPermissionMap>
ScopedAccessPermissionMap::CreateMapAndSendVideoFrameAccessHandlerReady(
    mojo::Remote<mojom::VideoFrameHandler>& video_frame_handler) {
  scoped_refptr<ScopedAccessPermissionMap> map =
      new ScopedAccessPermissionMap();
  mojo::PendingRemote<mojom::VideoFrameAccessHandler> pending_remote;
  mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
      std::make_unique<VideoFrameAccessHandlerImpl>(map),
      pending_remote.InitWithNewPipeAndPassReceiver());
  video_frame_handler->OnFrameAccessHandlerReady(std::move(pending_remote));
  return map;
}

ScopedAccessPermissionMap::ScopedAccessPermissionMap() = default;

ScopedAccessPermissionMap::~ScopedAccessPermissionMap() = default;

void ScopedAccessPermissionMap::InsertAccessPermission(
    int32_t buffer_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        scoped_access_permission) {
  auto result = scoped_access_permissions_by_buffer_id_.insert(
      std::make_pair(buffer_id, std::move(scoped_access_permission)));
  DCHECK(result.second);
}

void ScopedAccessPermissionMap::EraseAccessPermission(int32_t buffer_id) {
  auto it = scoped_access_permissions_by_buffer_id_.find(buffer_id);
  if (it == scoped_access_permissions_by_buffer_id_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  scoped_access_permissions_by_buffer_id_.erase(it);
}

VideoFrameAccessHandlerImpl::VideoFrameAccessHandlerImpl(
    scoped_refptr<ScopedAccessPermissionMap> scoped_access_permission_map)
    : scoped_access_permission_map_(std::move(scoped_access_permission_map)) {}

VideoFrameAccessHandlerImpl::~VideoFrameAccessHandlerImpl() = default;

void VideoFrameAccessHandlerImpl::OnFinishedConsumingBuffer(int32_t buffer_id) {
  scoped_access_permission_map_->EraseAccessPermission(buffer_id);
}

VideoFrameAccessHandlerRemote::VideoFrameAccessHandlerRemote(
    mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>
        frame_access_handler)
    : frame_access_handler_(std::move(frame_access_handler)) {}

VideoFrameAccessHandlerRemote::~VideoFrameAccessHandlerRemote() = default;

mojom::VideoFrameAccessHandler* VideoFrameAccessHandlerRemote::operator->() {
  return frame_access_handler_.get();
}

// static
void VideoFrameAccessHandlerForwarder::
    CreateForwarderAndSendVideoFrameAccessHandlerReady(
        mojo::Remote<mojom::VideoFrameHandler>& video_frame_handler,
        scoped_refptr<VideoFrameAccessHandlerRemote>
            video_frame_handler_remote) {
  mojo::PendingRemote<mojom::VideoFrameAccessHandler> pending_remote;
  mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
      std::make_unique<VideoFrameAccessHandlerForwarder>(
          std::move(video_frame_handler_remote)),
      pending_remote.InitWithNewPipeAndPassReceiver());
  video_frame_handler->OnFrameAccessHandlerReady(std::move(pending_remote));
}

VideoFrameAccessHandlerForwarder::VideoFrameAccessHandlerForwarder(
    scoped_refptr<VideoFrameAccessHandlerRemote> video_frame_handler_remote)
    : video_frame_handler_remote_(std::move(video_frame_handler_remote)) {}

VideoFrameAccessHandlerForwarder::~VideoFrameAccessHandlerForwarder() = default;

void VideoFrameAccessHandlerForwarder::OnFinishedConsumingBuffer(
    int32_t buffer_id) {
  (*video_frame_handler_remote_)->OnFinishedConsumingBuffer(buffer_id);
}

}  // namespace video_capture
