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
bool CardboardDeviceParams::use_cardboard_v1_device_params_for_testing_ = false;

// static
CardboardDeviceParams CardboardDeviceParams::GetDeviceParams() {
  if (use_cardboard_v1_device_params_for_testing_) {
    return GetCardboardV1DeviceParams();
  }

  CardboardDeviceParams params = GetSavedDeviceParams();

  // If no saved device params were returned, use the default V1 device
  // parameters as a fallback.
  if (!params.IsValid()) {
    params = GetCardboardV1DeviceParams();
  }

  return params;
}

// static
CardboardDeviceParams CardboardDeviceParams::GetSavedDeviceParams() {
  if (use_cardboard_v1_device_params_for_testing_) {
    return GetCardboardV1DeviceParams();
  }

  // Check if any device parameters have been saved.
  uint8_t* device_params = nullptr;
  int size = 0;
  CardboardQrCode_getSavedDeviceParams(&device_params, &size);
  if (size != 0) {
    // If saved device params were returned, store them as owned parameters so
    // them get cleaned up properly.
    CardboardDeviceParams params;
    params.encoded_device_params_ = OwnedCardboardParams(device_params);
    params.size_ = size;
    return params;
  }

  // If no saved device params were returned, return an empty object.
  return CardboardDeviceParams();
}

// static
CardboardDeviceParams CardboardDeviceParams::GetCardboardV1DeviceParams() {
  uint8_t* device_params = nullptr;
  int size = 0;
  CardboardQrCode_getCardboardV1DeviceParams(&device_params, &size);

  // Cardboard Viewer v1 device parameters don't need to be cleaned up (they are
  // statically allocated in memory).
  CardboardDeviceParams params;
  params.encoded_device_params_ = device_params;
  params.size_ = size;
  return params;
}

// static
void CardboardDeviceParams::set_use_cardboard_v1_device_params_for_testing(
    bool value) {
  use_cardboard_v1_device_params_for_testing_ = value;
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

  NOTREACHED();
}
}  // namespace device
