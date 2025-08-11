// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_AUDIO_MAC_CATAP_API_H_
#define MEDIA_AUDIO_MAC_CATAP_API_H_

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/CATapDescription.h>
#include <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>

namespace media {

// Abstract class that defines all the system calls to be used by
// CatapAudioInputStream. This allows to mock the calls in unit tests.
class CatapApi {
 public:
  virtual ~CatapApi() = default;

  virtual OSStatus AudioHardwareCreateAggregateDevice(
      CFDictionaryRef in_device_properties,
      AudioDeviceID* out_device) = 0;
  virtual OSStatus AudioDeviceCreateIOProcID(
      AudioDeviceID in_device,
      AudioDeviceIOProc proc,
      void* in_client_data,
      AudioDeviceIOProcID* out_proc_id) = 0;
  virtual OSStatus AudioObjectGetPropertyDataSize(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* out_data_size) = 0;
  virtual OSStatus AudioObjectGetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* io_data_size,
      void* out_data) = 0;
  virtual OSStatus AudioObjectSetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32 in_data_size,
      const void* in_data) = 0;

  API_AVAILABLE(macos(14.2))
  virtual OSStatus AudioHardwareCreateProcessTap(
      CATapDescription* in_description,
      AudioObjectID* out_tap) = 0;
  virtual OSStatus AudioDeviceStart(AudioDeviceID in_device,
                                    AudioDeviceIOProcID in_proc_id) = 0;
  virtual OSStatus AudioDeviceStop(AudioDeviceID in_device,
                                   AudioDeviceIOProcID in_proc_id) = 0;
  virtual OSStatus AudioDeviceDestroyIOProcID(
      AudioDeviceID in_device,
      AudioDeviceIOProcID in_proc_id) = 0;
  virtual OSStatus AudioHardwareDestroyAggregateDevice(
      AudioDeviceID in_device) = 0;
  virtual OSStatus AudioHardwareDestroyProcessTap(AudioObjectID in_tap) = 0;

  virtual OSStatus AudioObjectAddPropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) = 0;
  virtual OSStatus AudioObjectRemovePropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) = 0;
};

// Implementation of CatapApi that calls the system APIs.
class API_AVAILABLE(macos(14.2)) CatapApiImpl : public CatapApi {
 public:
  CatapApiImpl();
  ~CatapApiImpl() override;

  OSStatus AudioHardwareCreateAggregateDevice(
      CFDictionaryRef in_device_properties,
      AudioDeviceID* out_device) override;
  OSStatus AudioDeviceCreateIOProcID(AudioDeviceID in_device,
                                     AudioDeviceIOProc proc,
                                     void* in_client_data,
                                     AudioDeviceIOProcID* out_proc_id) override;
  OSStatus AudioObjectGetPropertyDataSize(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* out_data_size) override;
  OSStatus AudioObjectGetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32* io_data_size,
      void* out_data) override;
  OSStatus AudioObjectSetPropertyData(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      UInt32 in_qualifier_data_size,
      const void* in_qualifier_data,
      UInt32 in_data_size,
      const void* in_data) override;

  API_AVAILABLE(macos(14.2))
  OSStatus AudioHardwareCreateProcessTap(CATapDescription* in_description,
                                         AudioObjectID* out_tap) override;
  OSStatus AudioDeviceStart(AudioDeviceID in_device,
                            AudioDeviceIOProcID in_proc_id) override;
  OSStatus AudioDeviceStop(AudioDeviceID in_device,
                           AudioDeviceIOProcID in_proc_id) override;
  OSStatus AudioDeviceDestroyIOProcID(AudioDeviceID in_device,
                                      AudioDeviceIOProcID in_proc_id) override;
  OSStatus AudioHardwareDestroyAggregateDevice(
      AudioDeviceID in_device) override;
  OSStatus AudioHardwareDestroyProcessTap(AudioObjectID in_tap) override;

  OSStatus AudioObjectAddPropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) override;
  OSStatus AudioObjectRemovePropertyListenerBlock(
      AudioObjectID in_object_id,
      const AudioObjectPropertyAddress* in_address,
      dispatch_queue_t in_dispatch_queue,
      AudioObjectPropertyListenerBlock in_listener) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_CATAP_API_H_
