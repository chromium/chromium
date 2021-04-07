// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_SHARED_BUFFER_H_
#define DEVICE_GAMEPAD_SHARED_BUFFER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "device/gamepad/gamepad_export.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"

namespace device {

/*

 GamepadHardwareBuffer is stored in shared memory that's shared between the
 browser which does the hardware polling, and the various consumers of the
 gamepad state (renderers and NaCl plugins). The performance characteristics are
 that we want low latency (so would like to avoid explicit communication via IPC
 between producer and consumer) and relatively large data size.

 Writer and reader operate on the same buffer assuming contention is low, and
 contention is detected by using the associated SeqLock.

*/

class DEVICE_GAMEPAD_EXPORT GamepadSharedBuffer {
 public:
  GamepadSharedBuffer();
  ~GamepadSharedBuffer();

  base::ReadOnlySharedMemoryRegion DuplicateSharedMemoryRegion();
  Gamepads* buffer();
  GamepadHardwareBuffer* hardware_buffer();

  void WriteBegin();
  void WriteEnd();

 private:
  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;
  GamepadHardwareBuffer* hardware_buffer_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_SHARED_BUFFER_H_
