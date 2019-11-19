// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_device_factory_android.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "media/midi/midi_jni_headers/UsbMidiDeviceFactoryAndroid_jni.h"
#include "media/midi/usb_midi_device_android.h"

using base::android::JavaParamRef;

namespace midi {

namespace {

using Callback = UsbMidiDevice::Factory::Callback;

}  // namespace

UsbMidiDeviceFactoryAndroid::UsbMidiDeviceFactoryAndroid() : delegate_(NULL) {}

UsbMidiDeviceFactoryAndroid::~UsbMidiDeviceFactoryAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!raw_factory_.is_null())
    Java_UsbMidiDeviceFactoryAndroid_close(env, raw_factory_);
}

void UsbMidiDeviceFactoryAndroid::EnumerateDevices(
    UsbMidiDeviceDelegate* delegate,
    Callback callback) {
  DCHECK(!delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  uintptr_t pointer = reinterpret_cast<uintptr_t>(this);
  raw_factory_.Reset(Java_UsbMidiDeviceFactoryAndroid_create(env, pointer));

  delegate_ = delegate;
  callback_ = std::move(callback);

  if (Java_UsbMidiDeviceFactoryAndroid_enumerateDevices(env, raw_factory_)) {
    // Asynchronous operation.
    return;
  }
  // No devices are found.
  UsbMidiDevice::Devices devices;
  std::move(callback_).Run(true, &devices);
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceRequestDone(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& devices) {
  UsbMidiDevice::Devices devices_to_pass;
  for (auto raw_device : devices.ReadElements<jobject>()) {
    devices_to_pass.push_back(
        std::make_unique<UsbMidiDeviceAndroid>(raw_device, delegate_));
  }

  std::move(callback_).Run(true, &devices_to_pass);
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceAttached(
    JNIEnv* env,
    const JavaParamRef<jobject>& device) {
  delegate_->OnDeviceAttached(
      std::make_unique<UsbMidiDeviceAndroid>(device, delegate_));
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceDetached(
    JNIEnv* env,
    jint index) {
  delegate_->OnDeviceDetached(index);
}

}  // namespace midi
