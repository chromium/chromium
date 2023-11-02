// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ANDROID_WRAPPERS_H_
#define DEVICE_BLUETOOTH_ANDROID_WRAPPERS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// Bindings into Java methods in org.chromium.device.bluetooth.Wrappers classes:

// Calls Java: BluetoothAdapterWrapper.createWithDefaultAdapter().
DEVICE_BLUETOOTH_EXPORT base::android::ScopedJavaLocalRef<jobject>
BluetoothAdapterWrapper_CreateWithDefaultAdapter();

}  // namespace device

#endif  // DEVICE_BLUETOOTH_ANDROID_WRAPPERS_H_
