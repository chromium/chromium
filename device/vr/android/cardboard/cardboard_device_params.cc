// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_device_params.h"

#include <stdint.h>
#include <cstdint>
#include <utility>

#include "base/notreached.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

namespace device {
// static
CardboardDeviceParams CardboardDeviceParams::GetV1DeviceParams() {
  CardboardDeviceParams params;
  uint8_t* device_params = nullptr;
  CardboardQrCode_getCardboardV1DeviceParams(&device_params, &params.size_);

  params.encoded_device_params_ = device_params;
  return params;
}

CardboardDeviceParams::CardboardDeviceParams() = default;
CardboardDeviceParams::~CardboardDeviceParams() = default;

CardboardDeviceParams::CardboardDeviceParams(CardboardDeviceParams&& other)
    : encoded_device_params_(std::move(other.encoded_device_params_)),
      size_(other.size_) {
  other.size_ = 0;
}

CardboardDeviceParams& CardboardDeviceParams::operator=(
    CardboardDeviceParams&& other) {
  std::swap(encoded_device_params_, other.encoded_device_params_);
  std::swap(size_, other.size_);

  return *this;
}

bool CardboardDeviceParams::IsValid() {
  return size_ != 0;
}

const uint8_t* CardboardDeviceParams::encoded_device_params() {
  if (absl::holds_alternative<uint8_t*>(encoded_device_params_)) {
    return absl::get<uint8_t*>(encoded_device_params_);
  } else if (absl::holds_alternative<OwnedCardboardParams>(
                 encoded_device_params_)) {
    return absl::get<OwnedCardboardParams>(encoded_device_params_).get();
  }

  NOTREACHED_NORETURN();
}
}  // namespace device
