// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_audio_module.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"

namespace remoting::protocol {

namespace {

const int kSamplingRate = 48000;

// Webrtc uses 10ms frames.
const int kFrameLengthMs = 10;
const int kSamplesPerFrame = kSamplingRate * kFrameLengthMs / 1000;

constexpr base::TimeDelta kPollInterval =
    base::Milliseconds(5 * kFrameLengthMs);
const int kChannels = 2;
const int kBytesPerSample = 2;

}  // namespace

// webrtc::AudioDeviceModule is a generic interface that aims to provide all
// functionality normally supported audio input/output devices, but most of
// the functions are never called in Webrtc. This class implements only
// functions that are actually used. All unused functions are marked as
// NOTREACHED().

WebrtcAudioModule::WebrtcAudioModule() = default;
WebrtcAudioModule::~WebrtcAudioModule() = default;

void WebrtcAudioModule::SetAudioTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) {
  DCHECK(!audio_task_runner_);
  DCHECK(audio_task_runner);
  audio_task_runner_ = audio_task_runner;
}

int32_t WebrtcAudioModule::ActiveAudioLayer(AudioLayer* audio_layer) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::RegisterAudioCallback(
    webrtc::AudioTransport* audio_transport) {
  base::AutoLock lock(lock_);
  audio_transport_ = audio_transport;
  return 0;
}

int32_t WebrtcAudioModule::Init() {
  base::AutoLock auto_lock(lock_);
  initialized_ = true;
  return 0;
}

int32_t WebrtcAudioModule::Terminate() {
  base::AutoLock auto_lock(lock_);
  initialized_ = false;
  return 0;
}

bool WebrtcAudioModule::Initialized() const {
  base::AutoLock auto_lock(lock_);
  return initialized_;
}

int16_t WebrtcAudioModule::PlayoutDevices() {
  return 0;
}

int16_t WebrtcAudioModule::RecordingDevices() {
  return 0;
}

int32_t WebrtcAudioModule::PlayoutDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebrtcAudioModule::RecordingDeviceName(
    uint16_t index,
    char name[webrtc::kAdmMaxDeviceNameSize],
    char guid[webrtc::kAdmMaxGuidSize]) {
  return 0;
}

int32_t WebrtcAudioModule::SetPlayoutDevice(uint16_t index) {
  return 0;
}

int32_t WebrtcAudioModule::SetPlayoutDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebrtcAudioModule::SetRecordingDevice(uint16_t index) {
  return 0;
}

int32_t WebrtcAudioModule::SetRecordingDevice(WindowsDeviceType device) {
  return 0;
}

int32_t WebrtcAudioModule::PlayoutIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::InitPlayout() {
  return 0;
}

bool WebrtcAudioModule::PlayoutIsInitialized() const {
  base::AutoLock auto_lock(lock_);
  return initialized_;
}

int32_t WebrtcAudioModule::RecordingIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::InitRecording() {
  return 0;
}

bool WebrtcAudioModule::RecordingIsInitialized() const {
  return false;
}

int32_t WebrtcAudioModule::StartPlayout() {
  base::AutoLock auto_lock(lock_);
  if (!playing_ && audio_task_runner_) {
    audio_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebrtcAudioModule::StartPlayoutOnAudioThread,
                                  rtc::scoped_refptr<WebrtcAudioModule>(this)));
    playing_ = true;
  }
  return 0;
}

int32_t WebrtcAudioModule::StopPlayout() {
  base::AutoLock auto_lock(lock_);
  if (playing_) {
    audio_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WebrtcAudioModule::StopPlayoutOnAudioThread,
                                  rtc::scoped_refptr<WebrtcAudioModule>(this)));
    playing_ = false;
  }
  return 0;
}

bool WebrtcAudioModule::Playing() const {
  base::AutoLock auto_lock(lock_);
  return playing_;
}

int32_t WebrtcAudioModule::StartRecording() {
  return 0;
}

int32_t WebrtcAudioModule::StopRecording() {
  return 0;
}

