// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/audio/mac/catap_api.h"

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CoreAudio.h>

namespace media {

CatapApiImpl::CatapApiImpl() = default;
CatapApiImpl::~CatapApiImpl() = default;

OSStatus CatapApiImpl::AudioHardwareCreateAggregateDevice(
    CFDictionaryRef in_device_properties,
    AudioDeviceID* out_device) {
  return ::AudioHardwareCreateAggregateDevice(in_device_properties, out_device);
}

OSStatus CatapApiImpl::AudioDeviceCreateIOProcID(
    AudioDeviceID in_device,
    AudioDeviceIOProc proc,
    void* in_client_data,
    AudioDeviceIOProcID* out_proc_id) {
  return ::AudioDeviceCreateIOProcID(in_device, proc, in_client_data,
                                     out_proc_id);
}

OSStatus CatapApiImpl::AudioObjectGetPropertyDataSize(
    AudioObjectID in_object_id,
    const AudioObjectPropertyAddress* in_address,
    UInt32 in_qualifier_data_size,
    const void* in_qualifier_data,
    UInt32* out_data_size) {
  return ::AudioObjectGetPropertyDataSize(in_object_id, in_address,
                                          in_qualifier_data_size,
                                          in_qualifier_data, out_data_size);
}

OSStatus CatapApiImpl::AudioObjectGetPropertyData(
    AudioObjectID in_object_id,
    const AudioObjectPropertyAddress* in_address,
    UInt32 in_qualifier_data_size,
    const void* in_qualifier_data,
    UInt32* io_data_size,
    void* out_data) {
  return ::AudioObjectGetPropertyData(in_object_id, in_address,
                                      in_qualifier_data_size, in_qualifier_data,
                                      io_data_size, out_data);
}

OSStatus CatapApiImpl::AudioObjectSetPropertyData(
    AudioObjectID in_object_id,
    const AudioObjectPropertyAddress* in_address,
    UInt32 in_qualifier_data_size,
    const void* in_qualifier_data,
    UInt32 in_data_size,
    const void* in_data) {
  return ::AudioObjectSetPropertyData(in_object_id, in_address,
                                      in_qualifier_data_size, in_qualifier_data,
                                      in_data_size, in_data);
}

OSStatus CatapApiImpl::AudioHardwareCreateProcessTap(
    CATapDescription* in_description,
    AudioObjectID* out_tap) {
  return ::AudioHardwareCreateProcessTap(in_description, out_tap);
}

OSStatus CatapApiImpl::AudioDeviceStart(AudioDeviceID in_device,
                                        AudioDeviceIOProcID in_proc_id) {
  return ::AudioDeviceStart(in_device, in_proc_id);
}

OSStatus CatapApiImpl::AudioDeviceStop(AudioDeviceID in_device,
                                       AudioDeviceIOProcID in_proc_id) {
  return ::AudioDeviceStop(in_device, in_proc_id);
}

OSStatus CatapApiImpl::AudioDeviceDestroyIOProcID(
    AudioDeviceID in_device,
    AudioDeviceIOProcID in_proc_id) {
  return ::AudioDeviceDestroyIOProcID(in_device, in_proc_id);
}

OSStatus CatapApiImpl::AudioHardwareDestroyAggregateDevice(
    AudioDeviceID in_device) {
  return ::AudioHardwareDestroyAggregateDevice(in_device);
}

OSStatus CatapApiImpl::AudioHardwareDestroyProcessTap(AudioObjectID in_tap) {
  return ::AudioHardwareDestroyProcessTap(in_tap);
}

}  // namespace media
