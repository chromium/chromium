// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_android.h"

#include <list>
#include <memory>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/usb/usb_configuration_android.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle_android.h"
#include "services/device/usb/usb_interface_android.h"
#include "services/device/usb/usb_service_android.h"
#include "services/device/usb/webusb_descriptors.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/usb/jni_headers/ChromeUsbDevice_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaObjectArrayReader;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace device {

// static
scoped_refptr<UsbDeviceAndroid> UsbDeviceAndroid::Create(
    JNIEnv* env,
    base::WeakPtr<UsbServiceAndroid> service,
    const JavaRef<jobject>& usb_device) {
  auto* build_info = base::android::BuildInfo::GetInstance();
  ScopedJavaLocalRef<jobject> wrapper =
      Java_ChromeUsbDevice_create(env, usb_device);

  std::u16string manufacturer_string;
  ScopedJavaLocalRef<jstring> manufacturer_jstring =
      Java_ChromeUsbDevice_getManufacturerName(env, wrapper);
  if (!manufacturer_jstring.is_null())
    manufacturer_string = ConvertJavaStringToUTF16(env, manufacturer_jstring);

  std::u16string product_string;
  ScopedJavaLocalRef<jstring> product_jstring =
      Java_ChromeUsbDevice_getProductName(env, wrapper);
  if (!product_jstring.is_null())
    product_string = ConvertJavaStringToUTF16(env, product_jstring);

  // Reading the serial number requires device access permission when
  // targeting the Q SDK.
  std::u16string serial_number;
  if (service->HasDevicePermission(wrapper) ||
      build_info->sdk_int() < base::android::SDK_VERSION_Q) {
    ScopedJavaLocalRef<jstring> serial_jstring =
        Java_ChromeUsbDevice_getSerialNumber(env, wrapper);
    if (!serial_jstring.is_null())
      serial_number = ConvertJavaStringToUTF16(env, serial_jstring);
  }

  return base::WrapRefCounted(new UsbDeviceAndroid(
      env, service,
      0x0200,  // USB protocol version, not provided by the Android API.
      Java_ChromeUsbDevice_getDeviceClass(env, wrapper),
      Java_ChromeUsbDevice_getDeviceSubclass(env, wrapper),
      Java_ChromeUsbDevice_getDeviceProtocol(env, wrapper),
      Java_ChromeUsbDevice_getVendorId(env, wrapper),
      Java_ChromeUsbDevice_getProductId(env, wrapper),
      Java_ChromeUsbDevice_getDeviceVersion(env, wrapper),
      manufacturer_string, product_string, serial_number, wrapper));
}

void UsbDeviceAndroid::RequestPermission(ResultCallback callback) {
  if (!permission_granted_ && service_) {
    request_permission_callbacks_.push_back(std::move(callback));
    service_->RequestDevicePermission(j_object_);
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), permission_granted_));
  }
}

void UsbDeviceAndroid::Open(OpenCallback callback) {
  scoped_refptr<UsbDeviceHandle> device_handle;
  if (service_) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> connection =
        service_->OpenDevice(env, j_object_);
    if (!connection.is_null()) {
      device_handle = UsbDeviceHandleAndroid::Create(env, this, connection);
      handles().push_back(device_handle.get());
    }
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), device_handle));
}

bool UsbDeviceAndroid::permission_granted() const {
  return permission_granted_;
}

UsbDeviceAndroid::UsbDeviceAndroid(JNIEnv* env,
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
                                   const JavaRef<jobject>& wrapper)
    : UsbDevice(usb_version,
                device_class,
                device_subclass,
                device_protocol,
                vendor_id,
                product_id,
                device_version,
                manufacturer_string,
                product_string,
                serial_number,
                // We fake the bus and port number, because the underlying Java
                // UsbDevice class doesn't offer an interface for getting these
                // values, and nothing on Android seems to require them at this
                // time (23-Nov-2018)
                0,
                0),
      device_id_(Java_ChromeUsbDevice_getDeviceId(env, wrapper)),
      service_(service),
      j_object_(wrapper) {
  JavaObjectArrayReader<jobject> configs(
      Java_ChromeUsbDevice_getConfigurations(env, j_object_));
  device_info_->configurations.reserve(configs.size());
  for (auto config : configs) {
    device_info_->configurations.push_back(
        UsbConfigurationAndroid::Convert(env, config));
  }

  if (configurations().size() > 0)
    ActiveConfigurationChanged(configurations()[0]->configuration_value);
}

UsbDeviceAndroid::~UsbDeviceAndroid() {}

void UsbDeviceAndroid::PermissionGranted(JNIEnv* env, bool granted) {
  if (!granted) {
    CallRequestPermissionCallbacks(false);
    return;
  }

  ScopedJavaLocalRef<jstring> serial_jstring =
      Java_ChromeUsbDevice_getSerialNumber(env, j_object_);
  if (!serial_jstring.is_null())
    device_info_->serial_number = ConvertJavaStringToUTF16(env, serial_jstring);

  Open(
      base::BindOnce(&UsbDeviceAndroid::OnDeviceOpenedToReadDescriptors, this));
}

void UsbDeviceAndroid::CallRequestPermissionCallbacks(bool granted) {
  permission_granted_ = granted;
  std::list<ResultCallback> callbacks;
  callbacks.swap(request_permission_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(granted);
}

void UsbDeviceAndroid::OnDeviceOpenedToReadDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (!device_handle) {
    CallRequestPermissionCallbacks(false);
    return;
  }

  ReadUsbDescriptors(device_handle,
                     base::BindOnce(&UsbDeviceAndroid::OnReadDescriptors, this,
                                    device_handle));
}

void UsbDeviceAndroid::OnReadDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<UsbDeviceDescriptor> descriptor) {
  if (!descriptor) {
    device_handle->Close();
    CallRequestPermissionCallbacks(false);
    return;
  }

  // ReadUsbDescriptors() is called to read properties which aren't always
  // available from the Android OS. The other properties should be left alone.
  device_info_->usb_version_major = descriptor->device_info->usb_version_major;
  device_info_->usb_version_minor = descriptor->device_info->usb_version_minor;
  device_info_->usb_version_subminor =
      descriptor->device_info->usb_version_subminor;
  device_info_->configurations =
      std::move(descriptor->device_info->configurations);

  if (usb_version() >= 0x0210) {
    ReadWebUsbDescriptors(
        device_handle,
        base::BindOnce(&UsbDeviceAndroid::OnReadWebUsbDescriptors, this,
                       device_handle));
  } else {
    device_handle->Close();
    CallRequestPermissionCallbacks(true);
  }
}

void UsbDeviceAndroid::OnReadWebUsbDescriptors(
    scoped_refptr<UsbDeviceHandle> device_handle,
    const GURL& landing_page) {
  if (landing_page.is_valid())
    device_info_->webusb_landing_page = landing_page;

  device_handle->Close();
  CallRequestPermissionCallbacks(true);
}

}  // namespace device
