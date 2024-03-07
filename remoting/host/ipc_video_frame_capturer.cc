// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include "base/check.h"
#include "base/notreached.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/video_memory_utils.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

IpcVideoFrameCapturer::IpcVideoFrameCapturer() = default;

IpcVideoFrameCapturer::~IpcVideoFrameCapturer() = default;

void IpcVideoFrameCapturer::SetDesktopSessionControl(
    mojom::DesktopSessionControl* control) {
  desktop_session_control_ = control;
  if (!control) {
    OnDisconnect();
  }
}

base::WeakPtr<IpcVideoFrameCapturer> IpcVideoFrameCapturer::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void IpcVideoFrameCapturer::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = callback;
}

void IpcVideoFrameCapturer::CaptureFrame() {
  if (desktop_session_control_) {
    ++pending_capture_frame_requests_;
    desktop_session_control_->CaptureFrame();
  } else {
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                               nullptr);
  }
}

bool IpcVideoFrameCapturer::GetSourceList(SourceList* sources) {
  NOTIMPLEMENTED();
  return false;
}

bool IpcVideoFrameCapturer::SelectSource(SourceId id) {
  if (desktop_session_control_) {
    desktop_session_control_->SelectSource(id);
  }
  return true;
}

void IpcVideoFrameCapturer::OnSharedMemoryRegionCreated(
    int id,
    base::ReadOnlySharedMemoryRegion region,
    uint32_t size) {
  auto shared_buffer =
      base::MakeRefCounted<IpcSharedBufferCore>(id, std::move(region));

  if (shared_buffer->memory() != nullptr &&
      !shared_buffers_.insert(std::make_pair(id, shared_buffer)).second) {
    LOG(ERROR) << "Duplicate shared buffer id " << id << " encountered";
  }
}

void IpcVideoFrameCapturer::OnSharedMemoryRegionReleased(int id) {
  // Drop the cached reference to the buffer.
  shared_buffers_.erase(id);
}

void IpcVideoFrameCapturer::OnCaptureResult(mojom::CaptureResultPtr result) {
  CHECK(pending_capture_frame_requests_)
      << "Received unexpected capture result.";

  --pending_capture_frame_requests_;

  if (result->is_capture_error()) {
    callback_->OnCaptureResult(result->get_capture_error(), nullptr);
    return;
  }

  // Assume that |desktop_frame| is well-formed because it was received from a
  // more privileged process.
  mojom::DesktopFramePtr& desktop_frame = result->get_desktop_frame();
  scoped_refptr<IpcSharedBufferCore> shared_buffer_core =
      GetSharedBufferCore(desktop_frame->shared_buffer_id);
  CHECK(shared_buffer_core.get());

  std::unique_ptr<webrtc::DesktopFrame> frame =
      std::make_unique<webrtc::SharedMemoryDesktopFrame>(
          desktop_frame->size, desktop_frame->stride,
          new IpcSharedBuffer(shared_buffer_core));
  frame->set_capture_time_ms(desktop_frame->capture_time_ms);
  frame->set_dpi(desktop_frame->dpi);
  frame->set_capturer_id(desktop_frame->capturer_id);

  for (const auto& rect : desktop_frame->dirty_region) {
    frame->mutable_updated_region()->AddRect(rect);
  }

  callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                             std::move(frame));
}

void IpcVideoFrameCapturer::OnDisconnect() {
  shared_buffers_.clear();

  // Generate fake responses to keep the frame scheduler in sync.
  while (pending_capture_frame_requests_) {
    OnCaptureResult(mojom::CaptureResult::NewCaptureError(
        webrtc::DesktopCapturer::Result::ERROR_TEMPORARY));
  }
}

scoped_refptr<IpcSharedBufferCore> IpcVideoFrameCapturer::GetSharedBufferCore(
    int id) {
  SharedBuffers::const_iterator i = shared_buffers_.find(id);
  if (i != shared_buffers_.end()) {
    return i->second;
  } else {
    LOG(ERROR) << "Failed to find the shared buffer " << id;
    return nullptr;
  }
}

}  // namespace remoting
