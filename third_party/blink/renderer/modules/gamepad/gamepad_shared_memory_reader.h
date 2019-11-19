// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_SHARED_MEMORY_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_SHARED_MEMORY_READER_H_

#include <memory>

#include "base/macros.h"
#include "device/gamepad/public/mojom/gamepad.mojom-blink.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}

namespace device {
class Gamepad;
class Gamepads;
}  // namespace device

namespace blink {

class GamepadListener;
class LocalFrame;

class GamepadSharedMemoryReader : public device::mojom::blink::GamepadObserver {
 public:
  explicit GamepadSharedMemoryReader(LocalFrame& frame);
  ~GamepadSharedMemoryReader() override;

  void SampleGamepads(device::Gamepads& gamepads);
  void Start(blink::GamepadListener* listener);
  void Stop();

 protected:
  void SendStartMessage();
  void SendStopMessage();

 private:
  // device::mojom::blink::GamepadObserver methods.
  void GamepadConnected(uint32_t index,
                        const device::Gamepad& gamepad) override;
  void GamepadDisconnected(uint32_t index,
                           const device::Gamepad& gamepad) override;
  void GamepadButtonOrAxisChanged(uint32_t index,
                                  const device::Gamepad& gamepad) override;

  base::ReadOnlySharedMemoryRegion renderer_shared_buffer_region_;
  base::ReadOnlySharedMemoryMapping renderer_shared_buffer_mapping_;
  const device::GamepadHardwareBuffer* gamepad_hardware_buffer_ = nullptr;

  bool ever_interacted_with_ = false;

  mojo::Receiver<device::mojom::blink::GamepadObserver> receiver_{this};
  mojo::Remote<device::mojom::blink::GamepadMonitor> gamepad_monitor_remote_;
  blink::GamepadListener* listener_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GamepadSharedMemoryReader);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_SHARED_MEMORY_READER_H_
