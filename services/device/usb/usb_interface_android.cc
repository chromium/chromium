// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_interface_android.h"

#include "services/device/usb/usb_endpoint_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/usb/jni_headers/ChromeUsbInterface_jni.h"

using base::android::ScopedJavaLocalRef;

namespace device {

// static
mojom::UsbInterfaceInfoPtr UsbInterfaceAndroid::Convert(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& usb_interface) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbInterface_create(env, usb_interface);

  uint8_t alternate_setting =
      Java_ChromeUsbInterface_getAlternateSetting(env, wrapper);

  auto interface = BuildUsbInterfaceInfoPtr(
      Java_ChromeUsbInterface_getInterfaceNumber(env, wrapper),
      alternate_setting,
      Java_ChromeUsbInterface_getInterfaceClass(env, wrapper),
      Java_ChromeUsbInterface_getInterfaceSubclass(env, wrapper),
      Java_ChromeUsbInterface_getInterfaceProtocol(env, wrapper));

  base::android::JavaObjectArrayReader<jobject> endpoints(
      Java_ChromeUsbInterface_getEndpoints(env, wrapper));
  interface->alternates[0]->endpoints.reserve(endpoints.size());
  for (auto endpoint : endpoints) {
    interface->alternates[0]->endpoints.push_back(
        UsbEndpointAndroid::Convert(env, endpoint));
  }

  return interface;
}

}  // namespace device
