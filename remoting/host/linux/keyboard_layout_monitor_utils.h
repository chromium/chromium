// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_UTILS_H_
#define REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_UTILS_H_

#include <memory>

#include <gdk/gdk.h>
#include <xkbcommon/xkbcommon.h>

#include "remoting/proto/control.pb.h"

namespace remoting {

struct XkbKeyMapDeleter {
  void operator()(struct xkb_keymap* xkb_keymap) {
    xkb_keymap_unref(xkb_keymap);
  }
};

using XkbKeyMapUniquePtr = std::unique_ptr<struct xkb_keymap, XkbKeyMapDeleter>;

const char* DeadKeyToUtf8String(guint keyval);

// Convert from a GDK or XKB keysym to the corresponding LayoutKeyFunction, if
// there is one. Note that, although they are not documented as such, the two
// sets of keysyms are compatible, at least for these keys.
protocol::LayoutKeyFunction KeyvalToFunction(guint keyval);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_KEYBOARD_LAYOUT_MONITOR_UTILS_H_
