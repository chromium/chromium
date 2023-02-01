// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
#define REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_

#include "base/functional/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

// An interface extension to make synchronous methods on webrtc::DesktopCapturer
// asynchronous by allowing the new wrapper methods to accept callbacks.
class DesktopCapturer : public webrtc::DesktopCapturer {
 public:
  // Indicates whether to compose the mouse cursor into the desktop frame.
  virtual void SetComposeEnabled(bool enabled) {}

  // Change the shape of the composed mouse cursor.
  virtual void SetMouseCursor(
      std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {}

  // Change the position of the composed mouse cursor.
  virtual void SetMouseCursorPosition(const webrtc::DesktopVector& position) {}

  // Whether capturer can notify the callback interface of the available frames
  // immediately.
  virtual bool SupportsFrameCallbacks();

#if defined(WEBRTC_USE_GIO)
  virtual void GetMetadataAsync(
      base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback) {}
#endif
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
