// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include "base/logging.h"
#include "remoting/host/desktop_session_proxy.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

IpcVideoFrameCapturer::IpcVideoFrameCapturer(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : callback_(nullptr),
      desktop_session_proxy_(desktop_session_proxy),
      capture_pending_(false) {}

IpcVideoFrameCapturer::~IpcVideoFrameCapturer() = default;

void IpcVideoFrameCapturer::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = callback;
  desktop_session_proxy_->SetVideoCapturer(weak_factory_.GetWeakPtr());
}

void IpcVideoFrameCapturer::CaptureFrame() {
  DCHECK(!capture_pending_);
  capture_pending_ = true;
  desktop_session_proxy_->CaptureFrame();
}

void IpcVideoFrameCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(capture_pending_);
  capture_pending_ = false;
  callback_->OnCaptureResult(result, std::move(frame));
}

bool IpcVideoFrameCapturer::GetSourceList(SourceList* sources) {
  NOTIMPLEMENTED();
  return false;
}

bool IpcVideoFrameCapturer::SelectSource(SourceId id) {
  desktop_session_proxy_->SelectSource(id);
  return true;
}

}  // namespace remoting
