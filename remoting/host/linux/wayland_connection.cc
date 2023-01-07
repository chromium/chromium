// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_connection.h"

#include "base/time/time.h"
#include "remoting/base/logging.h"

namespace remoting {

WaylandConnection::WaylandConnection(std::string wl_socket)
    : wl_socket_(wl_socket),
      display_(wl_display_connect(wl_socket_.c_str())),
      registry_(wl_display_get_registry(display_.get())) {
  wl_registry_add_listener(registry_.get(), &wl_registry_listener_, this);
  timer_.Start(FROM_HERE, base::Milliseconds(5), this,
               &WaylandConnection::DispatchWaylandEvents);
}

WaylandConnection::~WaylandConnection() {
  if (display_)
    wl_display_disconnect(display_.get());
}

// static
void WaylandConnection::OnGlobalEvent(void* data,
                                      struct wl_registry* registry,
                                      uint32_t name,
                                      const char* interface,
                                      uint32_t version) {
  VLOG(1) << __func__
          << ": Interface: " << interface << ", version: " << version
          << ", name: " << name;
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  DCHECK(connection);
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection->sequence_checker_);
  if (strcmp(interface, wl_output_interface.name) == 0 ||
      strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
    connection->wayland_display_.HandleGlobalDisplayEvent(registry, name,
                                                          interface, version);
  } else if (strcmp(interface, wl_seat_interface.name) == 0) {
    connection->wayland_seat_.HandleGlobalSeatEvent(registry, name, interface,
                                                    version);
  }
}

// static
void WaylandConnection::OnGlobalRemoveEvent(void* data,
                                            struct wl_registry* registry,
                                            uint32_t name) {
  VLOG(1) << __func__ << " Removing name: " << name;
  WaylandConnection* connection = static_cast<WaylandConnection*>(data);
  DCHECK(connection);
  DCHECK_CALLED_ON_VALID_SEQUENCE(connection->sequence_checker_);
  connection->wayland_display_.HandleGlobalRemoveDisplayEvent(name);
}

void WaylandConnection::DispatchWaylandEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(display_);
  bool success = true;
  if (wl_display_dispatch_pending(display_.get()) < 0) {
    LOG(ERROR) << "Failed to dispatch requests to server, error: " << errno;
    success = false;
  }
  if (wl_display_roundtrip(display_.get()) < 0) {
    LOG(ERROR) << "Dispatched requests to wayland server failed, error: "
               << errno;
    success = false;
  }
  if (!success)
    timer_.Stop();
}

DesktopDisplayInfo WaylandConnection::GetCurrentDisplayInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wayland_display_.GetCurrentDisplayInfo();
}

}  // namespace remoting
