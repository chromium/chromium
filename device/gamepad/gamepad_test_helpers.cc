// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_test_helpers.h"

namespace device {

MockGamepadDataFetcher::MockGamepadDataFetcher(const Gamepads& test_data)
    : test_data_(test_data),
      read_data_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                 base::WaitableEvent::InitialState::NOT_SIGNALED) {}

MockGamepadDataFetcher::~MockGamepadDataFetcher() = default;

GamepadSource MockGamepadDataFetcher::source() {
  return GamepadSource::kTest;
}

void MockGamepadDataFetcher::GetGamepadData(bool devices_changed_hint) {
  {
    base::AutoLock lock(lock_);

    for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
      if (test_data_.items[i].connected) {
        PadState* pad = GetPadState(i);
        if (pad)
          memcpy(&pad->data, &test_data_.items[i], sizeof(Gamepad));
      }
    }
  }
  read_data_.Signal();
}

void MockGamepadDataFetcher::WaitForDataRead() {
  return read_data_.Wait();
}

void MockGamepadDataFetcher::WaitForDataReadAndCallbacksIssued() {
  // The provider will read the data on the background thread (setting the
  // event) and *then* will issue the callback on the client thread. Waiting for
  // it to read twice is a simple way to ensure that it was able to issue
  // callbacks for the first read (if it issued one).
  WaitForDataRead();
  WaitForDataRead();
}

void MockGamepadDataFetcher::SetTestData(const Gamepads& new_data) {
  base::AutoLock lock(lock_);
  test_data_ = new_data;
}

GamepadTestHelper::GamepadTestHelper() = default;

GamepadTestHelper::~GamepadTestHelper() = default;

GamepadServiceTestConstructor::GamepadServiceTestConstructor(
    const Gamepads& test_data) {
  data_fetcher_ = new MockGamepadDataFetcher(test_data);
  gamepad_service_ =
      new GamepadService(std::unique_ptr<GamepadDataFetcher>(data_fetcher_));
}

GamepadServiceTestConstructor::~GamepadServiceTestConstructor() {
  delete gamepad_service_;
}

}  // namespace device
