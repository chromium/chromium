// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/modules/audio_device/include/audio_device.h"

namespace content {

// WebRtcAudioDeviceNotImpl contains default implementations of all methods
// in the webrtc::AudioDeviceModule which are currently not supported in Chrome.
// The real implementation is in WebRtcAudioDeviceImpl and it derives from
// this class. The main purpose of breaking out non-implemented methods into
// a separate unit is to make WebRtcAudioDeviceImpl more readable and easier
// to maintain.
class CONTENT_EXPORT WebRtcAudioDeviceNotImpl
    : public webrtc::AudioDeviceModule {
 public:
  WebRtcAudioDeviceNotImpl();

  // Methods in webrtc::AudioDeviceModule which are not yet implemented.
  // The idea is that we can move methods from this class to the real
  // implementation in WebRtcAudioDeviceImpl when needed.

  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[webrtc::kAdmMaxDeviceNameSize],
                            char guid[webrtc::kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[webrtc::kAdmMaxDeviceNameSize],
                              char guid[webrtc::kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(WindowsDeviceType device) override;
  int32_t InitPlayout() override;
  int32_t InitRecording() override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool* available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t* volume) const override;
  int32_t MaxSpeakerVolume(uint32_t* max_volume) const override;
  int32_t MinSpeakerVolume(uint32_t* min_volume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool* available) override;
  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;
  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;
  bool BuiltInAECIsAvailable() const override;
  bool BuiltInAGCIsAvailable() const override;
  bool BuiltInNSIsAvailable() const override;
  int32_t EnableBuiltInAEC(bool enable) override;
  int32_t EnableBuiltInAGC(bool enable) override;
  int32_t EnableBuiltInNS(bool enable) override;
#if defined(OS_IOS)
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;
#endif  // OS_IOS

 protected:
  ~WebRtcAudioDeviceNotImpl() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioDeviceNotImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_DEVICE_NOT_IMPL_H_
