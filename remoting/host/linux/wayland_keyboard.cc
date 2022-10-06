// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_keyboard.h"

#include <sys/mman.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/wayland_manager.h"

namespace remoting {

WaylandKeyboard::WaylandKeyboard(struct wl_seat* wl_seat)
    : wl_seat_(wl_seat), wl_keyboard_(wl_seat_get_keyboard(wl_seat_.get())) {
  wl_keyboard_add_listener(wl_keyboard_, &wl_keyboard_listener_, this);
}

WaylandKeyboard::~WaylandKeyboard() {
  wl_keyboard_release(wl_keyboard_.get());
  wl_keyboard_ = nullptr;
  xkb_context_unref(xkb_context_.get());
}

// static
void WaylandKeyboard::OnKeyEvent(void* data,
                                 struct wl_keyboard* wl_keyboard,
                                 uint32_t serial,
                                 uint32_t time,
                                 uint32_t key,
                                 uint32_t state) {
  NOTIMPLEMENTED();
}

// static
void WaylandKeyboard::OnKeyMapEvent(void* data,
                                    struct wl_keyboard* wl_keyboard,
                                    uint32_t format,
                                    int32_t fd,
                                    uint32_t size) {
  WaylandKeyboard* wayland_keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(wayland_keyboard);
  DCHECK_CALLED_ON_VALID_SEQUENCE(wayland_keyboard->sequence_checker_);

  DCHECK(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
      << "Unexpected keymap format";

  char* mmapped_keymap =
      static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));

  if (mmapped_keymap == MAP_FAILED) {
    LOG(ERROR) << "Failed to mmap keymap";
    return;
  }

  XkbKeyMapUniquePtr xkb_keymap;
  xkb_keymap.reset(xkb_keymap_new_from_string(
      wayland_keyboard->xkb_context_.get(), mmapped_keymap,
      XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS));

  munmap(mmapped_keymap, size);
  WaylandManager::Get()->OnKeyboardLayout(std::move(xkb_keymap));
}

// static
void WaylandKeyboard::OnKeyboardEnterEvent(void* data,
                                           struct wl_keyboard* wl_keyboard,
                                           uint32_t serial,
                                           struct wl_surface* surface,
                                           struct wl_array* keys) {
  // We don't have a surface associated so the keyboard will not enter
  // any surface. We provide the event handler in any case since otherwise
  // the Wayland compositor might crash.
  NOTIMPLEMENTED();
}

// static
void WaylandKeyboard::OnKeyboardLeaveEvent(void* data,
                                           struct wl_keyboard* wl_keyboard,
                                           uint32_t serial,
                                           struct wl_surface* surface) {
  NOTIMPLEMENTED();
}

// static
void WaylandKeyboard::OnModifiersEvent(void* data,
                                       struct wl_keyboard* wl_keyboard,
                                       uint32_t serial,
                                       uint32_t mods_depressed,
                                       uint32_t mods_latched,
                                       uint32_t mods_locked,
                                       uint32_t group) {
  WaylandManager::Get()->OnKeyboardModifiers(group);
}

// static
void WaylandKeyboard::OnRepeatInfoEvent(void* data,
                                        struct wl_keyboard* wl_keyboard,
                                        int32_t rate,
                                        int32_t delay) {
  NOTIMPLEMENTED();
}

}  // namespace remoting
