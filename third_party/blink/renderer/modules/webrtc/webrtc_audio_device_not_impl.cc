// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_not_impl.h"

#include "build/build_config.h"

namespace blink {

WebRtcAudioDeviceNotImpl::WebRtcAudioDeviceNotImpl() = default;

int32_t WebRtcAudioDeviceNotImpl::ActiveAudioLayer(
    AudioLayer* audio_layer) const {
  return 0;
}

int16_t WebRtcAudioDeviceNotImpl::PlayoutDevices() {
  return 0;
}

int16_t WebRtcAudioDeviceNotImpl::RecordingDevices() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::PlayoutDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::RecordingDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetPlayoutDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetRecordingDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitPlayout() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitRecording() {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::InitSpeaker() {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::SpeakerIsInitialized() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::InitMicrophone() {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::MicrophoneIsInitialized() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerVolumeIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetSpeakerVolume(uint32_t volume) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerVolume(uint32_t* volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MaxSpeakerVolume(uint32_t* max_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MinSpeakerVolume(uint32_t* min_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneVolumeIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetMicrophoneVolume(uint32_t volume) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneVolume(uint32_t* volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MaxMicrophoneVolume(
    uint32_t* max_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MinMicrophoneVolume(
    uint32_t* min_volume) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerMuteIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetSpeakerMute(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SpeakerMute(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneMuteIsAvailable(bool* available) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetMicrophoneMute(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::MicrophoneMute(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoPlayoutIsAvailable(
    bool* available) const {
  *available = false;
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetStereoPlayout(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoPlayout(bool* enabled) const {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoRecordingIsAvailable(
    bool* available) const {
  *available = false;
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::SetStereoRecording(bool enable) {
  return 0;
}

int32_t WebRtcAudioDeviceNotImpl::StereoRecording(bool* enabled) const {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::BuiltInAECIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInAEC(bool enable) {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::BuiltInAGCIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInAGC(bool enable) {
  return 0;
}

bool WebRtcAudioDeviceNotImpl::BuiltInNSIsAvailable() const {
  return false;
}

int32_t WebRtcAudioDeviceNotImpl::EnableBuiltInNS(bool enable) {
  return 0;
}

#if BUILDFLAG(IS_IOS)
int WebRtcAudioDeviceNotImpl::GetPlayoutAudioParameters(
    webrtc::AudioParameters* params) const {
  return 0;
}

int WebRtcAudioDeviceNotImpl::GetRecordAudioParameters(
    webrtc::AudioParameters* params) const {
  return 0;
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace blink
