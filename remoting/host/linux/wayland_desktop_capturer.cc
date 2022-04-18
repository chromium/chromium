// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_desktop_capturer.h"

#include "remoting/host/linux/remote_desktop_portal.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace remoting {

using webrtc::DesktopCaptureOptions;
#if defined(WEBRTC_USE_GIO)
using webrtc::DesktopCaptureMetadata;
#endif
using webrtc::xdg_portal::RequestResponse;
using webrtc::xdg_portal::SessionDetails;

WaylandDesktopCapturer::WaylandDesktopCapturer(
    const DesktopCaptureOptions& options)
    : base_capturer_pipewire_(
          options,
          // Note: RemoteDesktopPortal doesn't own `this`
          std::make_unique<xdg_portal::RemoteDesktopPortal>(this)) {}
WaylandDesktopCapturer::~WaylandDesktopCapturer() {}

void WaylandDesktopCapturer::Start(Callback* callback) {
  base_capturer_pipewire_.Start(callback);
}

void WaylandDesktopCapturer::CaptureFrame() {
  return base_capturer_pipewire_.CaptureFrame();
}

bool WaylandDesktopCapturer::GetSourceList(SourceList* sources) {
  return base_capturer_pipewire_.GetSourceList(sources);
}

bool WaylandDesktopCapturer::SelectSource(SourceId id) {
  return base_capturer_pipewire_.SelectSource(id);
}

#if defined(WEBRTC_USE_GIO)
webrtc::DesktopCaptureMetadata WaylandDesktopCapturer::GetMetadata() {
  return {
      .session_details = base_capturer_pipewire_.GetSessionDetails(),
  };
}
#endif

void WaylandDesktopCapturer::OnScreenCastRequestResult(RequestResponse result,
                                                       uint32_t stream_node_id,
                                                       int fd) {
  base_capturer_pipewire_.OnScreenCastRequestResult(result, stream_node_id, fd);
}

void WaylandDesktopCapturer::OnScreenCastSessionClosed() {
  base_capturer_pipewire_.OnScreenCastSessionClosed();
}

}  // namespace remoting
