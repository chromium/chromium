// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/receiver_media_to_crosapi_adapter.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/video_capture/lacros/video_buffer_adapters.h"

namespace video_capture {

namespace {
void OnFrameDone(mojo::Remote<crosapi::mojom::ScopedAccessPermission>
                     remote_access_permission) {
  // There's nothing to do here, except to serve as a keep-alive for the
  // remote until the callback is run. So just let it be destroyed now.
}

std::unique_ptr<media::ScopedFrameDoneHelper> GetAccessPermissionHelper(
    mojo::PendingRemote<crosapi::mojom::ScopedAccessPermission>
        pending_remote_access_permission) {
  return std::make_unique<media::ScopedFrameDoneHelper>(base::BindOnce(
      &OnFrameDone, mojo::Remote<crosapi::mojom::ScopedAccessPermission>(
                        std::move(pending_remote_access_permission))));
}

media::ReadyFrameInBuffer ToMediaBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer) {
  return media::ReadyFrameInBuffer(
      buffer->buffer_id, buffer->frame_feedback_id,
      GetAccessPermissionHelper(std::move(buffer->access_permission)),
      ConvertToMediaVideoFrameInfo(std::move(buffer->frame_info)));
}

}  // namespace

ReceiverMediaToCrosapiAdapter::ReceiverMediaToCrosapiAdapter(
    mojo::PendingReceiver<crosapi::mojom::VideoFrameHandler> proxy_receiver,
    std::unique_ptr<media::VideoFrameReceiver> handler)
    : handler_(std::move(handler)) {
  receiver_.Bind(std::move(proxy_receiver));
}

ReceiverMediaToCrosapiAdapter::~ReceiverMediaToCrosapiAdapter() = default;

void ReceiverMediaToCrosapiAdapter::OnNewBuffer(
    int buffer_id,
    crosapi::mojom::VideoBufferHandlePtr buffer_handle) {
  handler_->OnNewBuffer(buffer_id,
                        ConvertToMediaVideoBuffer(std::move(buffer_handle)));
}

void ReceiverMediaToCrosapiAdapter::OnFrameReadyInBuffer(
    crosapi::mojom::ReadyFrameInBufferPtr buffer,
    std::vector<crosapi::mojom::ReadyFrameInBufferPtr> scaled_buffers) {
  std::vector<media::ReadyFrameInBuffer> media_scaled_buffers;
  for (auto& b : scaled_buffers) {
    media_scaled_buffers.push_back(ToMediaBuffer(std::move(b)));
  }

  handler_->OnFrameReadyInBuffer(ToMediaBuffer(std::move(buffer)),
                                 std::move(media_scaled_buffers));
}

void ReceiverMediaToCrosapiAdapter::OnBufferRetired(int buffer_id) {
  handler_->OnBufferRetired(buffer_id);
}

void ReceiverMediaToCrosapiAdapter::OnError(media::VideoCaptureError error) {
  handler_->OnError(error);
}

void ReceiverMediaToCrosapiAdapter::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  handler_->OnFrameDropped(reason);
}

void ReceiverMediaToCrosapiAdapter::OnFrameWithEmptyRegionCapture() {
  handler_->OnFrameWithEmptyRegionCapture();
}

void ReceiverMediaToCrosapiAdapter::OnLog(const std::string& message) {
  handler_->OnLog(message);
}

void ReceiverMediaToCrosapiAdapter::OnStarted() {
  handler_->OnStarted();
}

void ReceiverMediaToCrosapiAdapter::OnStartedUsingGpuDecode() {
  handler_->OnStartedUsingGpuDecode();
}

void ReceiverMediaToCrosapiAdapter::OnStopped() {
  handler_->OnStopped();
}

}  // namespace video_capture
