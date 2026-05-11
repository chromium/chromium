// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/fake_webrtc_audio_classes.h"

#include <algorithm>
#include <utility>

namespace remoting::protocol {

FakeAudioTrackSource::FakeAudioTrackSource() = default;

FakeAudioTrackSource::~FakeAudioTrackSource() = default;

void FakeAudioTrackSource::RegisterObserver(
    webrtc::ObserverInterface* observer) {}

void FakeAudioTrackSource::UnregisterObserver(
    webrtc::ObserverInterface* observer) {}

webrtc::MediaSourceInterface::SourceState FakeAudioTrackSource::state() const {
  return kLive;
}

bool FakeAudioTrackSource::remote() const {
  return false;
}

void FakeAudioTrackSource::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  sinks_.push_back(sink);
}

void FakeAudioTrackSource::RemoveSink(webrtc::AudioTrackSinkInterface* sink) {
  std::erase(sinks_, sink);
}

const std::vector<webrtc::AudioTrackSinkInterface*>&
FakeAudioTrackSource::sinks() const {
  return sinks_;
}

// =============================================================================
// FakeAudioTrack
// =============================================================================

FakeAudioTrack::FakeAudioTrack(FakeAudioTrackSource* source)
    : source_(source) {}

FakeAudioTrack::~FakeAudioTrack() = default;

void FakeAudioTrack::RegisterObserver(webrtc::ObserverInterface* observer) {}

void FakeAudioTrack::UnregisterObserver(webrtc::ObserverInterface* observer) {}

std::string FakeAudioTrack::kind() const {
  return "audio";
}

std::string FakeAudioTrack::id() const {
  return "audio_track";
}

bool FakeAudioTrack::enabled() const {
  return true;
}

bool FakeAudioTrack::set_enabled(bool enable) {
  return true;
}

webrtc::MediaStreamTrackInterface::TrackState FakeAudioTrack::state() const {
  return kLive;
}

void FakeAudioTrack::AddSink(webrtc::AudioTrackSinkInterface* sink) {
  source_->AddSink(sink);
}

void FakeAudioTrack::RemoveSink(webrtc::AudioTrackSinkInterface* sink) {
  source_->RemoveSink(sink);
}

webrtc::AudioSourceInterface* FakeAudioTrack::GetSource() const {
  return source_.get();
}

// =============================================================================
// FakeMediaStream
// =============================================================================

FakeMediaStream::FakeMediaStream(webrtc::AudioTrackInterface* track) {
  audio_tracks_.push_back(
      webrtc::scoped_refptr<webrtc::AudioTrackInterface>(track));
}

FakeMediaStream::~FakeMediaStream() = default;

void FakeMediaStream::RegisterObserver(webrtc::ObserverInterface* observer) {}

void FakeMediaStream::UnregisterObserver(webrtc::ObserverInterface* observer) {}

std::string FakeMediaStream::id() const {
  return "screen_stream";
}

webrtc::AudioTrackVector FakeMediaStream::GetAudioTracks() {
  return audio_tracks_;
}

webrtc::VideoTrackVector FakeMediaStream::GetVideoTracks() {
  return {};
}

webrtc::scoped_refptr<webrtc::AudioTrackInterface>
FakeMediaStream::FindAudioTrack(const std::string& track_id) {
  return audio_tracks_.empty() ? nullptr : audio_tracks_[0];
}

webrtc::scoped_refptr<webrtc::VideoTrackInterface>
FakeMediaStream::FindVideoTrack(const std::string& track_id) {
  return nullptr;
}

bool FakeMediaStream::AddTrack(
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> track) {
  return true;
}

bool FakeMediaStream::AddTrack(
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> track) {
  return true;
}

bool FakeMediaStream::RemoveTrack(
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> track) {
  return true;
}

bool FakeMediaStream::RemoveTrack(
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> track) {
  return true;
}

}  // namespace remoting::protocol
