// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_PUBLIC_MOJOM_GAMEPAD_HARDWARE_BUFFER_H_
#define DEVICE_GAMEPAD_PUBLIC_MOJOM_GAMEPAD_HARDWARE_BUFFER_H_

#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace device {

typedef SharedMemorySeqLockBuffer<Gamepads> GamepadHardwareBuffer;

}  // namespace device

#endif  // DEVICE_GAMEPAD_PUBLIC_MOJOM_GAMEPAD_HARDWARE_BUFFER_H_
