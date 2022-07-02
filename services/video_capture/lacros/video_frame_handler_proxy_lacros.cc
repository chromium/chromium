// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"

#include <map>
#include <memory>
#include <utility>

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/lacros/video_buffer_adapters.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"

namespace video_capture {

namespace {
mojom::ReadyFrameInBufferPtr ToVideoCaptureBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer) {
  auto video_capture_buffer = mojom::ReadyFrameInBuffer::New();
  video_capture_buffer->buffer_id = buffer->buffer_id;
  video_capture_buffer->frame_feedback_id = buffer->frame_feedback_id;

  video_capture_buffer->frame_info =
      ConvertToMediaVideoFrameInfo(std::move(buffer->frame_info));

  return video_capture_buffer;
}
}  // namespace

// A reference counted map keeping
// mojo::Remote<crosapi::mojom::ScopedAccessPermission> pipes alive until
// EraseAccessPermission() calls.
class VideoFrameHandlerProxyLacros::AccessPermissionProxyMap
    : public base::RefCountedThreadSafe<
          VideoFrameHandlerProxyLacros::AccessPermissionProxyMap> {
 public:
  AccessPermissionProxyMap() = default;

  void InsertAccessPermission(
      int32_t buffer_id,
      mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission>
          pending_remote_access_permission) {
    std::unique_ptr<mojo::Remote<crosapi::mojom::ScopedAccessPermission>>
        remote_access_permission = std::make_unique<
            mojo::Remote<crosapi::mojom::ScopedAccessPermission>>(
            std::move(pending_remote_access_permission));
    auto result = access_permissions_by_buffer_ids_.insert(
        std::make_pair(buffer_id, std::move(remote_access_permission)));
    DCHECK(result.second);
  }

  void EraseAccessPermission(int32_t buffer_id) {
    auto it = access_permissions_by_buffer_ids_.find(buffer_id);
    if (it == access_permissions_by_buffer_ids_.end()) {
      NOTREACHED();
      return;
    }
    access_permissions_by_buffer_ids_.erase(it);
  }

 private:
  friend class base::RefCountedThreadSafe<
      VideoFrameHandlerProxyLacros::AccessPermissionProxyMap>;
  ~AccessPermissionProxyMap() = default;

  std::map<
      int32_t,
      std::unique_ptr<mojo::Remote<crosapi::mojom::ScopedAccessPermission>>>
      access_permissions_by_buffer_ids_;
};

// mojom::VideoFrameAccessHandler implementation that takes care of erasing the
// mapped scoped access permissions.
class VideoFrameHandlerProxyLacros::VideoFrameAccessHandlerProxy
    : public mojom::VideoFrameAccessHandler {
 public:
  VideoFrameAccessHandlerProxy(
      scoped_refptr<AccessPermissionProxyMap> access_permission_proxy_map)
      : access_permission_proxy_map_(std::move(access_permission_proxy_map)) {}
  ~VideoFrameAccessHandlerProxy() override = default;

  void OnFinishedConsumingBuffer(int32_t buffer_id) override {
    access_permission_proxy_map_->EraseAccessPermission(buffer_id);
  }

 private:
  scoped_refptr<AccessPermissionProxyMap> access_permission_proxy_map_;
};

VideoFrameHandlerProxyLacros::VideoFrameHandlerProxyLacros(
    mojo::PendingReceiver<crosapi::mojom::VideoFrameHandler> proxy_receiver,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote)
    : handler_(std::move(handler_remote)) {
  receiver_.Bind(std::move(proxy_receiver));
}

VideoFrameHandlerProxyLacros::~VideoFrameHandlerProxyLacros() = default;

void VideoFrameHandlerProxyLacros::OnNewBuffer(
    int buffer_id,
    crosapi::mojom::VideoBufferHandlePtr buffer_handle) {
  handler_->OnNewBuffer(buffer_id,
                        ConvertToMediaVideoBuffer(std::move(buffer_handle)));
}

void VideoFrameHandlerProxyLacros::OnFrameReadyInBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<crosapi::mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  if (!access_permission_proxy_map_) {
    access_permission_proxy_map_ = new AccessPermissionProxyMap();
    mojo::PendingRemote<mojom::VideoFrameAccessHandler> pending_remote;
    mojo::MakeSelfOwnedReceiver<mojom::VideoFrameAccessHandler>(
        std::make_unique<VideoFrameAccessHandlerProxy>(
            access_permission_proxy_map_),
        pending_remote.InitWithNewPipeAndPassReceiver());
    handler_->OnFrameAccessHandlerReady(std::move(pending_remote));
  }

  access_permission_proxy_map_->InsertAccessPermission(
      buffer->buffer_id, std::move(buffer->access_permission));
  mojom::ReadyFrameInBufferPtr video_capture_buffer =
      ToVideoCaptureBuffer(std::move(buffer));
  std::vector<mojom::ReadyFrameInBufferPtr> video_capture_scaled_buffers;
  for (auto& b : scaled_buffers) {
    access_permission_proxy_map_->InsertAccessPermission(
        b->buffer_id, std::move(b->access_permission));
    video_capture_scaled_buffers.push_back(ToVideoCaptureBuffer(std::move(b)));
  }

  handler_->OnFrameReadyInBuffer(std::move(video_capture_buffer),
                                 std::move(video_capture_scaled_buffers));
}

void VideoFrameHandlerProxyLacros::OnBufferRetired(int buffer_id) {
  handler_->OnBufferRetired(buffer_id);
}

void VideoFrameHandlerProxyLacros::OnError(media::VideoCaptureError error) {
  handler_->OnError(error);
}

void VideoFrameHandlerProxyLacros::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  handler_->OnFrameDropped(reason);
}

void VideoFrameHandlerProxyLacros::OnNewCropVersion(uint32_t crop_version) {
  handler_->OnNewCropVersion(crop_version);
}

void VideoFrameHandlerProxyLacros::OnFrameWithEmptyRegionCapture() {
  handler_->OnFrameWithEmptyRegionCapture();
}

void VideoFrameHandlerProxyLacros::OnLog(const std::string& message) {
  handler_->OnLog(message);
}

void VideoFrameHandlerProxyLacros::OnStarted() {
  handler_->OnStarted();
}

void VideoFrameHandlerProxyLacros::OnStartedUsingGpuDecode() {
  handler_->OnStartedUsingGpuDecode();
}

void VideoFrameHandlerProxyLacros::OnStopped() {
  handler_->OnStopped();
}

}  // namespace video_capture
