// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_FAKE_WEBRTC_AUDIO_CLASSES_H_
#define REMOTING_PROTOCOL_FAKE_WEBRTC_AUDIO_CLASSES_H_

#include <string>
#include <vector>

#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace remoting::protocol {

class FakeAudioTrackSource : public webrtc::AudioSourceInterface {
 public:
  FakeAudioTrackSource();

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

  // webrtc::MediaSourceInterface implementation.
  SourceState state() const override;
  bool remote() const override;

  // webrtc::AudioSourceInterface implementation.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;

  const std::vector<webrtc::AudioTrackSinkInterface*>& sinks() const;

 protected:
  ~FakeAudioTrackSource() override;

 private:
  std::vector<webrtc::AudioTrackSinkInterface*> sinks_;
};

class FakeAudioTrack : public webrtc::AudioTrackInterface {
 public:
  explicit FakeAudioTrack(FakeAudioTrackSource* source);

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

  // webrtc::MediaStreamTrackInterface implementation.
  std::string kind() const override;
  std::string id() const override;
  bool enabled() const override;
  bool set_enabled(bool enable) override;
  TrackState state() const override;

  // webrtc::AudioTrackInterface implementation.
  void AddSink(webrtc::AudioTrackSinkInterface* sink) override;
  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override;
  webrtc::AudioSourceInterface* GetSource() const override;

 protected:
  ~FakeAudioTrack() override;

 private:
  webrtc::scoped_refptr<FakeAudioTrackSource> source_;
};

class FakeMediaStream : public webrtc::MediaStreamInterface {
 public:
  explicit FakeMediaStream(webrtc::AudioTrackInterface* track);

  // webrtc::NotifierInterface implementation.
  void RegisterObserver(webrtc::ObserverInterface* observer) override;
  void UnregisterObserver(webrtc::ObserverInterface* observer) override;

  // webrtc::MediaStreamInterface implementation.
  std::string id() const override;
  webrtc::AudioTrackVector GetAudioTracks() override;
  webrtc::VideoTrackVector GetVideoTracks() override;
  webrtc::scoped_refptr<webrtc::AudioTrackInterface> FindAudioTrack(
      const std::string& track_id) override;
  webrtc::scoped_refptr<webrtc::VideoTrackInterface> FindVideoTrack(
      const std::string& track_id) override;
  bool AddTrack(
      webrtc::scoped_refptr<webrtc::AudioTrackInterface> track) override;
  bool AddTrack(
      webrtc::scoped_refptr<webrtc::VideoTrackInterface> track) override;
  bool RemoveTrack(
      webrtc::scoped_refptr<webrtc::AudioTrackInterface> track) override;
  bool RemoveTrack(
      webrtc::scoped_refptr<webrtc::VideoTrackInterface> track) override;

 protected:
  ~FakeMediaStream() override;

 private:
  webrtc::AudioTrackVector audio_tracks_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_FAKE_WEBRTC_AUDIO_CLASSES_H_
