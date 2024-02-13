// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/apple/audio_manager_apple.h"

#include <memory>
#include <utility>

#include "base/apple/osstatus_logging.h"

namespace media {

AudioManagerApple::AudioManagerApple(std::unique_ptr<AudioThread> audio_thread,
                                     AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerApple::~AudioManagerApple() = default;

// static
std::unique_ptr<ScopedAudioChannelLayout>
AudioManagerApple::GetOutputDeviceChannelLayout(AudioUnit audio_unit) {
  UInt32 size = 0;
  // Note: We don't use kAudioDevicePropertyPreferredChannelLayout on the device
  // because it is not available on all devices.
  OSStatus result = AudioUnitGetPropertyInfo(
      audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Output,
      0, &size, nullptr);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to get property info for AudioUnit channel layout.";
    return nullptr;
  }

  auto output_layout = std::make_unique<ScopedAudioChannelLayout>(size);
  result = AudioUnitGetProperty(
      audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Output,
      0, output_layout->layout(), &size);
  if (result != noErr) {
    OSSTATUS_LOG(ERROR, result) << "Failed to get AudioUnit channel layout.";
    return nullptr;
  }

  // We don't want to have to know about all channel layout tags, so force
  // system to give us the channel descriptions from the bitmap or tag if
  // necessary.
  const AudioChannelLayoutTag tag = output_layout->layout()->mChannelLayoutTag;
  if (tag == kAudioChannelLayoutTag_UseChannelDescriptions) {
    return output_layout;
  }

  if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
    result = AudioFormatGetPropertyInfo(
        kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(UInt32),
        &output_layout->layout()->mChannelBitmap, &size);
  } else {
    result =
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag,
                                   sizeof(AudioChannelLayoutTag), &tag, &size);
  }
  if (result != noErr || !size) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to get AudioFormat property info, size=" << size;
    return nullptr;
  }

  auto new_layout = std::make_unique<ScopedAudioChannelLayout>(size);
  if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
    result = AudioFormatGetProperty(
        kAudioFormatProperty_ChannelLayoutForBitmap, sizeof(UInt32),
        &output_layout->layout()->mChannelBitmap, &size, new_layout->layout());
  } else {
    result = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                    sizeof(AudioChannelLayoutTag), &tag, &size,
                                    new_layout->layout());
  }
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to get AudioFormat property.";
    return nullptr;
  }

  return new_layout;
}

}  // namespace media
