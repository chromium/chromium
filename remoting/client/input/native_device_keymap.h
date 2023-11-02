// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_NATIVE_DEVICE_KEYMAP_H_
#define REMOTING_CLIENT_INPUT_NATIVE_DEVICE_KEYMAP_H_

#include <stddef.h>
#include <stdint.h>

namespace remoting {

uint32_t NativeDeviceKeycodeToUsbKeycode(size_t device_keycode);

}  // namespace remoting

#endif  // REMOTING_CLIENT_INPUT_NATIVE_DEVICE_KEYMAP_H_