bool WebrtcAudioModule::Recording() const {
  return false;
}

int32_t WebrtcAudioModule::InitSpeaker() {
  return 0;
}

bool WebrtcAudioModule::SpeakerIsInitialized() const {
  return false;
}

int32_t WebrtcAudioModule::InitMicrophone() {
  return 0;
}

bool WebrtcAudioModule::MicrophoneIsInitialized() const {
  return false;
}

int32_t WebrtcAudioModule::SpeakerVolumeIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SetSpeakerVolume(uint32_t volume) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SpeakerVolume(uint32_t* volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MaxSpeakerVolume(uint32_t* max_volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MinSpeakerVolume(uint32_t* min_volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MicrophoneVolumeIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SetMicrophoneVolume(uint32_t volume) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MicrophoneVolume(uint32_t* volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MaxMicrophoneVolume(uint32_t* max_volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MinMicrophoneVolume(uint32_t* min_volume) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SpeakerMuteIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SetSpeakerMute(bool enable) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SpeakerMute(bool* enabled) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MicrophoneMuteIsAvailable(bool* available) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::SetMicrophoneMute(bool enable) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::MicrophoneMute(bool* enabled) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::StereoPlayoutIsAvailable(bool* available) const {
  *available = true;
  return 0;
}

int32_t WebrtcAudioModule::SetStereoPlayout(bool enable) {
  DCHECK(enable);
  return 0;
}

int32_t WebrtcAudioModule::StereoPlayout(bool* enabled) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::StereoRecordingIsAvailable(bool* available) const {
  *available = false;
  return 0;
}

int32_t WebrtcAudioModule::SetStereoRecording(bool enable) {
  return 0;
}

int32_t WebrtcAudioModule::StereoRecording(bool* enabled) const {
  NOTREACHED();
}

int32_t WebrtcAudioModule::PlayoutDelay(uint16_t* delay_ms) const {
  *delay_ms = 0;
  return 0;
}

bool WebrtcAudioModule::BuiltInAECIsAvailable() const {
  return false;
}

bool WebrtcAudioModule::BuiltInAGCIsAvailable() const {
  return false;
}

bool WebrtcAudioModule::BuiltInNSIsAvailable() const {
  return false;
}

int32_t WebrtcAudioModule::EnableBuiltInAEC(bool enable) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::EnableBuiltInAGC(bool enable) {
  NOTREACHED();
}

int32_t WebrtcAudioModule::EnableBuiltInNS(bool enable) {
  NOTREACHED();
}

#if defined(WEBRTC_IOS)
int WebrtcAudioModule::GetPlayoutAudioParameters(
    webrtc::AudioParameters* params) const {
  NOTREACHED();
}

int WebrtcAudioModule::GetRecordAudioParameters(
    webrtc::AudioParameters* params) const {
  NOTREACHED();
}
#endif  // WEBRTC_IOS

void WebrtcAudioModule::StartPlayoutOnAudioThread() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  poll_timer_ = std::make_unique<base::RepeatingTimer>();
  poll_timer_->Start(FROM_HERE, kPollInterval,
                     base::BindRepeating(&WebrtcAudioModule::PollFromSource,
                                         base::Unretained(this)));
}

void WebrtcAudioModule::StopPlayoutOnAudioThread() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());
  poll_timer_.reset();
}

void WebrtcAudioModule::PollFromSource() {
  DCHECK(audio_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(lock_);
  if (!audio_transport_) {
    return;
  }

  for (int i = 0; i < kPollInterval.InMilliseconds() / kFrameLengthMs; i++) {
    int64_t elapsed_time_ms = -1;
    int64_t ntp_time_ms = -1;
    char data[kBytesPerSample * kChannels * kSamplesPerFrame];
    audio_transport_->PullRenderData(kBytesPerSample * 8, kSamplingRate,
                                     kChannels, kSamplesPerFrame, data,
                                     &elapsed_time_ms, &ntp_time_ms);
  }
}

}  // namespace remoting::protocol
