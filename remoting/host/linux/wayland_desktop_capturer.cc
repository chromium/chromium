// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_desktop_capturer.h"

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/remote_desktop_portal.h"
#include "remoting/host/linux/wayland_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/portal/xdg_session_details.h"

namespace remoting {

using webrtc::DesktopCaptureOptions;
#if defined(WEBRTC_USE_GIO)
using webrtc::DesktopCaptureMetadata;
#endif
using webrtc::xdg_portal::RequestResponse;
using webrtc::xdg_portal::SessionDetails;

WaylandDesktopCapturer::WaylandDesktopCapturer(
    const DesktopCaptureOptions& options)
    : base_capturer_pipewire_(options,
                              // Note: RemoteDesktopPortal doesn't own `this`
                              std::make_unique<xdg_portal::RemoteDesktopPortal>(
                                  this,
                                  options.prefer_cursor_embedded())) {
  base_capturer_pipewire_.SendFramesImmediately(true);
}

WaylandDesktopCapturer::~WaylandDesktopCapturer() {
  WaylandManager::Get()->OnDesktopCapturerDestroyed();
}

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

void WaylandDesktopCapturer::SetScreenResolution(ScreenResolution resolution,
                                                 webrtc::ScreenId screen_id) {
  // TODO(crbug.com/40266740): For multi-mon, we will need to verify that screen
  // id is managed by this capturer.
  base_capturer_pipewire_.UpdateResolution(resolution.dimensions().width(),
                                           resolution.dimensions().height());
}

void WaylandDesktopCapturer::SetMaxFrameRate(uint32_t max_frame_rate) {
  base_capturer_pipewire_.SetMaxFrameRate(max_frame_rate);
}

#if defined(WEBRTC_USE_GIO)
webrtc::DesktopCaptureMetadata WaylandDesktopCapturer::GetMetadata() {
  SessionDetails session_details = base_capturer_pipewire_.GetSessionDetails();
  DCHECK(session_details.proxy);
  DCHECK(session_details.cancellable);
  DCHECK(!session_details.session_handle.empty());
  DCHECK(session_details.pipewire_stream_node_id > 0);
  return {.session_details = std::move(session_details)};
}
#endif

void WaylandDesktopCapturer::OnScreenCastRequestResult(RequestResponse result,
                                                       uint32_t stream_node_id,
                                                       int fd) {
  base_capturer_pipewire_.OnScreenCastRequestResult(result, stream_node_id, fd);
  if (result == RequestResponse::kSuccess) {
    WaylandManager::Get()->OnDesktopCapturerMetadata(GetMetadata());
    WaylandManager::Get()->AddUpdateScreenResolutionCallback(
        base::BindRepeating(&WaylandDesktopCapturer::SetScreenResolution,
                            weak_factory_.GetWeakPtr()));
  } else {
    LOG(WARNING) << "Screen cast request didn't succeed, injector won't be "
                    "enabled";
  }
}

void WaylandDesktopCapturer::OnScreenCastSessionClosed() {
  base_capturer_pipewire_.OnScreenCastSessionClosed();
}

bool WaylandDesktopCapturer::SupportsFrameCallbacks() {
  return true;
}

}  // namespace remoting
