// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_ANDROID_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_ANDROID_H_

#include <jni.h>

#include <unordered_map>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/usb/usb_service.h"

namespace device {

class UsbDeviceAndroid;

// USB service implementation for Android.
class UsbServiceAndroid final : public UsbService {
 public:
  class JniDelegate;

  UsbServiceAndroid();
  ~UsbServiceAndroid() override;

  // Methods called by Java.
  void DeviceAttached(JNIEnv* env,
                      const base::android::JavaRef<jobject>& usb_device);
  void DeviceDetached(JNIEnv* env, int32_t device_id);
  void DevicePermissionRequestComplete(JNIEnv* env,
                                       int32_t device_id,
                                       bool granted);

  // Called by UsbDeviceAndroid.
  base::android::ScopedJavaLocalRef<jobject> OpenDevice(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& wrapper);
  bool HasDevicePermission(const base::android::JavaRef<jobject>& wrapper);
  void RequestDevicePermission(const base::android::JavaRef<jobject>& wrapper);

 private:
  void AddDevice(scoped_refptr<UsbDeviceAndroid> device);

  // Methods executed on the sequenced |task_runner_|.
  void DeviceAttachedInternal(
      const base::android::ScopedJavaGlobalRef<jobject>& usb_device);
  void DeviceDetachedInternal(int32_t device_id);
  void DevicePermissionRequestCompleteInternal(int32_t device_id, bool granted);

  std::unordered_map<int32_t, scoped_refptr<UsbDeviceAndroid>> devices_by_id_;

  // Java object org.chromium.device.usb.ChromeUsbService.
  base::android::ScopedJavaGlobalRef<jobject> j_object_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<JniDelegate> jni_delegate_;

  base::WeakPtrFactory<UsbServiceAndroid> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_ANDROID_H_
