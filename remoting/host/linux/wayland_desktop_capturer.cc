// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_desktop_capturer.h"

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
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
  constexpr base::TimeDelta kMetadataTimeout = base::Seconds(3);
  base::Time start_time = base::Time::Now();
  do {
    SessionDetails session_details =
        base_capturer_pipewire_.GetSessionDetails();
    if (session_details.proxy && session_details.cancellable &&
        !session_details.session_handle.empty() &&
        session_details.pipewire_stream_node_id > 0) {
      return {.session_details = std::move(session_details)};
    }
    base::PlatformThread::Sleep(base::Milliseconds(10));
  } while (base::Time::Now() - start_time < kMetadataTimeout);
  LOG(ERROR) << "Unable to retrievel portal session details in time: "
             << kMetadataTimeout << ". CRD session will fail.";
  return {};
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
