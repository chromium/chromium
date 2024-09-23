// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_android.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/usb/jni_headers/ChromeUsbService_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace device {

UsbServiceAndroid::UsbServiceAndroid() : UsbService() {
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(
      Java_ChromeUsbService_create(env, reinterpret_cast<jlong>(this)));
  ScopedJavaLocalRef<jobjectArray> devices =
      Java_ChromeUsbService_getDevices(env, j_object_);
  for (auto usb_device : devices.ReadElements<jobject>()) {
    scoped_refptr<UsbDeviceAndroid> device =
        UsbDeviceAndroid::Create(env, weak_factory_.GetWeakPtr(), usb_device);
    AddDevice(device);
  }
}

UsbServiceAndroid::~UsbServiceAndroid() {
  NotifyWillDestroyUsbService();
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeUsbService_close(env, j_object_);
}

void UsbServiceAndroid::DeviceAttached(JNIEnv* env,
                                       const JavaRef<jobject>& caller,
                                       const JavaRef<jobject>& usb_device) {
  scoped_refptr<UsbDeviceAndroid> device =
      UsbDeviceAndroid::Create(env, weak_factory_.GetWeakPtr(), usb_device);
  AddDevice(device);
  NotifyDeviceAdded(device);
}

void UsbServiceAndroid::DeviceDetached(JNIEnv* env,
                                       const JavaRef<jobject>& caller,
                                       jint device_id) {
  auto it = devices_by_id_.find(device_id);
  if (it == devices_by_id_.end())
    return;

  scoped_refptr<UsbDeviceAndroid> device = it->second;
  devices_by_id_.erase(it);
  devices().erase(device->guid());
  device->OnDisconnect();

  USB_LOG(USER) << "USB device removed: id=" << device->device_id()
                << " guid=" << device->guid();

  NotifyDeviceRemoved(device);
}

void UsbServiceAndroid::DevicePermissionRequestComplete(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& caller,
    jint device_id,
    jboolean granted) {
  const auto it = devices_by_id_.find(device_id);
  if (it == devices_by_id_.end()) {
    return;
  }
  it->second->PermissionGranted(env, granted);
}

ScopedJavaLocalRef<jobject> UsbServiceAndroid::OpenDevice(
    JNIEnv* env,
    const JavaRef<jobject>& wrapper) {
  return Java_ChromeUsbService_openDevice(env, j_object_, wrapper);
}

bool UsbServiceAndroid::HasDevicePermission(const JavaRef<jobject>& wrapper) {
  return Java_ChromeUsbService_hasDevicePermission(AttachCurrentThread(),
                                                   j_object_, wrapper);
}

void UsbServiceAndroid::RequestDevicePermission(
    const JavaRef<jobject>& wrapper) {
  Java_ChromeUsbService_requestDevicePermission(AttachCurrentThread(),
                                                j_object_, wrapper);
}

void UsbServiceAndroid::AddDevice(scoped_refptr<UsbDeviceAndroid> device) {
  DCHECK(!base::Contains(devices_by_id_, device->device_id()));
  DCHECK(!base::Contains(devices(), device->guid()));
  devices_by_id_[device->device_id()] = device;
  devices()[device->guid()] = device;

  USB_LOG(USER) << "USB device added: id=" << device->device_id()
                << " vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();
}

}  // namespace device
