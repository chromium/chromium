// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_video_capturer_list.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/mojo_video_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

MojoVideoCapturerList::MojoVideoCapturerList() = default;
MojoVideoCapturerList::~MojoVideoCapturerList() = default;

mojom::CreateVideoCapturerResultPtr MojoVideoCapturerList::CreateVideoCapturer(
    webrtc::ScreenId screen_id,
    DesktopEnvironment* environment,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner) {
  // Work around b/332935355 by destroying any full-desktop capturer before
  // creating a per-screen capturer. When switching from single-stream to
  // multi-stream, the Network process destroys the full-desktop capturer and
  // creates new capturers for each display. But (in the Desktop process) Mojo
  // does not guarantee the destruction of the previous capturer will happen
  // before creating the new capturer (because each capturer uses a separate
  // Mojo pipe). The overlapping capturer lifetimes appear to trigger the bug.

  // TODO: b/326319067 - Remove this hack when the Windows host no longer
  // supports single-stream.
  video_capturers_.erase(webrtc::kFullDesktopScreenId);

  auto capturer = std::make_unique<MojoVideoCapturer>(
      environment->CreateVideoCapturer(screen_id), caller_task_runner);
  mojom::CreateVideoCapturerResultPtr result = capturer->Start();

  // Register handler to remove the capturer when it is no longer needed.
  // base::Unretained() is safe because the callback is cancelled if the Mojo
  // endpoint is deleted, and these endpoints are owned by
  // this->video_capturers_.
  capturer->SetDisconnectHandler(base::BindOnce(
      [](MojoVideoCapturerList* self, webrtc::ScreenId id) {
        // The capturer is guaranteed to still exist in the map, because this
        // handler is cancelled if the capturer is deleted from the map (or it
        // is overwritten in the map by a different capturer for this
        // display ID).
        self->video_capturers_.erase(id);
      },
      base::Unretained(this), screen_id));

  // Store the new capturer in the map, which will delete any previous capturer
  // with the same ID. If the caller (Network process) creates 2 capturers with
  // the same ID, the first one will become disconnected. Multiple concurrent
  // VideoStreams capturing from the same display are not supported.
  video_capturers_[screen_id] = std::move(capturer);

  return result;
}

bool MojoVideoCapturerList::IsEmpty() const {
  return video_capturers_.empty();
}

void MojoVideoCapturerList::Clear() {
  video_capturers_.clear();
}

void MojoVideoCapturerList::SetMouseCursor(const webrtc::MouseCursor& cursor) {
  for (auto& [_, capturer] : video_capturers_) {
    capturer->SetMouseCursor(
        base::WrapUnique(webrtc::MouseCursor::CopyOf(cursor)));
  }
}

void MojoVideoCapturerList::SetMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  for (auto& [_, video_capturer] : video_capturers_) {
    video_capturer->SetMouseCursorPosition(position);
  }
}

void MojoVideoCapturerList::SetComposeEnabled(bool enabled) {
  for (auto& [_, video_capturer] : video_capturers_) {
    video_capturer->SetComposeEnabled(enabled);
  }
}

}  // namespace remoting
