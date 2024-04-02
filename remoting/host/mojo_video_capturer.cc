// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_video_capturer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/video_memory_utils.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/shared_memory.h"

namespace remoting {

MojoVideoCapturer::MojoVideoCapturer(
    std::unique_ptr<DesktopCapturer> capturer,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner)
    : video_capturer_(std::move(capturer)) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  video_capturer_->SetSharedMemoryFactory(
      std::make_unique<SharedVideoMemoryFactory>(
          base::BindPostTask(
              caller_task_runner,
              base::BindRepeating(
                  &MojoVideoCapturer::OnSharedMemoryRegionCreated, weak_ptr_)),
          base::BindPostTask(
              caller_task_runner,
              base::BindRepeating(
                  &MojoVideoCapturer::OnSharedMemoryRegionReleased,
                  weak_ptr_))));
}

MojoVideoCapturer::~MojoVideoCapturer() = default;

void MojoVideoCapturer::SetDisconnectHandler(base::OnceClosure handler) {
  capturer_control_.set_disconnect_handler(std::move(handler));
}

mojom::CreateVideoCapturerResultPtr MojoVideoCapturer::Start() {
  video_capturer_->Start(this);
  return mojom::CreateVideoCapturerResult::New(
      capturer_control_.BindNewPipeAndPassRemote(),
      event_handler_.BindNewPipeAndPassReceiver());
}

void MojoVideoCapturer::CaptureFrame() {
  video_capturer_->CaptureFrame();
}

void MojoVideoCapturer::SetComposeEnabled(bool enabled) {
  video_capturer_->SetComposeEnabled(enabled);
}

void MojoVideoCapturer::SetMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  video_capturer_->SetMouseCursor(std::move(mouse_cursor));
}

void MojoVideoCapturer::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  video_capturer_->SetMouseCursorPosition(position);
}

void MojoVideoCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  mojom::CaptureResultPtr capture_result;
  if (frame) {
    DCHECK_EQ(result, webrtc::DesktopCapturer::Result::SUCCESS);
    std::vector<webrtc::DesktopRect> dirty_region;
    for (webrtc::DesktopRegion::Iterator i(frame->updated_region());
         !i.IsAtEnd(); i.Advance()) {
      dirty_region.push_back(i.rect());
    }
    capture_result =
        mojom::CaptureResult::NewDesktopFrame(mojom::DesktopFrame::New(
            frame->shared_memory()->id(), frame->stride(), frame->size(),
            std::move(dirty_region), frame->capture_time_ms(), frame->dpi(),
            frame->capturer_id()));
  } else {
    DCHECK_NE(result, webrtc::DesktopCapturer::Result::SUCCESS);
    capture_result = mojom::CaptureResult::NewCaptureError(result);
  }

  last_frame_ = std::move(frame);

  if (event_handler_) {
    event_handler_->OnCaptureResult(std::move(capture_result));
  }
}

void MojoVideoCapturer::OnSharedMemoryRegionCreated(
    int id,
    base::ReadOnlySharedMemoryRegion region,
    uint32_t size) {
  if (event_handler_) {
    event_handler_->OnSharedMemoryRegionCreated(id, std::move(region), size);
  }
}

void MojoVideoCapturer::OnSharedMemoryRegionReleased(int id) {
  if (event_handler_) {
    event_handler_->OnSharedMemoryRegionReleased(id);
  }
}

}  // namespace remoting
