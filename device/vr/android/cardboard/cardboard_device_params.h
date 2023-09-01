// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PARAMS_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PARAMS_H_

#include <stdint.h>

#include "device/vr/android/cardboard/scoped_cardboard_objects.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#include "base/component_export.h"

namespace device {

using OwnedCardboardParams = internal::ScopedCardboardObject<uint8_t*>;

// While we have a cardboard-specific generalization for the uint8_t* type used
// by the encoded device params; they also have a size object that needs to be
// passed alongside them and certain constructors return us static (i.e.
// unowned) device params that should not be stored in the ScopedGeneric
// specialization because they should not be deleted. This class serves as both
// a wrapper for the device params and accompanying sizes and a place to expose
// factories which can internally decide if the stored params are owned or not
// without needing to pass that information beyond this class.
class COMPONENT_EXPORT(VR_CARDBOARD) CardboardDeviceParams {
 public:
  static CardboardDeviceParams GetDeviceParams();
  static CardboardDeviceParams GetSavedDeviceParams();
  static CardboardDeviceParams GetCardboardV1DeviceParams();

  static void set_use_cardboard_v1_device_params_for_testing(bool value);

  ~CardboardDeviceParams();

  CardboardDeviceParams(const CardboardDeviceParams&) = delete;
  CardboardDeviceParams& operator=(const CardboardDeviceParams&) = delete;

  CardboardDeviceParams(CardboardDeviceParams&& other);
  CardboardDeviceParams& operator=(CardboardDeviceParams&& other);

  bool IsValid();

  const uint8_t* encoded_device_params();
  int size() { return size_; }

 private:
  CardboardDeviceParams();

  // This flag forces to always return Cardboard Viewer v1 device parameters to
  // prevent any disk read or write and the QR code scanner activity to be
  // launched. Meant to be used for testing purposes only.
  static bool use_cardboard_v1_device_params_for_testing_;

  absl::variant<uint8_t*, OwnedCardboardParams> encoded_device_params_ =
      nullptr;
  int size_ = 0;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_PARAMS_H_
