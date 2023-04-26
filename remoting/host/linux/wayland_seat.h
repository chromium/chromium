// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_SEAT_H_
#define REMOTING_HOST_LINUX_WAYLAND_SEAT_H_

#include <wayland-client.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/wayland_keyboard.h"

namespace remoting {

class WaylandSeat {
 public:
  using OnSeatPresentCallback = base::OnceCallback<void()>;

  WaylandSeat();
  ~WaylandSeat();

  WaylandSeat(const WaylandSeat&) = delete;
  WaylandSeat& operator=(const WaylandSeat&) = delete;

  void HandleGlobalSeatEvent(struct wl_registry* registry,
                             uint32_t name,
                             const char* interface,
                             uint32_t version);

  void HandleGlobalRemoveSeatEvent(uint32_t name);

  // Calling this when a seat is not available is an error.
  uint32_t GetSeatId() const;

  void SetSeatPresentCallback(OnSeatPresentCallback callback);

 private:
  static void OnSeatCapabilitiesEvent(void* data,
                                      struct wl_seat* wl_seat,
                                      uint32_t capabilities);

  static void OnSeatNameEvent(void* data,
                              struct wl_seat* wl_seat,
                              const char* name);

  SEQUENCE_CHECKER(sequence_checker_);

  const struct wl_seat_listener wl_seat_listener_ {
    .capabilities = OnSeatCapabilitiesEvent, .name = OnSeatNameEvent
  };
  raw_ptr<struct wl_seat> wl_seat_ = nullptr;
  uint32_t seat_id_ = 0;
  std::unique_ptr<WaylandKeyboard> wayland_keyboard_;
  OnSeatPresentCallback seat_present_callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_SEAT_H_
