// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_PRIVATE_API_H_
#define MEDIA_AUDIO_IOS_AUDIO_PRIVATE_API_H_

#include <CoreFoundation/CFAvailability.h>
#include <MacTypes.h>

typedef UInt32 AudioObjectPropertySelector;
typedef UInt32 AudioObjectPropertyScope;
typedef UInt32 AudioObjectPropertyElement;

struct AudioObjectPropertyAddress {
  AudioObjectPropertySelector mSelector;
  AudioObjectPropertyScope mScope;
  AudioObjectPropertyElement mElement;
};
typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;

CF_ENUM(AudioObjectPropertyScope){kAudioObjectPropertyScopeGlobal = 'glob',
                                  kAudioObjectPropertyScopeOutput = 'outp'};

CF_ENUM(AudioObjectPropertySelector){
    kAudioHardwarePropertyDefaultOutputDevice = 'dOut',
    kAudioHardwarePropertyDefaultInputDevice = 'dIn ',
    kAudioDevicePropertyTapEnabled = 'tapd',
};

CF_ENUM(int){kAudioObjectSystemObject = 1};

typedef UInt32 AudioObjectID;
typedef AudioObjectID AudioDeviceID;

constexpr UInt32 kAudioObjectUnknown = 0;

extern Boolean AudioObjectHasProperty(
    AudioObjectID inObjectID,
    const AudioObjectPropertyAddress* __nullable inAddress);
extern OSStatus AudioObjectGetPropertyData(
    AudioObjectID inObjectID,
    const AudioObjectPropertyAddress* __nullable inAddress,
    UInt32 inQualifierDataSize,
    const void* __nullable inQualifierData,
    UInt32* __nullable ioDataSize,
    void* __nullable outData);

#endif  // MEDIA_AUDIO_IOS_AUDIO_PRIVATE_API_H_
