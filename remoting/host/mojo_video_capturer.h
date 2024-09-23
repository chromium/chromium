// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_
#define REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

class AutoThreadTaskRunner;
class DesktopCapturer;

class MojoVideoCapturer : public webrtc::DesktopCapturer::Callback,
                          public mojom::VideoCapturer {
 public:
  MojoVideoCapturer(std::unique_ptr<DesktopCapturer> capturer,
                    scoped_refptr<AutoThreadTaskRunner> caller_task_runner);
  MojoVideoCapturer(const MojoVideoCapturer&) = delete;
  MojoVideoCapturer& operator=(const MojoVideoCapturer&) = delete;
  ~MojoVideoCapturer() override;

  // Notifies the handler if the channel becomes disconnected. The handler may
  // delete this object. Must only be called after Start(), which binds the Mojo
  // endpoints.
  void SetDisconnectHandler(base::OnceClosure handler);

  // Starts the capturer, creating new Mojo endpoints to return to the network
  // process.
  mojom::CreateVideoCapturerResultPtr Start();

  // mojom::VideoCapturer implementation.
  void CaptureFrame() override;

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

  // Endpoint for receiving capturer commands from the network process.
  mojo::Receiver<mojom::VideoCapturer> capturer_control_{this};

  // Endpoint for sending capturer events to the network process.
  mojo::Remote<mojom::VideoCapturerEventHandler> event_handler_;

  base::WeakPtr<MojoVideoCapturer> weak_ptr_;
  base::WeakPtrFactory<MojoVideoCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_VIDEO_CAPTURER_H_
