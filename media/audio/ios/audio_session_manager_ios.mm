// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_session_manager_ios.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"

namespace media {

AudioSessionManagerIOS::AudioSessionManagerIOS() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];

  NSString* playbackCategory = AVAudioSessionCategoryPlayAndRecord;
  [audio_session setCategory:playbackCategory error:nil];
  [audio_session setActive:YES error:nil];
}

bool AudioSessionManagerIOS::HasAudioHardware(bool is_input) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  AVAudioSessionRouteDescription* route = [audio_session currentRoute];
  if (is_input) {
    // Seach for a audio input hardware.
    NSArray* inputs = [route inputs];
    return [inputs count];
  }

  // Seach for a audio output hardware.
  NSArray* outputs = [route outputs];
  return [outputs count];
}

void AudioSessionManagerIOS::GetAudioDeviceInfo(
    bool is_input,
    media::AudioDeviceNames* device_names) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  if (is_input) {
    AVAudioSessionPortDescription* currentInput =
        [audio_session currentRoute].inputs.firstObject;
    device_names->emplace_back(
        std::string(base::SysNSStringToUTF8([currentInput portName])),
        std::string(base::SysNSStringToUTF8([currentInput UID])));
  } else {
    AVAudioSessionPortDescription* currentOutput =
        [audio_session currentRoute].outputs.firstObject;
    device_names->emplace_back(
        std::string(base::SysNSStringToUTF8([currentOutput portName])),
        std::string(base::SysNSStringToUTF8([currentOutput UID])));
  }
}

}  // namespace media
