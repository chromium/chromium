// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_KEYBOARD_H_
#define REMOTING_HOST_LINUX_WAYLAND_KEYBOARD_H_

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"

namespace remoting {

class WaylandKeyboard {
 public:
  // |wl_seat| must outlive this class.
  explicit WaylandKeyboard(struct wl_seat* wl_seat);
  WaylandKeyboard(const WaylandKeyboard&) = delete;
  WaylandKeyboard& operator=(const WaylandKeyboard&) = delete;

  ~WaylandKeyboard();

 private:
  static void OnKeyEvent(void* data,
                         struct wl_keyboard* wl_keyboard,
                         uint32_t serial,
                         uint32_t time,
                         uint32_t key,
                         uint32_t state);
  static void OnKeyMapEvent(void* data,
                            struct wl_keyboard* wl_keyboard,
                            uint32_t format,
                            int32_t fd,
                            uint32_t size);
  static void OnKeyboardEnterEvent(void* data,
                                   struct wl_keyboard* wl_keyboard,
                                   uint32_t serial,
                                   struct wl_surface* surface,
                                   struct wl_array* keys);
  static void OnKeyboardLeaveEvent(void* data,
                                   struct wl_keyboard* wl_keyboard,
                                   uint32_t serial,
                                   struct wl_surface* surface);
  static void OnModifiersEvent(void* data,
                               struct wl_keyboard* wl_keyboard,
                               uint32_t serial,
                               uint32_t mods_depressed,
                               uint32_t mods_latched,
                               uint32_t mods_locked,
                               uint32_t group);
  static void OnRepeatInfoEvent(void* data,
                                struct wl_keyboard* wl_keyboard,
                                int32_t rate,
                                int32_t delay);

  const struct wl_keyboard_listener wl_keyboard_listener_ = {
      .keymap = OnKeyMapEvent,
      .enter = OnKeyboardEnterEvent,
      .leave = OnKeyboardLeaveEvent,
      .key = OnKeyEvent,
      .modifiers = OnModifiersEvent,
      .repeat_info = OnRepeatInfoEvent,
  };

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<struct wl_seat> wl_seat_ = nullptr;
  raw_ptr<struct wl_keyboard> wl_keyboard_ = nullptr;
  raw_ptr<struct xkb_context> xkb_context_ =
      xkb_context_new(XKB_CONTEXT_NO_FLAGS);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_KEYBOARD_H_
