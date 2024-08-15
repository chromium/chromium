// Copyright 2021 The Chromium Authors
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
      NOTREACHED_IN_MIGRATION();
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
    std::optional<mojo::PendingRemote<mojom::VideoFrameHandler>> handler_remote,
    base::WeakPtr<media::VideoFrameReceiver> handler_remote_in_process) {
  CHECK(handler_remote || handler_remote_in_process);
  receiver_.Bind(std::move(proxy_receiver));
  if (handler_remote)
    handler_.Bind(std::move(*handler_remote));
  else if (handler_remote_in_process)
    handler_in_process_ = handler_remote_in_process;
}

VideoFrameHandlerProxyLacros::~VideoFrameHandlerProxyLacros() = default;

void VideoFrameHandlerProxyLacros::OnCaptureConfigurationChanged() {
  if (handler_.is_bound()) {
    handler_->OnCaptureConfigurationChanged();
  } else if (handler_in_process_) {
    handler_in_process_->OnCaptureConfigurationChanged();
  }
}

void VideoFrameHandlerProxyLacros::OnNewBuffer(
    int buffer_id,
    crosapi::mojom::VideoBufferHandlePtr buffer_handle) {
  if (handler_.is_bound()) {
    handler_->OnNewBuffer(buffer_id,
                          ConvertToMediaVideoBuffer(std::move(buffer_handle)));
  } else if (handler_in_process_) {
    handler_in_process_->OnNewBuffer(
        buffer_id, ConvertToMediaVideoBuffer(std::move(buffer_handle)));
  }
}

void VideoFrameHandlerProxyLacros::DEPRECATED_OnFrameReadyInBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<crosapi::mojom::ReadyFrameInBufferPtr> /*scaled_buffers*/) {
  NOTREACHED()
      << "This method is deprecated, use OnFrameReadyInBuffer instead.";
}

void VideoFrameHandlerProxyLacros::OnFrameReadyInBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer) {
  if (handler_.is_bound()) {
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

    handler_->OnFrameReadyInBuffer(std::move(video_capture_buffer));
  } else if (handler_in_process_) {
    handler_in_process_->OnFrameReadyInBuffer(
        ConvertToMediaReadyFrame(std::move(buffer)));
  }
}

void VideoFrameHandlerProxyLacros::OnBufferRetired(int buffer_id) {
  if (handler_.is_bound()) {
    handler_->OnBufferRetired(buffer_id);
  } else if (handler_in_process_) {
    handler_in_process_->OnBufferRetired(buffer_id);
  }
}

void VideoFrameHandlerProxyLacros::OnError(media::VideoCaptureError error) {
  if (handler_.is_bound()) {
    handler_->OnError(error);
  } else if (handler_in_process_) {
    handler_in_process_->OnError(error);
  }
}

void VideoFrameHandlerProxyLacros::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  if (handler_.is_bound()) {
    handler_->OnFrameDropped(reason);
  } else if (handler_in_process_) {
    handler_in_process_->OnFrameDropped(reason);
  }
}

void VideoFrameHandlerProxyLacros::DEPRECATED_OnNewCropVersion(
    uint32_t crop_version) {
  OnNewSubCaptureTargetVersion(crop_version);
}

void VideoFrameHandlerProxyLacros::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  if (handler_.is_bound()) {
    handler_->OnNewSubCaptureTargetVersion(sub_capture_target_version);
  } else if (handler_in_process_) {
    handler_in_process_->OnNewSubCaptureTargetVersion(
        sub_capture_target_version);
  }
}

void VideoFrameHandlerProxyLacros::OnFrameWithEmptyRegionCapture() {
  if (handler_.is_bound()) {
    handler_->OnFrameWithEmptyRegionCapture();
  } else if (handler_in_process_) {
    handler_in_process_->OnFrameWithEmptyRegionCapture();
  }
}

void VideoFrameHandlerProxyLacros::OnLog(const std::string& message) {
  if (handler_.is_bound()) {
    handler_->OnLog(message);
  } else if (handler_in_process_) {
    handler_in_process_->OnLog(message);
  }
}

void VideoFrameHandlerProxyLacros::OnStarted() {
  if (handler_.is_bound()) {
    handler_->OnStarted();
  } else if (handler_in_process_) {
    handler_in_process_->OnStarted();
  }
}

void VideoFrameHandlerProxyLacros::OnStartedUsingGpuDecode() {
  if (handler_.is_bound()) {
    handler_->OnStartedUsingGpuDecode();
  } else if (handler_in_process_) {
    handler_in_process_->OnStartedUsingGpuDecode();
  }
}

void VideoFrameHandlerProxyLacros::OnStopped() {
  if (handler_.is_bound()) {
    handler_->OnStopped();
  } else if (handler_in_process_) {
    handler_in_process_->OnStopped();
  }
}

}  // namespace video_capture
