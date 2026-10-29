// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_MAC_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_MAC_H_

#include <CoreGraphics/CGEventTypes.h>

#include <cstdint>
#include <optional>

namespace remoting {

// Inspects a CGEvent from the session keyboard event tap and returns its USB
// keycode if it represents genuine local keyboard input, or std::nullopt if
// the event should be ignored (such as kCGEventFlagsChanged events or events
// injected by this CRD host process).
std::optional<uint32_t> GetLocalKeyboardEventUsbKeycode(CGEventType type,
                                                        CGEventRef event);

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_KEYBOARD_INPUT_MONITOR_MAC_H_
