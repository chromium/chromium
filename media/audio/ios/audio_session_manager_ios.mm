// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_session_manager_ios.h"

#import <AVFAudio/AVFAudio.h>
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"

namespace media {

// Below constant values are taken from :
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/sdk/objc/components/audio/RTCAudioSessionConfiguration.m
const int kRTCAudioSessionPreferredNumberOfChannels = 1;
const double kRTCAudioSessionHighPerformanceSampleRate = 48000.0;
const double kRTCAudioSessionHighPerformanceIOBufferDuration = 0.02;

// static
AudioSessionManagerIOS& AudioSessionManagerIOS::GetInstance() {
  static base::NoDestructor<AudioSessionManagerIOS> instance;
  return *instance;
}

AudioSessionManagerIOS::AudioSessionManagerIOS() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];

  NSError* error = nil;
  auto options = AVAudioSessionCategoryOptionDefaultToSpeaker |
                 AVAudioSessionCategoryOptionAllowBluetooth |
                 AVAudioSessionCategoryOptionAllowBluetoothA2DP |
                 AVAudioSessionCategoryOptionMixWithOthers;
  [audio_session setCategory:AVAudioSessionCategoryPlayAndRecord
                        mode:AVAudioSessionModeDefault
                     options:options
                       error:&error];
  if (error) {
    NSLog(@"Failed to set audio session category with error: %@.",
          error.localizedDescription);
  }

  [audio_session
      setPreferredSampleRate:kRTCAudioSessionHighPerformanceSampleRate
                       error:nil];
  [audio_session setPreferredIOBufferDuration:
                     kRTCAudioSessionHighPerformanceIOBufferDuration
                                        error:nil];
  // Find the desired input port
  NSArray* inputs = [audio_session availableInputs];
  AVAudioSessionPortDescription* builtInMic = nil;
  for (AVAudioSessionPortDescription* port in inputs) {
    if ([port.portType isEqualToString:AVAudioSessionPortBuiltInMic]) {
      builtInMic = port;
      break;
    }
  }

  [audio_session setPreferredInput:builtInMic error:nil];

  AVAudioSessionPortDescription* preferredInput =
      [audio_session preferredInput];
  if (preferredInput != nil) {
    NSArray<AVAudioSessionDataSourceDescription*>* dataSources =
        audio_session.preferredInput.dataSources;
    AVAudioSessionDataSourceDescription* newDataSource = nil;
    for (AVAudioSessionDataSourceDescription* dataSource in dataSources) {
      // Choosing AVAudioSessionOrientationBottom sets the mono channel for a
      // audio session.
      if ([dataSource.orientation isEqual:AVAudioSessionOrientationBottom]) {
        newDataSource = dataSource;
        break;
      }
    }

    if (newDataSource != nil) {
      [preferredInput setPreferredDataSource:newDataSource error:nil];
    }
  }

  // Find the desired audio output device
  AVAudioSessionRouteDescription* currentRoute = [audio_session currentRoute];
  if ([currentRoute.outputs count] > 0) {
    AVAudioSessionPortDescription* output = currentRoute.outputs.firstObject;
    if ([output.portType isEqualToString:AVAudioSessionPortBuiltInReceiver] ||
        [output.portType isEqualToString:AVAudioSessionPortBuiltInSpeaker]) {
      [audio_session overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker
                                       error:&error];
      if (error) {
        NSLog(@"Error overriding output audio port: %@",
              [error localizedDescription]);
      } else {
        NSLog(@"Set default output device to Speaker");
      }
    } else {
      [audio_session overrideOutputAudioPort:AVAudioSessionPortOverrideNone
                                       error:&error];
      if (error) {
        NSLog(@"Error overriding output audio port: %@",
              [error localizedDescription]);
      } else {
        NSLog(@"Using System chosen default audio output device");
      }
    }
  }

  [audio_session setActive:YES error:nil];
  [audio_session setPreferredInputNumberOfChannels:
                     kRTCAudioSessionPreferredNumberOfChannels
                                             error:nil];
  [audio_session setPreferredOutputNumberOfChannels:
                     kRTCAudioSessionPreferredNumberOfChannels
                                              error:nil];
}

