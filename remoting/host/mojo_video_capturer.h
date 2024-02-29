// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_
#define REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

class AutoThreadTaskRunner;
class DesktopCapturer;

class MojoVideoCapturer : public webrtc::DesktopCapturer::Callback {
 public:
  MojoVideoCapturer(std::unique_ptr<DesktopCapturer> capturer,
                    scoped_refptr<AutoThreadTaskRunner> caller_task_runner);
  MojoVideoCapturer(const MojoVideoCapturer&) = delete;
  MojoVideoCapturer& operator=(const MojoVideoCapturer&) = delete;
  ~MojoVideoCapturer() override;

  void set_event_handler(mojom::DesktopSessionEventHandler* event_handler) {
    event_handler_ = event_handler;
  }

  void Start();
  void SelectSource(webrtc::DesktopCapturer::SourceId id);
  void CaptureFrame();
  void SetComposeEnabled(bool enabled);
  void SetMouseCursor(std::unique_ptr<webrtc::MouseCursor> mouse_cursor);
  void SetMouseCursorPosition(const webrtc::DesktopVector& position);

  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

 private:
  // Notifies the network process when a new shared memory region is created.
  void OnSharedMemoryRegionCreated(int id,
                                   base::ReadOnlySharedMemoryRegion region,
                                   uint32_t size);

  // Notifies the network process when a shared memory region is released.
  void OnSharedMemoryRegionReleased(int id);

  // The real video-capturer wrapped by this class.
  std::unique_ptr<DesktopCapturer> video_capturer_;

  // Keep reference to the last frame sent to make sure shared buffer is alive
  // before it's received.
  std::unique_ptr<webrtc::DesktopFrame> last_frame_;

  // Event-handler used for sending capturer events to the network process.
  raw_ptr<mojom::DesktopSessionEventHandler> event_handler_ = nullptr;

  base::WeakPtr<MojoVideoCapturer> weak_ptr_;
  base::WeakPtrFactory<MojoVideoCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_
