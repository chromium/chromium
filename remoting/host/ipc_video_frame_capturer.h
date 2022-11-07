// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

class DesktopSessionProxy;

// Routes webrtc::DesktopCapturer calls though the IPC channel to the desktop
// session agent running in the desktop integration process.
// GetSourceList() and SelectSource() functions are not implemented, they always
// return false.
class IpcVideoFrameCapturer : public DesktopCapturer {
 public:
  explicit IpcVideoFrameCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcVideoFrameCapturer(const IpcVideoFrameCapturer&) = delete;
  IpcVideoFrameCapturer& operator=(const IpcVideoFrameCapturer&) = delete;

  ~IpcVideoFrameCapturer() override;

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // Called when a video |frame| has been captured.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

 private:
  // Points to the callback passed to webrtc::DesktopCapturer::Start().
  raw_ptr<webrtc::DesktopCapturer::Callback> callback_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Set to true when a frame is being captured.
  bool capture_pending_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcVideoFrameCapturer> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
