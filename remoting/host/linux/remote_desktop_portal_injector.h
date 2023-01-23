// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_
#define REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_

#include <gio/gio.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace remoting::xdg_portal {

// This class is used by the `ChromotingInputThread` to inject input into the
// wayland remote host using XDG desktop portal APIs.
class RemoteDesktopPortalInjector {
 public:
  enum ScrollType {
    VERTICAL_SCROLL = 0,
    HORIZONTAL_SCROLL = 1,
  };

  // Default constructor is used by input injector module.
  RemoteDesktopPortalInjector();
  RemoteDesktopPortalInjector(const RemoteDesktopPortalInjector&) = delete;
  RemoteDesktopPortalInjector& operator=(const RemoteDesktopPortalInjector&) =
      delete;
  ~RemoteDesktopPortalInjector();

  // This method populates the session details for this object. Session details
  // are borrowed from the wayland desktop capturer.
  void SetSessionDetails(webrtc::xdg_portal::SessionDetails session_details);

  // Methods related to input injection.
  void InjectMouseButton(int code, bool pressed);
  void InjectMouseScroll(int axis, int steps);
  void MovePointerTo(int x, int y);
  void MovePointerBy(int delta_x, int delta_y);
  void InjectKeyPress(int code, bool pressed, bool is_code = true);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  static void ValidateGDPBusProxyResult(GObject* proxy,
                                        GAsyncResult* result,
                                        gpointer user_data);

  raw_ptr<GDBusConnection> connection_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<GDBusProxy> proxy_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<GCancellable> cancellable_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  uint32_t pipewire_stream_node_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting::xdg_portal

#endif  // REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_
