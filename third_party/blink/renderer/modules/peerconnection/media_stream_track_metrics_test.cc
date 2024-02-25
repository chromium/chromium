// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_track_metrics.h"

#include <stddef.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/media_stream_interface.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackSinkInterface;
using webrtc::MediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::PeerConnectionInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackSourceInterface;

namespace blink {

// A very simple mock that implements only the id() method.
class MockAudioTrackInterface : public AudioTrackInterface {
 public:
  explicit MockAudioTrackInterface(const std::string& id) : id_(id) {}
  ~MockAudioTrackInterface() override {}

  std::string id() const override { return id_; }

  MOCK_METHOD1(RegisterObserver, void(ObserverInterface*));
  MOCK_METHOD1(UnregisterObserver, void(ObserverInterface*));
  MOCK_CONST_METHOD0(kind, std::string());
  MOCK_CONST_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(state, TrackState());
  MOCK_METHOD1(set_enabled, bool(bool));
  MOCK_METHOD1(set_state, bool(TrackState));
  MOCK_CONST_METHOD0(GetSource, AudioSourceInterface*());
  MOCK_METHOD1(AddSink, void(AudioTrackSinkInterface*));
  MOCK_METHOD1(RemoveSink, void(AudioTrackSinkInterface*));

 private:
  std::string id_;
};

// A very simple mock that implements only the id() method.
class MockVideoTrackInterface : public VideoTrackInterface {
 public:
  explicit MockVideoTrackInterface(const std::string& id) : id_(id) {}
  ~MockVideoTrackInterface() override {}

  std::string id() const override { return id_; }

  MOCK_METHOD1(RegisterObserver, void(ObserverInterface*));
  MOCK_METHOD1(UnregisterObserver, void(ObserverInterface*));
  MOCK_CONST_METHOD0(kind, std::string());
  MOCK_CONST_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(state, TrackState());
  MOCK_METHOD1(set_enabled, bool(bool));
  MOCK_METHOD1(set_state, bool(TrackState));
  MOCK_METHOD2(AddOrUpdateSink,
               void(rtc::VideoSinkInterface<webrtc::VideoFrame>*,
                    const rtc::VideoSinkWants&));
  MOCK_METHOD1(RemoveSink, void(rtc::VideoSinkInterface<webrtc::VideoFrame>*));
  MOCK_CONST_METHOD0(GetSource, VideoTrackSourceInterface*());

 private:
  std::string id_;
};

class MockMediaStreamTrackMetrics : public MediaStreamTrackMetrics {
 public:
  virtual ~MockMediaStreamTrackMetrics() {}

  MOCK_METHOD4(SendLifetimeMessage,
               void(const std::string&, Kind, LifetimeEvent, Direction));
  using MediaStreamTrackMetrics::MakeUniqueIdImpl;
};

class MediaStreamTrackMetricsTest : public testing::Test {
 public:
  MediaStreamTrackMetricsTest() : signaling_thread_("signaling_thread") {}

  void SetUp() override {
    metrics_ = std::make_unique<MockMediaStreamTrackMetrics>();
    stream_ = new rtc::RefCountedObject<blink::MockMediaStream>("stream");
    signaling_thread_.Start();
  }

  void TearDown() override {
    signaling_thread_.Stop();
    metrics_.reset();
    stream_ = nullptr;
  }

  scoped_refptr<MockAudioTrackInterface> MakeAudioTrack(const std::string& id) {
    return new rtc::RefCountedObject<MockAudioTrackInterface>(id);
  }

