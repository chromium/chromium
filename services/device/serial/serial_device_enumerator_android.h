// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_

#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// A placeholder implementation of a wired serial device enumerator for Android.
class SerialDeviceEnumeratorAndroid : public SerialDeviceEnumerator {
 public:
  SerialDeviceEnumeratorAndroid();

  SerialDeviceEnumeratorAndroid(const SerialDeviceEnumeratorAndroid&) = delete;
  SerialDeviceEnumeratorAndroid& operator=(
      const SerialDeviceEnumeratorAndroid&) = delete;
};
}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_ANDROID_H_
