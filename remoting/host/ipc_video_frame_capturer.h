// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

class DesktopSessionProxy;
class IpcSharedBufferCore;

// Routes webrtc::DesktopCapturer calls though the IPC channel to the desktop
// session agent running in the desktop integration process.
// GetSourceList() and SelectSource() functions are not implemented, they always
// return false.
class IpcVideoFrameCapturer : public DesktopCapturer,
                              public mojom::VideoCapturerEventHandler {
 public:
  explicit IpcVideoFrameCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcVideoFrameCapturer(const IpcVideoFrameCapturer&) = delete;
  IpcVideoFrameCapturer& operator=(const IpcVideoFrameCapturer&) = delete;

  ~IpcVideoFrameCapturer() override;

  // Sets the new Mojo endpoints for the capturer.
  void OnCreateVideoCapturerResult(mojom::CreateVideoCapturerResultPtr result);

  // Returns a WeakPtr to this capturer. Used by DesktopSessionProxy to set
  // the Mojo endpoints for this capturer (on initial creation, and after a
  // desktop detach/attach sequence. A WeakPtr is needed since the lifetime of
  // this capturer is bound to the VideoStream.
  base::WeakPtr<IpcVideoFrameCapturer> GetWeakPtr();

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // mojom::VideoCapturerEventHandler interface.
  void OnSharedMemoryRegionCreated(int id,
                                   base::ReadOnlySharedMemoryRegion region,
                                   uint32_t size) override;
  void OnSharedMemoryRegionReleased(int id) override;
  void OnCaptureResult(mojom::CaptureResultPtr result) override;

 private:
  typedef std::map<int, scoped_refptr<IpcSharedBufferCore>> SharedBuffers;

  // Called when the Mojo endpoint is disconnected. Cleans up shared buffers,
  // and sends fake responses to `callback_` where needed to keep the frame
  // scheduler in sync.
  void OnDisconnect();

  // Returns a shared buffer from the list of known buffers.
  scoped_refptr<IpcSharedBufferCore> GetSharedBufferCore(int id);

  // Points to the callback passed to webrtc::DesktopCapturer::Start().
  raw_ptr<webrtc::DesktopCapturer::Callback> callback_ = nullptr;

  // Mojo endpoint for sending capturer commands to the Desktop process.
  mojo::Remote<mojom::VideoCapturer> capturer_control_;

  // Mojo endpoint for receiving capturer events from the Desktop process.
  mojo::Receiver<mojom::VideoCapturerEventHandler> event_handler_{this};

  int pending_capture_frame_requests_ = 0;

  // Shared memory buffers by Id. Each buffer is owned by the corresponding
  // frame.
  SharedBuffers shared_buffers_;

  // Used by SelectSource() in the single-stream case. Changing the display
  // requires creating a new video-capturer in the Desktop process, and
  // connecting the new Mojo endpoints.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcVideoFrameCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
