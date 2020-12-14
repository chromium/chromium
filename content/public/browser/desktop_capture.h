// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_

#include "content/common/content_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace content {
namespace desktop_capture {

// Creates a DesktopCaptureOptions with required settings.
CONTENT_EXPORT webrtc::DesktopCaptureOptions CreateDesktopCaptureOptions();

// Creates specific DesktopCapturer with required settings.
CONTENT_EXPORT std::unique_ptr<webrtc::DesktopCapturer> CreateScreenCapturer();
CONTENT_EXPORT std::unique_ptr<webrtc::DesktopCapturer> CreateWindowCapturer();

// Returns whether we can use PipeWire capturer based on:
// 1) We run Linux Wayland session
// 2) WebRTC is built with PipeWire enabled
// 3) Chromium has features::kWebRtcPipeWireCapturer enabled
CONTENT_EXPORT bool CanUsePipeWire();

}  // namespace desktop_capture
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_CAPTURE_H_
