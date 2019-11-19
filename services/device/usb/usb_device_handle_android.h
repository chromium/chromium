// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_ANDROID_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "services/device/usb/usb_device_handle_usbfs.h"

namespace device {

class UsbDevice;

// Extends UsbDeviceHandleUsbfs with support for managing a device connection
// through an instance of android.hardware.usb.UsbDeviceConnection.
class UsbDeviceHandleAndroid : public UsbDeviceHandleUsbfs {
 public:
  static scoped_refptr<UsbDeviceHandleAndroid> Create(
      JNIEnv* env,
      scoped_refptr<UsbDevice> device,
      const base::android::JavaRef<jobject>& usb_connection);

 private:
  // |wrapper| is an instance of org.chromium.device.usb.ChromeUsbConnection.
  UsbDeviceHandleAndroid(scoped_refptr<UsbDevice> device,
                         base::ScopedFD fd,
                         const base::android::JavaRef<jobject>& wrapper);
  ~UsbDeviceHandleAndroid() override;

  // UsbDeviceHandleUsbfs:
  void CloseBlocking() override;

  void CloseConnection();

  // Java object org.chromium.device.usb.ChromeUsbConnection.
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_ANDROID_H_
