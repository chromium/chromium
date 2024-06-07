// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_
#define REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_

#include <gio/gio.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/ei_event_watcher_glib.h"
#include "third_party/libei/cipd/include/libei.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"
#include "third_party/webrtc/modules/portal/xdg_session_details.h"

namespace remoting::xdg_portal {

struct DeviceRegion {
  uint32_t x;
  uint32_t y;
  uint32_t w;
  uint32_t h;
};

// This class is used by the `ChromotingInputThread` to inject input into the
// wayland remote host using XDG desktop portal APIs.
class RemoteDesktopPortalInjector : public EiEventWatcherGlib::EiEventHandler {
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
  ~RemoteDesktopPortalInjector() override;

  // This method populates the session details for this object. Session details
  // are borrowed from the wayland desktop capturer.
  void SetSessionDetails(webrtc::xdg_portal::SessionDetails session_details);

  // Methods related to input injection.
  void InjectMouseButton(int code, bool pressed);
  void InjectMouseScroll(int axis, int steps);
  void MovePointerTo(int x, int y);
  void MovePointerBy(int delta_x, int delta_y);
  void InjectKeyPress(int code, bool pressed, bool is_code = true);

  // EiEventWatcherGlib::EiEventHandler interface
  void HandleEiEvent(struct ei_event* event) override;

  void SetupLibei(base::OnceCallback<void(bool)> OnLibeiDone);
  void Shutdown();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  static void ValidateGDPBusProxyResult(GObject* proxy,
                                        GAsyncResult* result,
                                        gpointer user_data);
  static void OnEiFdRequested(GObject* object,
                              GAsyncResult* result,
                              gpointer user_data);
  void HandleRegions(struct ei_device* device);
  bool InDeviceRegion(uint32_t x, uint32_t y);

  void OnEiSeatAddedEvent(struct ei_event* event);
  void OnEiSeatRemovedEvent(struct ei_event* event);
  void OnEiDeviceAddedEvent(struct ei_event* event);
  void OnEiDeviceResumedEvent(struct ei_event* event);
  void OnEiDevicePausedEvent(struct ei_event* event);
  void OnEiDeviceRemovedEvent(struct ei_event* event);

  raw_ptr<GDBusConnection> connection_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<GDBusProxy> proxy_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<GCancellable> cancellable_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;

  uint32_t pipewire_stream_node_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string session_handle_ GUARDED_BY_CONTEXT(sequence_checker_);

  // EI related fields.
  raw_ptr<ei> ei_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  raw_ptr<struct ei_seat> ei_seat_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<ei_device> ei_pointer_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<ei_device> ei_keyboard_ GUARDED_BY_CONTEXT(sequence_checker_) =
      nullptr;
  raw_ptr<ei_device> ei_absolute_pointer_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  bool ei_pointer_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool ei_absolute_pointer_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
  bool ei_keyboard_enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool use_ei_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<EiEventWatcherGlib> ei_event_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int ei_fd_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;
  int device_serial_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;
  std::vector<DeviceRegion> device_regions_
      GUARDED_BY_CONTEXT(sequence_checker_) = {};
  base::OnceCallback<void(bool)> on_libei_setup_done_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting::xdg_portal

#endif  // REMOTING_HOST_LINUX_REMOTE_DESKTOP_PORTAL_INJECTOR_H_
