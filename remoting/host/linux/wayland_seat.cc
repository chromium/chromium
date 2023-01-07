// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_seat.h"

namespace remoting {

namespace {

constexpr int kSeatInterfaceVersion = 3;

}  // namespace

WaylandSeat::WaylandSeat() = default;
WaylandSeat::~WaylandSeat() = default;

void WaylandSeat::HandleGlobalSeatEvent(struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registry);
  DCHECK(strcmp(interface, wl_seat_interface.name) == 0);
  wl_seat_ = static_cast<wl_seat*>(wl_registry_bind(
      registry, name, &wl_seat_interface, kSeatInterfaceVersion));
  wl_seat_add_listener(wl_seat_, &wl_seat_listener_, this);
}

// static
void WaylandSeat::OnSeatCapabilitiesEvent(void* data,
                                          struct wl_seat* wl_seat,
                                          uint32_t capabilities) {
  WaylandSeat* wayland_seat = static_cast<WaylandSeat*>(data);
  DCHECK(wayland_seat);
  DCHECK_CALLED_ON_VALID_SEQUENCE(wayland_seat->sequence_checker_);
  const bool has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  if (has_keyboard && !wayland_seat->wayland_keyboard_) {
    wayland_seat->wayland_keyboard_ =
        std::make_unique<WaylandKeyboard>(wayland_seat->wl_seat_);
  } else if (!has_keyboard && wayland_seat->wayland_keyboard_) {
    wayland_seat->wayland_keyboard_.reset();
  }
}

// static
void WaylandSeat::OnSeatNameEvent(void* data,
                                  struct wl_seat* wl_seat,
                                  const char* name) {
  WaylandSeat* wayland_seat = static_cast<WaylandSeat*>(data);
  DCHECK(wayland_seat);
  DCHECK_CALLED_ON_VALID_SEQUENCE(wayland_seat->sequence_checker_);
}

}  // namespace remoting
