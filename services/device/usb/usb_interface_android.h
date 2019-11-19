// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_INTERFACE_ANDROID_H_
#define SERVICES_DEVICE_USB_USB_INTERFACE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "services/device/usb/usb_descriptors.h"

namespace device {

class UsbInterfaceAndroid {
 public:
  static mojom::UsbInterfaceInfoPtr Convert(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& usb_interface);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_INTERFACE_ANDROID_H_