void AudioSessionManagerIOS::SetActive(bool active) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  if (active) {
    [audio_session setActive:YES error:nil];
  } else {
    [audio_session setActive:NO error:nil];
  }
}

bool AudioSessionManagerIOS::HasAudioHardware(bool is_input) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  AVAudioSessionRouteDescription* route = [audio_session currentRoute];
  if (is_input) {
    // Search for an audio input hardware.
    NSArray* inputs = [route inputs];
    return [inputs count];
  }

  // Search for an audio output hardware.
  NSArray* outputs = [route outputs];
  return [outputs count];
}

void AudioSessionManagerIOS::GetAudioDeviceInfo(
    bool is_input,
    media::AudioDeviceNames* device_names) {
  if (is_input) {
    GetAudioInputDeviceInfo(device_names);
  } else {
    GetAudioOutputDeviceInfo(device_names);
  }
}

std::string AudioSessionManagerIOS::GetDefaultOutputDeviceID() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  AVAudioSessionPortDescription* currentOutput =
      [audio_session currentRoute].outputs.firstObject;
  return base::SysNSStringToUTF8([currentOutput portName]);
}

std::string AudioSessionManagerIOS::GetDefaultInputDeviceID() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  AVAudioSessionPortDescription* currentInput =
      [audio_session currentRoute].inputs.firstObject;
  return base::SysNSStringToUTF8([currentInput portName]);
}

double AudioSessionManagerIOS::HardwareSampleRate() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return audio_session.sampleRate;
}

double AudioSessionManagerIOS::HardwareIOBufferDuration() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return audio_session.IOBufferDuration;
}

double AudioSessionManagerIOS::HardwareLatency(bool is_input) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return is_input ? audio_session.inputLatency : audio_session.outputLatency;
}

long AudioSessionManagerIOS::GetDeviceChannels(bool is_input) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return is_input ? audio_session.inputNumberOfChannels
                  : audio_session.outputNumberOfChannels;
}

float AudioSessionManagerIOS::GetInputGain() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return audio_session.inputGain;
}

bool AudioSessionManagerIOS::SetInputGain(float volume) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  if ([audio_session isInputGainSettable] == YES) {
    BOOL success = [audio_session setInputGain:volume error:nil];
    return success;
  }
  return false;
}

bool AudioSessionManagerIOS::IsInputMuted() {
#if defined(__IPHONE_17_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_17_0
  if (@available(iOS 17.0, *)) {
    AVAudioApplication* audio_application = [AVAudioApplication sharedInstance];
    return audio_application.isInputMuted;
  }
#endif
  return false;
}

bool AudioSessionManagerIOS::IsInputGainSettable() {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  return [audio_session isInputGainSettable] == YES;
}

// private
void AudioSessionManagerIOS::GetAudioInputDeviceInfo(
    media::AudioDeviceNames* device_names) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  // Find the desired input port
  NSArray* inputs = [audio_session availableInputs];
  for (AVAudioSessionPortDescription* port in inputs) {
    device_names->emplace_back(
        std::string(base::SysNSStringToUTF8([port portName])),
        std::string(base::SysNSStringToUTF8([port UID])));
  }

  if (!device_names->empty()) {
    // Prepend the default device to the list since we always want it to be
    // on the top of the list for all platforms. There is no duplicate
    // counting here since the default device has been abstracted out before.
    device_names->push_front(media::AudioDeviceName::CreateDefault());
  }
}

void AudioSessionManagerIOS::GetAudioOutputDeviceInfo(
    media::AudioDeviceNames* device_names) {
  AVAudioSession* audio_session = [AVAudioSession sharedInstance];
  AVAudioSessionPortDescription* currentOutput =
      [audio_session currentRoute].outputs.firstObject;
  device_names->emplace_back(
      std::string(base::SysNSStringToUTF8([currentOutput portName])),
      std::string(base::SysNSStringToUTF8([currentOutput UID])));

  if (!device_names->empty()) {
    // Prepend the default device to the list since we always want it to be
    // on the top of the list for all platforms. There is no duplicate
    // counting here since the default device has been abstracted out before.
    device_names->push_front(media::AudioDeviceName::CreateDefault());
  }
}

}  // namespace media
