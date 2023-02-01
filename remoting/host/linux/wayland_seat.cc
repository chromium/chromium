// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_seat.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/linux/wayland_manager.h"

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
  seat_id_ = name;
  wl_seat_ = static_cast<wl_seat*>(wl_registry_bind(
      registry, name, &wl_seat_interface, kSeatInterfaceVersion));
  wl_seat_add_listener(wl_seat_, &wl_seat_listener_, this);
  if (seat_present_callback_) {
    std::move(seat_present_callback_).Run();
  }
}

uint32_t WaylandSeat::GetSeatId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seat_id_;
}

void WaylandSeat::HandleGlobalRemoveSeatEvent(uint32_t name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(seat_id_, name);
  seat_id_ = 0;
}

// static
void WaylandSeat::OnSeatCapabilitiesEvent(void* data,
                                          struct wl_seat* wl_seat,
                                          uint32_t capabilities) {
  WaylandSeat* wayland_seat = static_cast<WaylandSeat*>(data);
  DCHECK(wayland_seat);
  DCHECK_CALLED_ON_VALID_SEQUENCE(wayland_seat->sequence_checker_);
  const bool has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
  const bool has_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
  if (has_keyboard && !wayland_seat->wayland_keyboard_) {
    wayland_seat->wayland_keyboard_ =
        std::make_unique<WaylandKeyboard>(wayland_seat->wl_seat_);
    WaylandManager::Get()->OnSeatKeyboardCapability();
  } else if (!has_keyboard && wayland_seat->wayland_keyboard_) {
    WaylandManager::Get()->OnSeatKeyboardCapabilityRevoked();
    wayland_seat->wayland_keyboard_.reset();
  }

  if (has_pointer) {
    WaylandManager::Get()->OnSeatPointerCapability();
  } else {
    WaylandManager::Get()->OnSeatPointerCapabilityRevoked();
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

void WaylandSeat::SetSeatPresentCallback(
    WaylandSeat::OnSeatPresentCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seat_present_callback_ = std::move(callback);
  if (seat_id_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(seat_present_callback_));
  }
}

}  // namespace remoting
