// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_TYPE_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_TYPE_H_

#include <optional>

#include "media/base/media_export.h"

namespace media::android {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum must also be in sync with
// the constants in the Java `android.media.AudioDeviceInfo`, which are
// themselves expected not to be renumbered or reused.
//
// LINT.IfChange(AudioDeviceType)
enum class AudioDeviceType {
  kUnknown = 0,
  kBuiltinEarpiece = 1,
  kBuiltinSpeaker = 2,
  kWiredHeadset = 3,
  kWiredHeadphones = 4,
  kLineAnalog = 5,
  kLineDigital = 6,
  kBluetoothSco = 7,
  kBluetoothA2dp = 8,
  kHdmi = 9,
  kHdmiArc = 10,
  kUsbDevice = 11,
  kUsbAccessory = 12,
  kDock = 13,
  kFm = 14,
  kBuiltinMic = 15,
  kFmTuner = 16,
  kTvTuner = 17,
  kTelephony = 18,
  kAuxLine = 19,
  kIp = 20,
  kBus = 21,
  kUsbHeadset = 22,
  kHearingAid = 23,
  kBuiltinSpeakerSafe = 24,
  kRemoteSubmix = 25,
  kBleHeadset = 26,
  kBleSpeaker = 27,
  kEchoReference = 28,
  kHdmiEarc = 29,
  kBleBroadcast = 30,
  kDockAnalog = 31,
  kMultichannelGroup = 32,
  kMaxValue = kMultichannelGroup,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/media/enums.xml:AndroidAudioDeviceType)

// Converts an integer device type value to an `AudioDeviceType`. Returns
// `std::nullopt` if the value does not map to a known `AudioDeviceType`.
MEDIA_EXPORT std::optional<AudioDeviceType> IntToAudioDeviceType(int value);

}  // namespace media::android

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_TYPE_H_
