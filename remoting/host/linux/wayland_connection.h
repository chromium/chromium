// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_CONNECTION_H_
#define REMOTING_HOST_LINUX_WAYLAND_CONNECTION_H_

#include <wayland-client.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/wayland_display.h"
#include "remoting/host/linux/wayland_seat.h"
#include "ui/events/platform/wayland/wayland_event_watcher.h"

namespace remoting {

// This class models Wayland connection and acts as a Wayland client by
// connecting to the provided wayland socket where the Wayland compositor is
// listening.
class WaylandConnection {
 public:
  explicit WaylandConnection(std::string wl_socket);
  ~WaylandConnection();

  WaylandConnection(const WaylandConnection&) = delete;
  WaylandConnection& operator=(const WaylandConnection&) = delete;

  DesktopDisplayInfo GetCurrentDisplayInfo() const;
  uint32_t GetSeatId() const;
  void SetSeatPresentCallback(WaylandSeat::OnSeatPresentCallback callback);

 private:
  static void OnGlobalEvent(void* data,
                            struct wl_registry* registry,
                            uint32_t name,
                            const char* interface,
                            uint32_t version);

  static void OnGlobalRemoveEvent(void* data,
                                  struct wl_registry* registry,
                                  uint32_t name);

  SEQUENCE_CHECKER(sequence_checker_);

  std::string wl_socket_;
  raw_ptr<struct wl_display> display_ = nullptr;
  raw_ptr<struct wl_proxy> wrapped_display_ = nullptr;
  raw_ptr<struct wl_event_queue> event_queue_ = nullptr;
  raw_ptr<struct wl_registry> registry_ = nullptr;
  const struct wl_registry_listener wl_registry_listener_ = {
      .global = OnGlobalEvent,
      .global_remove = OnGlobalRemoveEvent,
  };
  std::unique_ptr<ui::WaylandEventWatcher> event_watcher_;
  WaylandDisplay wayland_display_;
  WaylandSeat wayland_seat_;
  uint32_t seat_id_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_CONNECTION_H_
