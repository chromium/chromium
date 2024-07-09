// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_EMULATION_FAKE_READ_RESPONSE_H_
#define DEVICE_BLUETOOTH_EMULATION_FAKE_READ_RESPONSE_H_

#include <cstdint>
#include <optional>
#include <vector>

namespace bluetooth {

// Holds the necessary values for dispatching a fake read response.
//
// Not intended for direct use by clients.  See README.md.
class FakeReadResponse {
 public:
  FakeReadResponse(uint16_t gatt_code,
                   const std::optional<std::vector<uint8_t>>& value);

  FakeReadResponse(const FakeReadResponse&) = delete;
  FakeReadResponse& operator=(const FakeReadResponse&) = delete;

  ~FakeReadResponse();

  uint16_t gatt_code() { return gatt_code_; }
  const std::optional<std::vector<uint8_t>>& value() { return value_; }

 private:
  uint16_t gatt_code_;
  std::optional<std::vector<uint8_t>> value_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_EMULATION_FAKE_READ_RESPONSE_H_
