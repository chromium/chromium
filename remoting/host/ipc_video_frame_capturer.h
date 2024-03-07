// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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
class IpcVideoFrameCapturer : public DesktopCapturer {
 public:
  IpcVideoFrameCapturer();

  IpcVideoFrameCapturer(const IpcVideoFrameCapturer&) = delete;
  IpcVideoFrameCapturer& operator=(const IpcVideoFrameCapturer&) = delete;

  ~IpcVideoFrameCapturer() override;

  // Sets the Mojo implementation for sending video-capture requests to the
  // Desktop process. `control` may be nullptr to indicate the Mojo endpoint is
  // disconnected.
  void SetDesktopSessionControl(mojom::DesktopSessionControl* control);

  // Returns a WeakPtr to this capturer. Used by DesktopSessionProxy to set
  // the Mojo implementation when the endpoints are re-created during a
  // DetachFromDesktop/Reattach sequence. A WeakPtr is needed since the
  // lifetime of this capturer is bound to the VideoStream.
  base::WeakPtr<IpcVideoFrameCapturer> GetWeakPtr();

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // Called by DesktopSessionProxy's implementation of
  // mojom::DesktopSessionEventHandler.
  void OnSharedMemoryRegionCreated(int id,
                                   base::ReadOnlySharedMemoryRegion region,
                                   uint32_t size);
  void OnSharedMemoryRegionReleased(int id);
  void OnCaptureResult(mojom::CaptureResultPtr result);

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

  // Points to the IPC channel to the desktop session agent. This is owned by
  // DesktopSessionProxy which is responsible for setting/unsetting this
  // whenever the Mojo Remote is bound/unbound.
  raw_ptr<mojom::DesktopSessionControl> desktop_session_control_ = nullptr;

  int pending_capture_frame_requests_ = 0;

  // Shared memory buffers by Id. Each buffer is owned by the corresponding
  // frame.
  SharedBuffers shared_buffers_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcVideoFrameCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
