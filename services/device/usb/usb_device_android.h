// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_ANDROID_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_ANDROID_H_

#include <list>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "services/device/usb/usb_device.h"

namespace device {

class UsbServiceAndroid;

class UsbDeviceAndroid : public UsbDevice {
 public:
  static scoped_refptr<UsbDeviceAndroid> Create(
      JNIEnv* env,
      base::WeakPtr<UsbServiceAndroid> service,
      const base::android::JavaRef<jobject>& usb_device);

  // UsbDevice:
  void RequestPermission(ResultCallback callback) override;
  bool permission_granted() const override;
  void Open(OpenCallback callback) override;

  jint device_id() const { return device_id_; }
  void PermissionGranted(JNIEnv* env, bool granted);

 private:
  UsbDeviceAndroid(JNIEnv* env,
                   base::WeakPtr<UsbServiceAndroid> service,
                   uint16_t usb_version,
                   uint8_t device_class,
                   uint8_t device_subclass,
                   uint8_t device_protocol,
                   uint16_t vendor_id,
                   uint16_t product_id,
                   uint16_t device_version,
                   const std::u16string& manufacturer_string,
                   const std::u16string& product_string,
                   const std::u16string& serial_number,
                   const base::android::JavaRef<jobject>& wrapper);
  ~UsbDeviceAndroid() override;

  void CallRequestPermissionCallbacks(bool granted);
  void OnDeviceOpenedToReadDescriptors(
      scoped_refptr<UsbDeviceHandle> device_handle);
  void OnReadDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                         std::unique_ptr<UsbDeviceDescriptor> descriptor);
  void OnReadWebUsbDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                               const GURL& landing_page);

  const jint device_id_;
  bool permission_granted_ = false;
  std::list<ResultCallback> request_permission_callbacks_;
  base::WeakPtr<UsbServiceAndroid> service_;

  // Java object org.chromium.device.usb.ChromeUsbDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_ANDROID_H_
