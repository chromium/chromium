// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_android.h"

#include <string>
#include <vector>

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

// Bounces JNI callbacks to `task_runner_` (the service sequence). Holds a weak
// reference to the service since it may be destroyed. The weak pointer must be
// checked on the service sequence.
class UsbServiceAndroid::JniDelegate
    : public base::RefCountedThreadSafe<JniDelegate> {
 public:
  explicit JniDelegate(base::WeakPtr<UsbServiceAndroid> service)
      : service_(std::move(service)),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  void DeviceAttached(JNIEnv* env,
                      const base::android::JavaRef<jobject>& usb_device) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&JniDelegate::DeviceAttachedInternal, this,
                                  base::android::ScopedJavaGlobalRef<jobject>(
                                      env, usb_device)));
  }

  void DeviceDetached(int32_t device_id) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&JniDelegate::DeviceDetachedInternal, this, device_id));
  }

  void DevicePermissionRequestComplete(int32_t device_id, bool granted) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&JniDelegate::DevicePermissionRequestCompleteInternal,
                       this, device_id, granted));
  }

 private:
  friend class base::RefCountedThreadSafe<JniDelegate>;
  ~JniDelegate() = default;

  void DeviceAttachedInternal(
      base::android::ScopedJavaGlobalRef<jobject> usb_device) {
    if (service_) {
      service_->DeviceAttachedInternal(usb_device);
    }
  }

  void DeviceDetachedInternal(int32_t device_id) {
    if (service_) {
      service_->DeviceDetachedInternal(device_id);
    }
  }

  void DevicePermissionRequestCompleteInternal(int32_t device_id,
                                               bool granted) {
    if (service_) {
      service_->DevicePermissionRequestCompleteInternal(device_id, granted);
    }
  }

  base::WeakPtr<UsbServiceAndroid> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

UsbServiceAndroid::UsbServiceAndroid()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  jni_delegate_ = base::MakeRefCounted<JniDelegate>(weak_factory_.GetWeakPtr());
  JNIEnv* env = AttachCurrentThread();
  j_object_.Reset(Java_ChromeUsbService_create(
      env, reinterpret_cast<int64_t>(jni_delegate_.get())));
  ScopedJavaLocalRef<jobjectArray> devices =
      Java_ChromeUsbService_getDevices(env, j_object_);
  for (auto usb_device : devices.CreateView(env)) {
    scoped_refptr<UsbDeviceAndroid> device =
        UsbDeviceAndroid::Create(env, weak_factory_.GetWeakPtr(), usb_device);
    AddDevice(device);
  }
}

UsbServiceAndroid::~UsbServiceAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyWillDestroyUsbService();
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeUsbService_close(env, j_object_);
}

void UsbServiceAndroid::DeviceAttachedInternal(
    const base::android::ScopedJavaGlobalRef<jobject>& usb_device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = AttachCurrentThread();
  scoped_refptr<UsbDeviceAndroid> device =
      UsbDeviceAndroid::Create(env, weak_factory_.GetWeakPtr(), usb_device);
  AddDevice(device);
  NotifyDeviceAdded(device);
}

void UsbServiceAndroid::DeviceDetachedInternal(int32_t device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void UsbServiceAndroid::DevicePermissionRequestCompleteInternal(
    int32_t device_id,
    bool granted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = devices_by_id_.find(device_id);
  if (it == devices_by_id_.end()) {
    return;
  }
  it->second->PermissionGranted(AttachCurrentThread(), granted);
}

ScopedJavaLocalRef<jobject> UsbServiceAndroid::OpenDevice(
    JNIEnv* env,
    const JavaRef<jobject>& wrapper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Java_ChromeUsbService_openDevice(env, j_object_, wrapper);
}

bool UsbServiceAndroid::HasDevicePermission(const JavaRef<jobject>& wrapper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return Java_ChromeUsbService_hasDevicePermission(AttachCurrentThread(),
                                                   j_object_, wrapper);
}

void UsbServiceAndroid::RequestDevicePermission(
    const JavaRef<jobject>& wrapper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_ChromeUsbService_requestDevicePermission(AttachCurrentThread(),
                                                j_object_, wrapper);
}

void UsbServiceAndroid::AddDevice(scoped_refptr<UsbDeviceAndroid> device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!devices_by_id_.contains(device->device_id()));
  DCHECK(!devices().contains(device->guid()));
  devices_by_id_[device->device_id()] = device;
  devices()[device->guid()] = device;

  USB_LOG(USER) << "USB device added: id=" << device->device_id()
                << " vendor=" << device->vendor_id() << " \""
                << device->manufacturer_string()
                << "\", product=" << device->product_id() << " \""
                << device->product_string() << "\", serial=\""
                << device->serial_number() << "\", guid=" << device->guid();
}

DEFINE_JNI(ChromeUsbService)

}  // namespace device
