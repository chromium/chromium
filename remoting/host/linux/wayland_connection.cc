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
      wrapped_display_(reinterpret_cast<struct wl_proxy*>(
          wl_proxy_create_wrapper(display_))),
      event_queue_(wl_display_create_queue(display_)) {
  wl_proxy_set_queue(wrapped_display_.get(), event_queue_.get());
  registry_ = wl_display_get_registry(
      reinterpret_cast<wl_display*>(wrapped_display_.get()));
  wl_registry_add_listener(registry_.get(), &wl_registry_listener_, this);

  event_watcher_ = ui::WaylandEventWatcher::CreateWaylandEventWatcher(
      display_.get(), event_queue_.get());
  event_watcher_->StartProcessingEvents();
}

WaylandConnection::~WaylandConnection() {
  if (event_queue_) {
    wl_event_queue_destroy(event_queue_.get());
  }
  if (display_) {
    wl_display_disconnect(display_.get());
  }
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
    connection->seat_id_ = name;
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
  if (connection->seat_id_ == name) {
    connection->wayland_seat_.HandleGlobalRemoveSeatEvent(name);
    connection->seat_id_ = 0;
  } else {
    connection->wayland_display_.HandleGlobalRemoveDisplayEvent(name);
  }
}

uint32_t WaylandConnection::GetSeatId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(wayland_seat_.GetSeatId(), 0u);
  return wayland_seat_.GetSeatId();
}

DesktopDisplayInfo WaylandConnection::GetCurrentDisplayInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wayland_display_.GetCurrentDisplayInfo();
}

void WaylandConnection::SetSeatPresentCallback(
    WaylandSeat::OnSeatPresentCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wayland_seat_.SetSeatPresentCallback(std::move(callback));
}

}  // namespace remoting
