// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_android.h"

#include "base/bind.h"
#include "base/location.h"
#include "services/device/usb/jni_headers/ChromeUsbConnection_jni.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"

using base::android::ScopedJavaLocalRef;

namespace device {

// static
scoped_refptr<UsbDeviceHandleAndroid> UsbDeviceHandleAndroid::Create(
    JNIEnv* env,
    scoped_refptr<UsbDevice> device,
    const base::android::JavaRef<jobject>& usb_connection) {
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbConnection_create(env, usb_connection);
  // C++ doesn't own this file descriptor so CloseBlocking() is overridden
  // below to release it without closing it.
  base::ScopedFD fd(Java_ChromeUsbConnection_getFileDescriptor(env, wrapper));
  return base::WrapRefCounted(
      new UsbDeviceHandleAndroid(device, std::move(fd), wrapper));
}

UsbDeviceHandleAndroid::UsbDeviceHandleAndroid(
    scoped_refptr<UsbDevice> device,
    base::ScopedFD fd,
    const base::android::JavaRef<jobject>& wrapper)
    : UsbDeviceHandleUsbfs(device,
                           std::move(fd),
                           UsbService::CreateBlockingTaskRunner()),
      j_object_(wrapper) {}

UsbDeviceHandleAndroid::~UsbDeviceHandleAndroid() {}

void UsbDeviceHandleAndroid::CloseBlocking() {
  ReleaseFileDescriptor();
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UsbDeviceHandleAndroid::CloseConnection, this));
}

void UsbDeviceHandleAndroid::CloseConnection() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeUsbConnection_close(env, j_object_);
  j_object_.Reset();
}

}  // namespace device
