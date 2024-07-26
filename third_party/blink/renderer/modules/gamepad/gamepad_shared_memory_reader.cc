// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/gamepad_shared_memory_reader.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_listener.h"

namespace blink {

GamepadSharedMemoryReader::GamepadSharedMemoryReader(LocalDOMWindow& window)
    : receiver_(this, &window), gamepad_monitor_remote_(&window) {
  // See https://bit.ly/2S0zRAS for task types
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      window.GetTaskRunner(TaskType::kMiscPlatformAPI);
  window.GetBrowserInterfaceBroker().GetInterface(
      gamepad_monitor_remote_.BindNewPipeAndPassReceiver(task_runner));
  gamepad_monitor_remote_->SetObserver(
      receiver_.BindNewPipeAndPassRemote(task_runner));
}

void GamepadSharedMemoryReader::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(gamepad_monitor_remote_);
}

void GamepadSharedMemoryReader::SendStartMessage() {
  if (gamepad_monitor_remote_.is_bound()) {
    gamepad_monitor_remote_->GamepadStartPolling(
        &renderer_shared_buffer_region_);
  }
}

void GamepadSharedMemoryReader::SendStopMessage() {
  if (gamepad_monitor_remote_.is_bound()) {
    gamepad_monitor_remote_->GamepadStopPolling();
  }
}

void GamepadSharedMemoryReader::Start(blink::GamepadListener* listener) {
  DCHECK(!listener_);
  listener_ = listener;

  SendStartMessage();

  // If we don't get a valid handle from the browser, don't try to Map (we're
  // probably out of memory or file handles).
  bool is_valid = renderer_shared_buffer_region_.IsValid();
  UMA_HISTOGRAM_BOOLEAN("Gamepad.ValidSharedMemoryHandle", is_valid);

  if (!is_valid)
    return;

  renderer_shared_buffer_mapping_ = renderer_shared_buffer_region_.Map();
  CHECK(renderer_shared_buffer_mapping_.IsValid());
  gamepad_hardware_buffer_ = renderer_shared_buffer_mapping_
                                 .GetMemoryAs<device::GamepadHardwareBuffer>();
  CHECK(gamepad_hardware_buffer_);
}

void GamepadSharedMemoryReader::Stop() {
  DCHECK(listener_);
  listener_ = nullptr;
  renderer_shared_buffer_region_ = base::ReadOnlySharedMemoryRegion();
  renderer_shared_buffer_mapping_ = base::ReadOnlySharedMemoryMapping();
  gamepad_hardware_buffer_ = nullptr;

  SendStopMessage();
}

void GamepadSharedMemoryReader::SampleGamepads(device::Gamepads* gamepads) {
  // Blink should have started observing at this point.
  CHECK(listener_);

  // ==========
  //   DANGER
  // ==========
  //
  // This logic is duplicated in Pepper as well. If you change it, that also
  // needs to be in sync. See ppapi/proxy/gamepad_resource.cc.
  device::Gamepads read_into;
  TRACE_EVENT0("GAMEPAD", "SampleGamepads");

  if (!renderer_shared_buffer_region_.IsValid())
    return;

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer. TODO(scottmg) Tune this
  // number (as low as 1?) if histogram shows distribution as mostly
  // 0-and-maximum.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  do {
    version = gamepad_hardware_buffer_->seqlock.ReadBegin();
    memcpy(&read_into, &gamepad_hardware_buffer_->data, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (gamepad_hardware_buffer_->seqlock.ReadRetry(version));
  UMA_HISTOGRAM_COUNTS_1M("Gamepad.ReadContentionCount", contention_count);

  if (contention_count >= kMaximumContentionCount) {
    // We failed to successfully read, presumably because the hardware
    // thread was taking unusually long. Don't copy the data to the output
    // buffer, and simply leave what was there before.
    return;
  }

  // New data was read successfully, copy it into the output buffer.
  memcpy(gamepads, &read_into, sizeof(*gamepads));

  if (!ever_interacted_with_) {
    // Clear the connected flag if the user hasn't interacted with any of the
    // gamepads to prevent fingerprinting. The actual data is not cleared.
    // WebKit will only copy out data into the JS buffers for connected
    // gamepads so this is sufficient.
    for (auto& item : gamepads->items) {
      item.connected = false;
    }
  }
}

GamepadSharedMemoryReader::~GamepadSharedMemoryReader() {
  if (listener_)
    Stop();
}

void GamepadSharedMemoryReader::GamepadConnected(
    uint32_t index,
    const device::Gamepad& gamepad) {
  // The browser already checks if the user actually interacted with a device.
  ever_interacted_with_ = true;

  if (listener_)
    listener_->DidConnectGamepad(index, gamepad);
}

void GamepadSharedMemoryReader::GamepadDisconnected(
    uint32_t index,
    const device::Gamepad& gamepad) {
  if (listener_)
    listener_->DidDisconnectGamepad(index, gamepad);
}

void GamepadSharedMemoryReader::GamepadChanged(
    device::mojom::blink::GamepadChangesPtr change) {
  // TODO(crbug.com/856290): use these calls to Generate Button Event.
}

}  // namespace blink