  scoped_refptr<MockVideoTrackInterface> MakeVideoTrack(const std::string& id) {
    return new rtc::RefCountedObject<MockVideoTrackInterface>(id);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockMediaStreamTrackMetrics> metrics_;
  scoped_refptr<MediaStreamInterface> stream_;

  base::Thread signaling_thread_;
};

TEST_F(MediaStreamTrackMetricsTest, MakeUniqueId) {
  // The important testable properties of the unique ID are that it
  // should differ when any of the three constituents differ
  // (PeerConnection pointer, track ID, remote or not. Also, testing
  // that the implementation does not discard the upper 32 bits of the
  // PeerConnection pointer is important.
  //
  // The important hard-to-test property is that the ID be generated
  // using a hash function with virtually zero chance of
  // collisions. We don't test this, we rely on MD5 having this
  // property.

  // Lower 32 bits the same, upper 32 differ.
  EXPECT_NE(
      metrics_->MakeUniqueIdImpl(0x1000000000000001, "x",
                                 MediaStreamTrackMetrics::Direction::kReceive),
      metrics_->MakeUniqueIdImpl(0x2000000000000001, "x",
                                 MediaStreamTrackMetrics::Direction::kReceive));

  // Track ID differs.
  EXPECT_NE(metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::Direction::kReceive),
            metrics_->MakeUniqueIdImpl(
                42, "y", MediaStreamTrackMetrics::Direction::kReceive));

  // Remove vs. local track differs.
  EXPECT_NE(metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::Direction::kReceive),
            metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::Direction::kSend));
}

TEST_F(MediaStreamTrackMetricsTest, BasicRemoteStreams) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kReceive));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kReceive));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kReceive));
  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("video", MediaStreamTrackMetrics::Kind::kVideo,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kReceive));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionDisconnected);
}

TEST_F(MediaStreamTrackMetricsTest, BasicLocalStreams) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("video", MediaStreamTrackMetrics::Kind::kVideo,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamAddedAferIceConnect) {
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));

  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamAddedAferIceConnect) {
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kReceive));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kReceive));

  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamTrackRemoved) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "first");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "second");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "first", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "second", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("first", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kAudio, "first");

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("second", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
}

TEST_F(MediaStreamTrackMetricsTest, RemoveAfterDisconnect) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);

  // This happens after the call is disconnected so no lifetime
  // message should be sent.
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamMultipleDisconnects) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kReceive));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kReceive));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionDisconnected);
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kReceive,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamConnectDisconnectTwice) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kReceive,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");

  for (size_t i = 0; i < 2; ++i) {
    EXPECT_CALL(
        *metrics_,
        SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                            MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                            MediaStreamTrackMetrics::Direction::kReceive));
    metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionConnected);

    EXPECT_CALL(*metrics_,
                SendLifetimeMessage(
                    "audio", MediaStreamTrackMetrics::Kind::kAudio,
                    MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                    MediaStreamTrackMetrics::Direction::kReceive));
    metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionDisconnected);
  }

  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kReceive,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamRemovedNoDisconnect) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("video", MediaStreamTrackMetrics::Kind::kVideo,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kVideo, "video");
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamLargerTest) {
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kVideo, "video");

  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "video", MediaStreamTrackMetrics::Kind::kVideo,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");

  // Add back audio
  EXPECT_CALL(*metrics_, SendLifetimeMessage(
                             "audio", MediaStreamTrackMetrics::Kind::kAudio,
                             MediaStreamTrackMetrics::LifetimeEvent::kConnected,
                             MediaStreamTrackMetrics::Direction::kSend));
  metrics_->AddTrack(MediaStreamTrackMetrics::Direction::kSend,
                     MediaStreamTrackMetrics::Kind::kAudio, "audio");

  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("audio", MediaStreamTrackMetrics::Kind::kAudio,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kAudio, "audio");
  EXPECT_CALL(
      *metrics_,
      SendLifetimeMessage("video", MediaStreamTrackMetrics::Kind::kVideo,
                          MediaStreamTrackMetrics::LifetimeEvent::kDisconnected,
                          MediaStreamTrackMetrics::Direction::kSend));
  metrics_->RemoveTrack(MediaStreamTrackMetrics::Direction::kSend,
                        MediaStreamTrackMetrics::Kind::kVideo, "video");
}

}  // namespace blink
