// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_AUDIO_MODULE_H_
#define REMOTING_PROTOCOL_WEBRTC_AUDIO_MODULE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "third_party/webrtc/modules/audio_device/include/audio_device.h"

namespace base {
class RepeatingTimer;
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting::protocol {

// Audio module passed to WebRTC. It doesn't access actual audio devices, but it
// provides all functionality we need to ensure that audio streaming works
// properly in WebRTC. Particularly it's responsible for calling AudioTransport
// on regular intervals when playback is active. This ensures that all incoming
// audio data is processed and passed to webrtc::AudioTrackSinkInterface
// connected to the audio track.
class WebrtcAudioModule : public webrtc::AudioDeviceModule {
 public:
  WebrtcAudioModule();
  ~WebrtcAudioModule() override;

  void SetAudioTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner);

  // webrtc::AudioDeviceModule implementation.
  int32_t ActiveAudioLayer(AudioLayer* audio_layer) const override;
  int32_t RegisterAudioCallback(
      webrtc::AudioTransport* audio_callback) override;
  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;
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
  int32_t PlayoutIsAvailable(bool* available) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool* available) override;
  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;
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
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* max_volume) const override;
  int32_t MinMicrophoneVolume(uint32_t* min_volume) const override;
  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;
  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;
  int32_t PlayoutDelay(uint16_t* delay_ms) const override;
  bool BuiltInAECIsAvailable() const override;
  bool BuiltInAGCIsAvailable() const override;
  bool BuiltInNSIsAvailable() const override;
  int32_t EnableBuiltInAEC(bool enable) override;
  int32_t EnableBuiltInAGC(bool enable) override;
  int32_t EnableBuiltInNS(bool enable) override;

// Only supported on iOS.
#if defined(WEBRTC_IOS)
  int GetPlayoutAudioParameters(webrtc::AudioParameters* params) const override;
  int GetRecordAudioParameters(webrtc::AudioParameters* params) const override;
#endif  // WEBRTC_IOS

 private:
  void StartPlayoutOnAudioThread();
  void StopPlayoutOnAudioThread();

  void PollFromSource();

  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // |lock_| must be locked when accessing |initialized_|, |playing_| and
  // |audio_transport_|.
  mutable base::Lock lock_;

  bool initialized_ = false;
  bool playing_ = false;
  raw_ptr<webrtc::AudioTransport> audio_transport_ = nullptr;

  // Timer running on the |audio_task_runner_| that polls audio from
  // |audio_transport_|.
  std::unique_ptr<base::RepeatingTimer> poll_timer_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_AUDIO_MODULE_H_
