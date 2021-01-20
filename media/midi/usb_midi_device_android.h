// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
#define MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_

#include <jni.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_export.h"

namespace midi {

class USB_MIDI_EXPORT UsbMidiDeviceAndroid : public UsbMidiDevice {
 public:
  static std::unique_ptr<Factory> CreateFactory();

  UsbMidiDeviceAndroid(const base::android::JavaRef<jobject>& raw_device,
                       UsbMidiDeviceDelegate* delegate);
  ~UsbMidiDeviceAndroid() override;

  // UsbMidiDevice implementation.
  std::vector<uint8_t> GetDescriptors() override;
  std::string GetManufacturer() override;
  std::string GetProductName() override;
  std::string GetDeviceVersion() override;
  void Send(int endpoint_number, const std::vector<uint8_t>& data) override;

  // Called by the Java world.
  void OnData(JNIEnv* env,
              jint endpoint_number,
              const base::android::JavaParamRef<jbyteArray>& data);

 private:
  void GetDescriptorsInternal();
  void InitDeviceInfo();
  std::vector<uint8_t> GetStringDescriptor(int index);
  std::string GetString(int index, const std::string& backup);

  // The actual device object.
  base::android::ScopedJavaGlobalRef<jobject> raw_device_;
  UsbMidiDeviceDelegate* delegate_;

  std::vector<uint8_t> descriptors_;
  std::string manufacturer_;
  std::string product_;
  std::string device_version_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(UsbMidiDeviceAndroid);
};

}  // namespace midi

#endif  // MEDIA_MIDI_USB_MIDI_DEVICE_ANDROID_H_
