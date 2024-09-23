// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/usb_midi_device_android.h"

#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/midi/usb_midi_descriptor_parser.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/midi/midi_jni_headers/UsbMidiDeviceAndroid_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace midi {

UsbMidiDeviceAndroid::UsbMidiDeviceAndroid(
    const base::android::JavaRef<jobject>& raw_device,
    UsbMidiDeviceDelegate* delegate)
    : raw_device_(raw_device), delegate_(delegate) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_UsbMidiDeviceAndroid_registerSelf(env, raw_device_,
                                         reinterpret_cast<jlong>(this));

  GetDescriptorsInternal();
  InitDeviceInfo();
}

UsbMidiDeviceAndroid::~UsbMidiDeviceAndroid() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_UsbMidiDeviceAndroid_close(env, raw_device_);
}

std::vector<uint8_t> UsbMidiDeviceAndroid::GetDescriptors() {
  return descriptors_;
}

std::string UsbMidiDeviceAndroid::GetManufacturer() {
  return manufacturer_;
}

std::string UsbMidiDeviceAndroid::GetProductName() {
  return product_;
}

std::string UsbMidiDeviceAndroid::GetDeviceVersion() {
  return device_version_;
}

void UsbMidiDeviceAndroid::Send(int endpoint_number,
                                const std::vector<uint8_t>& data) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  const uint8_t* head = data.size() ? &data[0] : NULL;
  ScopedJavaLocalRef<jbyteArray> data_to_pass =
      base::android::ToJavaByteArray(env, head, data.size());

  Java_UsbMidiDeviceAndroid_send(env, raw_device_, endpoint_number,
                                 data_to_pass);
}

void UsbMidiDeviceAndroid::OnData(JNIEnv* env,
                                  jint endpoint_number,
                                  const JavaParamRef<jbyteArray>& data) {
  std::vector<uint8_t> bytes;
  base::android::JavaByteArrayToByteVector(env, data, &bytes);

  const uint8_t* head = bytes.size() ? &bytes[0] : NULL;
  delegate_->ReceiveUsbMidiData(this, endpoint_number, head, bytes.size(),
                                base::TimeTicks::Now());
}

void UsbMidiDeviceAndroid::GetDescriptorsInternal() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> descriptors =
      Java_UsbMidiDeviceAndroid_getDescriptors(env, raw_device_);

  base::android::JavaByteArrayToByteVector(env, descriptors, &descriptors_);
}

void UsbMidiDeviceAndroid::InitDeviceInfo() {
  UsbMidiDescriptorParser parser;
  UsbMidiDescriptorParser::DeviceInfo info;

  const uint8_t* data = descriptors_.size() > 0 ? &descriptors_[0] : nullptr;

  if (!parser.ParseDeviceInfo(data, descriptors_.size(), &info)) {
    // We don't report the error here. If it is critical, we will realize it
    // when we parse the descriptors again for ports.
    manufacturer_ = "invalid descriptor";
    product_ = "invalid descriptor";
    device_version_ = "invalid descriptor";
    return;
  }

  manufacturer_ =
      GetString(info.manufacturer_index,
                base::StringPrintf("(vendor id = 0x%04x)", info.vendor_id));
  product_ =
      GetString(info.product_index,
                base::StringPrintf("(product id = 0x%04x)", info.product_id));
  device_version_ = info.BcdVersionToString(info.bcd_device_version);
}

std::vector<uint8_t> UsbMidiDeviceAndroid::GetStringDescriptor(int index) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> descriptors =
      Java_UsbMidiDeviceAndroid_getStringDescriptor(env, raw_device_, index);

  std::vector<uint8_t> ret;
  base::android::JavaByteArrayToByteVector(env, descriptors, &ret);
  return ret;
}

std::string UsbMidiDeviceAndroid::GetString(int index,
                                            const std::string& backup) {
  const uint8_t DESCRIPTOR_TYPE_STRING = 3;

  if (!index) {
    // index 0 means there is no such descriptor.
    return backup;
  }
  std::vector<uint8_t> descriptor = GetStringDescriptor(index);
  if (descriptor.size() < 2 || descriptor.size() < descriptor[0] ||
      descriptor[1] != DESCRIPTOR_TYPE_STRING) {
    // |descriptor| is not a valid string descriptor.
    return backup;
  }
  size_t size = descriptor[0];
  std::string encoded(reinterpret_cast<char*>(&descriptor[0]) + 2, size - 2);
  std::string result;
  // Unicode ECN specifies that the string is encoded in UTF-16LE.
  if (!base::ConvertToUtf8AndNormalize(encoded, "utf-16le", &result))
    return backup;
  return result;
}

}  // namespace midi
