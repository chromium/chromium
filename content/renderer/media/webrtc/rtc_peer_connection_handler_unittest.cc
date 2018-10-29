// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_peer_connection_handler.h"

#include <stddef.h>
#include <string.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio/mock_audio_device_factory.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_audio_track.h"
#include "content/renderer/media/stream/media_stream_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/stream/mock_constraint_factory.h"
#include "content/renderer/media/stream/mock_media_stream_video_source.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/mock_data_channel_impl.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/mock_peer_connection_impl.h"
#include "content/renderer/media/webrtc/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/webrtc/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/rtc_stats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_data_channel_handler.h"
#include "third_party/blink/public/platform/web_rtc_data_channel_init.h"
#include "third_party/blink/public/platform/web_rtc_dtmf_sender_handler.h"
#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_session_description.h"
#include "third_party/blink/public/platform/web_rtc_session_description_request.h"
#include "third_party/blink/public/platform/web_rtc_stats_request.h"
#include "third_party/blink/public/platform/web_rtc_void_request.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/rtpreceiverinterface.h"
#include "third_party/webrtc/stats/test/rtcteststats.h"

static const char kDummySdp[] = "dummy sdp";
static const char kDummySdpType[] = "dummy type";

using blink::WebRTCPeerConnectionHandlerClient;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Ref;
using testing::SaveArg;
using testing::WithArg;

namespace content {

// Action SaveArgPointeeMove<k>(pointer) saves the value pointed to by the k-th
// (0-based) argument of the mock function by moving it to *pointer.
ACTION_TEMPLATE(SaveArgPointeeMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(*testing::get<k>(args));
}

class MockRTCStatsResponse : public LocalRTCStatsResponse {
 public:
  MockRTCStatsResponse()
      : report_count_(0),
        statistic_count_(0) {
  }

  void addStats(const blink::WebRTCLegacyStats& stats) override {
    ++report_count_;
    for (std::unique_ptr<blink::WebRTCLegacyStatsMemberIterator> member(
             stats.Iterator());
         !member->IsEnd(); member->Next()) {
      ++statistic_count_;
    }
  }

  int report_count() const { return report_count_; }

 private:
  int report_count_;
  int statistic_count_;
};

// Mocked wrapper for blink::WebRTCStatsRequest
class MockRTCStatsRequest : public LocalRTCStatsRequest {
 public:
  MockRTCStatsRequest()
      : has_selector_(false),
        request_succeeded_called_(false) {}

  bool hasSelector() const override { return has_selector_; }
  blink::WebMediaStreamTrack component() const override { return component_; }
  scoped_refptr<LocalRTCStatsResponse> createResponse() override {
    DCHECK(!response_.get());
    response_ = new rtc::RefCountedObject<MockRTCStatsResponse>();
    return response_;
  }

  void requestSucceeded(const LocalRTCStatsResponse* response) override {
    EXPECT_EQ(response, response_.get());
    request_succeeded_called_ = true;
  }

  // Function for setting whether or not a selector is available.
  void setSelector(const blink::WebMediaStreamTrack& component) {
    has_selector_ = true;
    component_ = component;
  }

  // Function for inspecting the result of a stats request.
  MockRTCStatsResponse* result() {
    if (request_succeeded_called_) {
      return response_.get();
    } else {
      return nullptr;
    }
  }

 private:
  bool has_selector_;
  blink::WebMediaStreamTrack component_;
  scoped_refptr<MockRTCStatsResponse> response_;
  bool request_succeeded_called_;
};

class MockPeerConnectionTracker : public PeerConnectionTracker {
 public:
  MockPeerConnectionTracker()
      : PeerConnectionTracker(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  MOCK_METHOD1(UnregisterPeerConnection,
               void(RTCPeerConnectionHandler* pc_handler));
  // TODO(jiayl): add coverage for the following methods
  MOCK_METHOD2(TrackCreateOffer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaConstraints& constraints));
  MOCK_METHOD2(TrackCreateAnswer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaConstraints& constraints));
  MOCK_METHOD4(TrackSetSessionDescription,
               void(RTCPeerConnectionHandler* pc_handler,
                    const std::string& sdp, const std::string& type,
                    Source source));
  MOCK_METHOD2(
      TrackSetConfiguration,
      void(RTCPeerConnectionHandler* pc_handler,
           const webrtc::PeerConnectionInterface::RTCConfiguration& config));
  MOCK_METHOD4(TrackAddIceCandidate,
               void(RTCPeerConnectionHandler* pc_handler,
                    scoped_refptr<blink::WebRTCICECandidate> candidate,
                    Source source,
                    bool succeeded));
  MOCK_METHOD4(TrackAddTransceiver,
               void(RTCPeerConnectionHandler* pc_handler,
                    TransceiverUpdatedReason reason,
                    const blink::WebRTCRtpTransceiver& transceiver,
                    size_t transceiver_index));
  MOCK_METHOD4(TrackModifyTransceiver,
               void(RTCPeerConnectionHandler* pc_handler,
                    TransceiverUpdatedReason reason,
                    const blink::WebRTCRtpTransceiver& transceiver,
                    size_t transceiver_index));
  MOCK_METHOD4(TrackRemoveTransceiver,
               void(RTCPeerConnectionHandler* pc_handler,
                    TransceiverUpdatedReason reason,
                    const blink::WebRTCRtpTransceiver& transceiver,
                    size_t transceiver_index));
  MOCK_METHOD1(TrackOnIceComplete,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD3(TrackCreateDataChannel,
               void(RTCPeerConnectionHandler* pc_handler,
                    const webrtc::DataChannelInterface* data_channel,
                    Source source));
  MOCK_METHOD1(TrackStop, void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackSignalingStateChange,
               void(RTCPeerConnectionHandler* pc_handler,
                    webrtc::PeerConnectionInterface::SignalingState state));
  MOCK_METHOD2(TrackIceConnectionStateChange,
               void(RTCPeerConnectionHandler* pc_handler,
                    webrtc::PeerConnectionInterface::IceConnectionState state));
  MOCK_METHOD2(TrackIceGatheringStateChange,
               void(RTCPeerConnectionHandler* pc_handler,
                    webrtc::PeerConnectionInterface::IceGatheringState state));
  MOCK_METHOD4(TrackSessionDescriptionCallback,
               void(RTCPeerConnectionHandler* pc_handler,
                    Action action,
                    const std::string& type,
                    const std::string& value));
  MOCK_METHOD1(TrackOnRenegotiationNeeded,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackCreateDTMFSender,
               void(RTCPeerConnectionHandler* pc_handler,
                     const blink::WebMediaStreamTrack& track));
};

class MockRTCStatsReportCallback : public blink::WebRTCStatsReportCallback {
 public:
  explicit MockRTCStatsReportCallback(
      std::unique_ptr<blink::WebRTCStatsReport>* result)
      : main_thread_(blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        result_(result) {
    DCHECK(result_);
  }

  void OnStatsDelivered(
      std::unique_ptr<blink::WebRTCStatsReport> report) override {
    EXPECT_TRUE(main_thread_->BelongsToCurrentThread());
    EXPECT_TRUE(report);
    result_->reset(report.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  std::unique_ptr<blink::WebRTCStatsReport>* result_;
};

template<typename T>
std::vector<T> ToSequence(T value) {
  std::vector<T> vec;
  vec.push_back(value);
  return vec;
}

template<typename T>
void ExpectSequenceEquals(const blink::WebVector<T>& sequence, T value) {
  EXPECT_EQ(sequence.size(), static_cast<size_t>(1));
  EXPECT_EQ(sequence[0], value);
}

class RTCPeerConnectionHandlerUnderTest : public RTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandlerUnderTest(
      WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory)
      : RTCPeerConnectionHandler(
            client,
            dependency_factory,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<MockPeerConnectionImpl*>(
        RTCPeerConnectionHandler::native_peer_connection());
  }

  webrtc::PeerConnectionObserver* observer() {
    return native_peer_connection()->observer();
  }
};

class RTCPeerConnectionHandlerTest : public ::testing::Test {
 public:
  RTCPeerConnectionHandlerTest() : mock_peer_connection_(nullptr) {}

  void SetUp() override {
    mock_client_.reset(new NiceMock<MockWebRTCPeerConnectionHandlerClient>());
    mock_dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    pc_handler_ = CreateRTCPeerConnectionHandlerUnderTest();
    mock_tracker_.reset(new NiceMock<MockPeerConnectionTracker>());
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kPlanB;
    blink::WebMediaConstraints constraints;
    EXPECT_TRUE(pc_handler_->InitializeForTest(
        config, constraints, mock_tracker_.get()->AsWeakPtr()));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
    EXPECT_CALL(*mock_peer_connection_, Close());
  }

  void TearDown() override {
    pc_handler_.reset();
    mock_tracker_.reset();
    mock_dependency_factory_.reset();
    mock_client_.reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  std::unique_ptr<RTCPeerConnectionHandlerUnderTest>
  CreateRTCPeerConnectionHandlerUnderTest() {
    return std::make_unique<RTCPeerConnectionHandlerUnderTest>(
        mock_client_.get(), mock_dependency_factory_.get());
  }

  // Creates a WebKit local MediaStream.
  blink::WebMediaStream CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");
    blink::WebMediaStreamSource blink_audio_source;
    blink_audio_source.Initialize(blink::WebString::FromUTF8(audio_track_label),
                                  blink::WebMediaStreamSource::kTypeAudio,
                                  blink::WebString::FromUTF8("audio_track"),
                                  false /* remote */);
    ProcessedLocalAudioSource* const audio_source =
        new ProcessedLocalAudioSource(
            -1 /* consumer_render_frame_id is N/A for non-browser tests */,
            MediaStreamDevice(MEDIA_DEVICE_AUDIO_CAPTURE, "mock_device_id",
                              "Mock device",
                              media::AudioParameters::kAudioCDSampleRate,
                              media::CHANNEL_LAYOUT_STEREO,
                              media::AudioParameters::kAudioCDSampleRate / 100),
            false /* hotword_enabled */, false /* disable_local_echo */,
            AudioProcessingProperties(),
            base::Bind(&RTCPeerConnectionHandlerTest::OnAudioSourceStarted),
            mock_dependency_factory_.get());
    audio_source->SetAllowInvalidRenderFrameIdForTesting(true);
    blink_audio_source.SetExtraData(audio_source);  // Takes ownership.

    blink::WebMediaStreamSource video_source;
    video_source.Initialize(blink::WebString::FromUTF8(video_track_label),
                            blink::WebMediaStreamSource::kTypeVideo,
                            blink::WebString::FromUTF8("video_track"),
                            false /* remote */);
    MockMediaStreamVideoSource* native_video_source =
        new MockMediaStreamVideoSource();
    video_source.SetExtraData(native_video_source);

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
        static_cast<size_t>(1));
    audio_tracks[0].Initialize(blink_audio_source.Id(), blink_audio_source);
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                Initialize(_, _));
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                SetAutomaticGainControl(true));
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Start());
    EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Stop());
    CHECK(audio_source->ConnectToTrack(audio_tracks[0]));
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
        static_cast<size_t>(1));
    video_tracks[0] = MediaStreamVideoTrack::CreateVideoTrack(
        native_video_source, MediaStreamVideoSource::ConstraintsCallback(),
        true);

    blink::WebMediaStream local_stream;
    local_stream.Initialize(blink::WebString::FromUTF8(stream_label),
                            audio_tracks, video_tracks);
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  rtc::scoped_refptr<webrtc::MediaStreamInterface> AddRemoteMockMediaStream(
      const std::string& stream_label,
      const std::string& video_track_label,
      const std::string& audio_track_label) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label).get());
    if (!video_track_label.empty()) {
      InvokeAddTrack(stream,
                     MockWebRtcVideoTrack::Create(video_track_label).get());
    }
    if (!audio_track_label.empty()) {
      InvokeAddTrack(stream,
                     MockWebRtcAudioTrack::Create(audio_track_label).get());
    }
    mock_peer_connection_->AddRemoteStream(stream);
    return stream;
  }

  void StopAllTracks(const blink::WebMediaStream& stream) {
    for (const auto& track : stream.AudioTracks())
      MediaStreamAudioTrack::From(track)->Stop();
    for (const auto& track : stream.VideoTracks())
      MediaStreamVideoTrack::GetVideoTrack(track)->Stop();
  }

  static void OnAudioSourceStarted(MediaStreamSource* source,
                                   MediaStreamRequestResult result,
                                   const blink::WebString& result_name) {}

  bool AddStream(const blink::WebMediaStream& web_stream) {
    size_t senders_size_before_add = senders_.size();
    for (const auto& web_audio_track : web_stream.AudioTracks()) {
      auto error_or_transceiver = pc_handler_->AddTrack(
          web_audio_track, std::vector<blink::WebMediaStream>({web_stream}));
      if (error_or_transceiver.ok()) {
        DCHECK_EQ(
            error_or_transceiver.value()->ImplementationType(),
            blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
        auto sender = error_or_transceiver.value()->Sender();
        senders_.push_back(std::unique_ptr<RTCRtpSender>(
            static_cast<RTCRtpSender*>(sender.release())));
      }
    }
    for (const auto& web_video_track : web_stream.VideoTracks()) {
      auto error_or_transceiver = pc_handler_->AddTrack(
          web_video_track, std::vector<blink::WebMediaStream>({web_stream}));
      if (error_or_transceiver.ok()) {
        DCHECK_EQ(
            error_or_transceiver.value()->ImplementationType(),
            blink::WebRTCRtpTransceiverImplementationType::kPlanBSenderOnly);
        auto sender = error_or_transceiver.value()->Sender();
        senders_.push_back(std::unique_ptr<RTCRtpSender>(
            static_cast<RTCRtpSender*>(sender.release())));
      }
    }
    return senders_size_before_add < senders_.size();
  }

  std::vector<std::unique_ptr<RTCRtpSender>>::iterator FindSenderForTrack(
      const blink::WebMediaStreamTrack& web_track) {
    for (auto it = senders_.begin(); it != senders_.end(); ++it) {
      if ((*it)->Track().UniqueId() == web_track.UniqueId())
        return it;
    }
    return senders_.end();
  }

  bool RemoveStream(const blink::WebMediaStream& web_stream) {
    size_t senders_size_before_remove = senders_.size();
    // TODO(hbos): With Unified Plan senders are not removed.
    // https://crbug.com/799030
    for (const auto& web_audio_track : web_stream.AudioTracks()) {
      auto it = FindSenderForTrack(web_audio_track);
      if (it != senders_.end() && pc_handler_->RemoveTrack((*it).get()).ok())
        senders_.erase(it);
    }
    for (const auto& web_video_track : web_stream.VideoTracks()) {
      auto it = FindSenderForTrack(web_video_track);
      if (it != senders_.end() && pc_handler_->RemoveTrack((*it).get()).ok())
        senders_.erase(it);
    }
    return senders_size_before_remove > senders_.size();
  }

  void InvokeOnAddStream(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream) {
    for (const auto& audio_track : remote_stream->GetAudioTracks()) {
      InvokeOnAddTrack(audio_track, remote_stream);
    }
    for (const auto& video_track : remote_stream->GetVideoTracks()) {
      InvokeOnAddTrack(video_track, remote_stream);
    }
  }

  void InvokeOnAddTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>& remote_track,
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream) {
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver(
        new rtc::RefCountedObject<FakeRtpReceiver>(remote_track));
    receivers_by_track_.insert(std::make_pair(remote_track.get(), receiver));
    std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>
        receiver_streams;
    receiver_streams.push_back(remote_stream);
    InvokeOnSignalingThread(base::Bind(
        &webrtc::PeerConnectionObserver::OnAddTrack,
        base::Unretained(pc_handler_->observer()), receiver, receiver_streams));
  }

  void InvokeOnRemoveStream(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream) {
    for (const auto& audio_track : remote_stream->GetAudioTracks()) {
      InvokeOnRemoveTrack(audio_track);
    }
    for (const auto& video_track : remote_stream->GetVideoTracks()) {
      InvokeOnRemoveTrack(video_track);
    }
  }

  void InvokeOnRemoveTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>&
          remote_track) {
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver =
        receivers_by_track_.find(remote_track.get())->second;
    InvokeOnSignalingThread(
        base::Bind(&webrtc::PeerConnectionObserver::OnRemoveTrack,
                   base::Unretained(pc_handler_->observer()), receiver));
  }

  template <typename T>
  void InvokeAddTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream,
      T* webrtc_track) {
    InvokeOnSignalingThread(base::Bind(
        [](webrtc::MediaStreamInterface* remote_stream, T* webrtc_track) {
          EXPECT_TRUE(remote_stream->AddTrack(webrtc_track));
        },
        base::Unretained(remote_stream.get()), base::Unretained(webrtc_track)));
  }

  template <typename T>
  void InvokeRemoveTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream,
      T* webrtc_track) {
    InvokeOnSignalingThread(base::Bind(
        [](webrtc::MediaStreamInterface* remote_stream, T* webrtc_track) {
          EXPECT_TRUE(remote_stream->RemoveTrack(webrtc_track));
        },
        base::Unretained(remote_stream.get()), base::Unretained(webrtc_track)));
  }

  bool HasReceiverForEveryTrack(
      const rtc::scoped_refptr<webrtc::MediaStreamInterface>& remote_stream,
      const std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>>& receivers) {
    for (const auto& audio_track : remote_stream->GetAudioTracks()) {
      if (!HasReceiverForTrack(*audio_track.get(), receivers))
        return false;
    }
    for (const auto& video_track : remote_stream->GetAudioTracks()) {
      if (!HasReceiverForTrack(*video_track.get(), receivers))
        return false;
    }
    return true;
  }

  bool HasReceiverForTrack(
      const webrtc::MediaStreamTrackInterface& track,
      const std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>>& receivers) {
    for (const auto& receiver : receivers) {
      if (receiver->Track().Id().Utf8() == track.id())
        return true;
    }
    return false;
  }

  template <typename T>
  void InvokeOnSignalingThread(T callback) {
    mock_dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, std::move(callback));
    RunMessageLoopsUntilIdle();
  }

  // Wait for all current posts to the webrtc signaling thread to run and then
  // run the message loop until idle on the main thread.
  void RunMessageLoopsUntilIdle() {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    mock_dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                  base::Unretained(&waitable_event)));
    waitable_event.Wait();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void SignalWaitableEvent(base::WaitableEvent* waitable_event) {
    waitable_event->Signal();
  }

 public:
  // The ScopedTaskEnvironment prevents the ChildProcess from leaking a
  // TaskScheduler.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ChildProcess child_process_;
  std::unique_ptr<MockWebRTCPeerConnectionHandlerClient> mock_client_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> mock_dependency_factory_;
  std::unique_ptr<NiceMock<MockPeerConnectionTracker>> mock_tracker_;
  std::unique_ptr<RTCPeerConnectionHandlerUnderTest> pc_handler_;
  MockAudioDeviceFactory mock_audio_device_factory_;

  // Weak reference to the mocked native peer connection implementation.
  MockPeerConnectionImpl* mock_peer_connection_;

  std::vector<std::unique_ptr<RTCRtpSender>> senders_;
  std::map<webrtc::MediaStreamTrackInterface*,
           rtc::scoped_refptr<webrtc::RtpReceiverInterface>>
      receivers_by_track_;
};

TEST_F(RTCPeerConnectionHandlerTest, Destruct) {
  EXPECT_CALL(*mock_tracker_.get(), UnregisterPeerConnection(pc_handler_.get()))
      .Times(1);
  pc_handler_.reset(nullptr);
}

TEST_F(RTCPeerConnectionHandlerTest, NoCallbacksToClientAfterStop) {
  pc_handler_->Stop();

  EXPECT_CALL(*mock_client_.get(), NegotiationNeeded()).Times(0);
  pc_handler_->observer()->OnRenegotiationNeeded();

  EXPECT_CALL(*mock_client_.get(), DidGenerateICECandidate(_)).Times(0);
  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("sdpMid", 1, kDummySdp));
  pc_handler_->observer()->OnIceCandidate(native_candidate.get());

  EXPECT_CALL(*mock_client_.get(), DidChangeSignalingState(_)).Times(0);
  pc_handler_->observer()->OnSignalingChange(
      webrtc::PeerConnectionInterface::kHaveRemoteOffer);

  EXPECT_CALL(*mock_client_.get(), DidChangeIceGatheringState(_)).Times(0);
  pc_handler_->observer()->OnIceGatheringChange(
      webrtc::PeerConnectionInterface::kIceGatheringNew);

  EXPECT_CALL(*mock_client_.get(), DidChangeIceConnectionState(_)).Times(0);
  pc_handler_->observer()->OnIceConnectionChange(
      webrtc::PeerConnectionInterface::kIceConnectionDisconnected);

  EXPECT_CALL(*mock_client_.get(), DidAddReceiverPlanBForMock(_)).Times(0);
  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream("remote_stream", "video", "audio"));
  InvokeOnAddStream(remote_stream);

  EXPECT_CALL(*mock_client_.get(), DidRemoveReceiverPlanBForMock(_)).Times(0);
  InvokeOnRemoveStream(remote_stream);

  EXPECT_CALL(*mock_client_.get(), DidAddRemoteDataChannel(_)).Times(0);
  webrtc::DataChannelInit config;
  rtc::scoped_refptr<webrtc::DataChannelInterface> remote_data_channel(
      new rtc::RefCountedObject<MockDataChannel>("dummy", &config));
  pc_handler_->observer()->OnDataChannel(remote_data_channel);

  RunMessageLoopsUntilIdle();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateOffer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateOffer(pc_handler_.get(), _));

  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != nullptr);
  pc_handler_->CreateOffer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != nullptr);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateAnswer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateAnswer(pc_handler_.get(), _));
  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != nullptr);
  pc_handler_->CreateAnswer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != nullptr);
}

TEST_F(RTCPeerConnectionHandlerTest, setLocalDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), kDummySdp,
                                         kDummySdpType,
                                         PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_peer_connection_, SetLocalDescription(_, _));

  pc_handler_->SetLocalDescription(request, description);
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(description.GetType(), pc_handler_->LocalDescription().GetType());
  EXPECT_EQ(description.Sdp(), pc_handler_->LocalDescription().Sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->local_description() != nullptr);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->local_description()->type());
  mock_peer_connection_->local_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);

  // TODO(deadbeef): Also mock the "success" callback from the PeerConnection
  // and ensure that the sucessful result is tracked by PeerConnectionTracker.
}

// Test that setLocalDescription with invalid SDP will result in a failure, and
// is tracked as a failure with PeerConnectionTracker.
TEST_F(RTCPeerConnectionHandlerTest, setLocalDescriptionParseError) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  testing::InSequence sequence;
  // Expect two "Track" calls, one for the start of the attempt and one for the
  // failure.
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSetSessionDescription(pc_handler_.get(), kDummySdp, kDummySdpType,
                                 PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSessionDescriptionCallback(
          pc_handler_.get(),
          PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION, "OnFailure", _));

  // Used to simulate a parse failure.
  mock_dependency_factory_->SetFailToCreateSessionDescription(true);
  pc_handler_->SetLocalDescription(request, description);
  RunMessageLoopsUntilIdle();
  // A description that failed to be applied shouldn't be stored.
  EXPECT_TRUE(pc_handler_->LocalDescription().IsNull());
}

TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);

  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), kDummySdp,
                                         kDummySdpType,
                                         PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_peer_connection_, SetRemoteDescriptionForMock(_, _));

  pc_handler_->SetRemoteDescription(request, description);
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(description.GetType(), pc_handler_->RemoteDescription().GetType());
  EXPECT_EQ(description.Sdp(), pc_handler_->RemoteDescription().Sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->remote_description() != nullptr);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->remote_description()->type());
  mock_peer_connection_->remote_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);

  // TODO(deadbeef): Also mock the "success" callback from the PeerConnection
  // and ensure that the sucessful result is tracked by PeerConnectionTracker.
}

// Test that setRemoteDescription with invalid SDP will result in a failure, and
// is tracked as a failure with PeerConnectionTracker.
TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescriptionParseError) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.Initialize(kDummySdpType, kDummySdp);
  testing::InSequence sequence;
  // Expect two "Track" calls, one for the start of the attempt and one for the
  // failure.
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackSetSessionDescription(pc_handler_.get(), kDummySdp, kDummySdpType,
                                 PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSessionDescriptionCallback(
                  pc_handler_.get(),
                  PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION,
                  "OnFailure", _));

  // Used to simulate a parse failure.
  mock_dependency_factory_->SetFailToCreateSessionDescription(true);
  pc_handler_->SetRemoteDescription(request, description);
  RunMessageLoopsUntilIdle();
  // A description that failed to be applied shouldn't be stored.
  EXPECT_TRUE(pc_handler_->RemoteDescription().IsNull());
}

TEST_F(RTCPeerConnectionHandlerTest, setConfiguration) {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kPlanB;

  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetConfiguration(pc_handler_.get(), _));
  EXPECT_EQ(webrtc::RTCErrorType::NONE, pc_handler_->SetConfiguration(config));
}

// Test that when an error occurs in SetConfiguration, it's converted to a
// blink error and false is returned.
TEST_F(RTCPeerConnectionHandlerTest, setConfigurationError) {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kPlanB;

  mock_peer_connection_->set_setconfiguration_error_type(
      webrtc::RTCErrorType::INVALID_MODIFICATION);
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetConfiguration(pc_handler_.get(), _));
  EXPECT_EQ(webrtc::RTCErrorType::INVALID_MODIFICATION,
            pc_handler_->SetConfiguration(config));
}

TEST_F(RTCPeerConnectionHandlerTest, addICECandidate) {
  scoped_refptr<blink::WebRTCICECandidate> candidate =
      blink::WebRTCICECandidate::Create(kDummySdp, "sdpMid", 1);

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(), candidate,
                                   PeerConnectionTracker::SOURCE_REMOTE, true));
  EXPECT_TRUE(pc_handler_->AddICECandidate(candidate));
  EXPECT_EQ(kDummySdp, mock_peer_connection_->ice_sdp());
  EXPECT_EQ(1, mock_peer_connection_->sdp_mline_index());
  EXPECT_EQ("sdpMid", mock_peer_connection_->sdp_mid());
}

TEST_F(RTCPeerConnectionHandlerTest, addAndRemoveStream) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));

  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackAddTransceiver(
          pc_handler_.get(),
          PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack, _, _))
      .Times(2);
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackRemoveTransceiver(
          pc_handler_.get(),
          PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack, _, _))
      .Times(2);
  EXPECT_TRUE(AddStream(local_stream));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(2u, mock_peer_connection_->GetSenders().size());

  EXPECT_FALSE(AddStream(local_stream));
  EXPECT_TRUE(RemoveStream(local_stream));
  EXPECT_EQ(0u, mock_peer_connection_->GetSenders().size());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, addStreamWithStoppedAudioAndVideoTrack) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
      local_stream.AudioTracks();
  MediaStreamAudioSource* native_audio_source =
      MediaStreamAudioSource::From(audio_tracks[0].Source());
  native_audio_source->StopSource();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
      local_stream.VideoTracks();
  MediaStreamVideoSource* native_video_source =
      static_cast<MediaStreamVideoSource*>(
          video_tracks[0].Source().GetExtraData());
  native_video_source->StopSource();

  EXPECT_TRUE(AddStream(local_stream));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(2u, mock_peer_connection_->GetSenders().size());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsNoSelector) {
  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsAfterClose) {
  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->Stop();
  RunMessageLoopsUntilIdle();
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithLocalSelector) {
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream"));
  EXPECT_TRUE(AddStream(local_stream));
  blink::WebVector<blink::WebMediaStreamTrack> tracks =
      local_stream.AudioTracks();
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(tracks[0]);
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(1, request->result()->report_count());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithBadSelector) {
  // The setup is the same as GetStatsWithLocalSelector, but the stream is not
  // added to the PeerConnection.
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream_2"));
  blink::WebVector<blink::WebMediaStreamTrack> tracks =
      local_stream.AudioTracks();
  blink::WebMediaStreamTrack component = tracks[0];
  mock_peer_connection_->SetGetStatsResult(false);

  scoped_refptr<MockRTCStatsRequest> request(
      new rtc::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(component);
  pc_handler_->getStats(request.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ(0, request->result()->report_count());

  StopAllTracks(local_stream);
}

TEST_F(RTCPeerConnectionHandlerTest, GetRTCStats) {
  WhitelistStatsForTesting(webrtc::RTCTestStats::kType);

  rtc::scoped_refptr<webrtc::RTCStatsReport> report =
      webrtc::RTCStatsReport::Create();

  report->AddStats(std::unique_ptr<const webrtc::RTCStats>(
      new webrtc::RTCTestStats("RTCUndefinedStats", 1000)));

  std::unique_ptr<webrtc::RTCTestStats> stats_defined_members(
      new webrtc::RTCTestStats("RTCDefinedStats", 2000));
  stats_defined_members->m_bool = true;
  stats_defined_members->m_int32 = 42;
  stats_defined_members->m_uint32 = 42;
  stats_defined_members->m_int64 = 42;
  stats_defined_members->m_uint64 = 42;
  stats_defined_members->m_double = 42.0;
  stats_defined_members->m_string = "42";
  stats_defined_members->m_sequence_bool = ToSequence<bool>(true);
  stats_defined_members->m_sequence_int32 = ToSequence<int32_t>(42);
  stats_defined_members->m_sequence_uint32 = ToSequence<uint32_t>(42);
  stats_defined_members->m_sequence_int64 = ToSequence<int64_t>(42);
  stats_defined_members->m_sequence_uint64 = ToSequence<uint64_t>(42);
  stats_defined_members->m_sequence_double = ToSequence<double>(42);
  stats_defined_members->m_sequence_string = ToSequence<std::string>("42");
  report->AddStats(std::unique_ptr<const webrtc::RTCStats>(
      stats_defined_members.release()));

  pc_handler_->native_peer_connection()->SetGetStatsReport(report);
  std::unique_ptr<blink::WebRTCStatsReport> result;
  pc_handler_->GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>(
      new MockRTCStatsReportCallback(&result)));
  RunMessageLoopsUntilIdle();
  EXPECT_TRUE(result);

  int undefined_stats_count = 0;
  int defined_stats_count = 0;
  for (std::unique_ptr<blink::WebRTCStats> stats = result->Next(); stats;
       stats.reset(result->Next().release())) {
    EXPECT_EQ(stats->GetType().Utf8(), webrtc::RTCTestStats::kType);
    if (stats->Id().Utf8() == "RTCUndefinedStats") {
      ++undefined_stats_count;
      EXPECT_EQ(stats->Timestamp(), 1.0);
      for (size_t i = 0; i < stats->MembersCount(); ++i) {
        EXPECT_FALSE(stats->GetMember(i)->IsDefined());
      }
    } else if (stats->Id().Utf8() == "RTCDefinedStats") {
      ++defined_stats_count;
      EXPECT_EQ(stats->Timestamp(), 2.0);
      std::set<blink::WebRTCStatsMemberType> members;
      for (size_t i = 0; i < stats->MembersCount(); ++i) {
        std::unique_ptr<blink::WebRTCStatsMember> member = stats->GetMember(i);
        // TODO(hbos): A WebRTC-change is adding new members, this would cause
        // not all members to be defined. This if-statement saves Chromium from
        // crashing. As soon as the change has been rolled in, I will update
        // this test. crbug.com/627816
        if (!member->IsDefined())
          continue;
        EXPECT_TRUE(member->IsDefined());
        members.insert(member->GetType());
        switch (member->GetType()) {
          case blink::kWebRTCStatsMemberTypeBool:
            EXPECT_EQ(member->ValueBool(), true);
            break;
          case blink::kWebRTCStatsMemberTypeInt32:
            EXPECT_EQ(member->ValueInt32(), static_cast<int32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeUint32:
            EXPECT_EQ(member->ValueUint32(), static_cast<uint32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeInt64:
            EXPECT_EQ(member->ValueInt64(), static_cast<int64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeUint64:
            EXPECT_EQ(member->ValueUint64(), static_cast<uint64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeDouble:
            EXPECT_EQ(member->ValueDouble(), 42.0);
            break;
          case blink::kWebRTCStatsMemberTypeString:
            EXPECT_EQ(member->ValueString(), blink::WebString::FromUTF8("42"));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceBool:
            ExpectSequenceEquals(member->ValueSequenceBool(), 1);
            break;
          case blink::kWebRTCStatsMemberTypeSequenceInt32:
            ExpectSequenceEquals(member->ValueSequenceInt32(),
                                 static_cast<int32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceUint32:
            ExpectSequenceEquals(member->ValueSequenceUint32(),
                                 static_cast<uint32_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceInt64:
            ExpectSequenceEquals(member->ValueSequenceInt64(),
                                 static_cast<int64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceUint64:
            ExpectSequenceEquals(member->ValueSequenceUint64(),
                                 static_cast<uint64_t>(42));
            break;
          case blink::kWebRTCStatsMemberTypeSequenceDouble:
            ExpectSequenceEquals(member->ValueSequenceDouble(), 42.0);
            break;
          case blink::kWebRTCStatsMemberTypeSequenceString:
            ExpectSequenceEquals(member->ValueSequenceString(),
                                 blink::WebString::FromUTF8("42"));
            break;
          default:
            NOTREACHED();
        }
      }
      EXPECT_EQ(members.size(), static_cast<size_t>(14));
    } else {
      NOTREACHED();
    }
  }
  EXPECT_EQ(undefined_stats_count, 1);
  EXPECT_EQ(defined_stats_count, 1);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceConnectionChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::IceConnectionState new_state =
      webrtc::PeerConnectionInterface::kIceConnectionNew;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionNew));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionNew));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionChecking;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionChecking));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionChecking));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionConnected;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionConnected));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionConnected));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionCompleted;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionCompleted));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionCompleted));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionFailed;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionFailed));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionFailed));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionDisconnected));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionDisconnected));
  pc_handler_->observer()->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceConnectionStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceConnectionClosed));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceConnectionState(
                  webrtc::PeerConnectionInterface::kIceConnectionClosed));
  pc_handler_->observer()->OnIceConnectionChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceGatheringChange) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceGatheringStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceGatheringNew));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceGatheringState(
                  webrtc::PeerConnectionInterface::kIceGatheringNew));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceGatheringStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceGatheringGathering));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceGatheringState(
                  webrtc::PeerConnectionInterface::kIceGatheringGathering));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackIceGatheringStateChange(
                  pc_handler_.get(),
                  webrtc::PeerConnectionInterface::kIceGatheringComplete));
  EXPECT_CALL(*mock_client_.get(),
              DidChangeIceGatheringState(
                  webrtc::PeerConnectionInterface::kIceGatheringComplete));

  webrtc::PeerConnectionInterface::IceGatheringState new_state =
        webrtc::PeerConnectionInterface::kIceGatheringNew;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringGathering;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringComplete;
  pc_handler_->observer()->OnIceGatheringChange(new_state);

  // Check NULL candidate after ice gathering is completed.
  EXPECT_EQ("", mock_client_->candidate_mid());
  EXPECT_EQ(-1, mock_client_->candidate_mlineindex());
  EXPECT_EQ("", mock_client_->candidate_sdp());
}

// TODO(hbos): Enable when not mocking or remove test. https://crbug.com/788659
TEST_F(RTCPeerConnectionHandlerTest, DISABLED_OnAddAndOnRemoveStream) {
  rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream("remote_stream", "video", "audio"));
  // Grab the added receivers when it's been successfully added to the PC.
  std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers_added;
  EXPECT_CALL(*mock_client_.get(), DidAddReceiverPlanBForMock(_))
      .WillRepeatedly(
          Invoke([&receivers_added](
                     std::unique_ptr<blink::WebRTCRtpReceiver>* receiver) {
            receivers_added.push_back(std::move(*receiver));
          }));
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackAddTransceiver(
          pc_handler_.get(),
          PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack, _, _))
      .Times(2);
  // Grab the removed receivers when it's been successfully added to the PC.
  std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>> receivers_removed;
  EXPECT_CALL(
      *mock_tracker_.get(),
      TrackRemoveTransceiver(
          pc_handler_.get(),
          PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack, _, _))
      .Times(2);
  EXPECT_CALL(*mock_client_.get(), DidRemoveReceiverPlanBForMock(_))
      .WillRepeatedly(
          Invoke([&receivers_removed](
                     std::unique_ptr<blink::WebRTCRtpReceiver>* receiver) {
            receivers_removed.push_back(std::move(*receiver));
          }));

  InvokeOnAddStream(remote_stream);
  RunMessageLoopsUntilIdle();
  EXPECT_TRUE(HasReceiverForEveryTrack(remote_stream, receivers_added));
  InvokeOnRemoveStream(remote_stream);
  RunMessageLoopsUntilIdle();

  EXPECT_EQ(receivers_added.size(), 2u);
  EXPECT_EQ(receivers_added.size(), receivers_removed.size());
  EXPECT_EQ(receivers_added[0]->Id(), receivers_removed[0]->Id());
  EXPECT_EQ(receivers_added[1]->Id(), receivers_removed[1]->Id());
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceCandidate) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(), _,
                                   PeerConnectionTracker::SOURCE_LOCAL, true));
  EXPECT_CALL(*mock_client_.get(), DidGenerateICECandidate(_));

  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("sdpMid", 1, kDummySdp));
  pc_handler_->observer()->OnIceCandidate(native_candidate.get());
  RunMessageLoopsUntilIdle();
  EXPECT_EQ("sdpMid", mock_client_->candidate_mid());
  EXPECT_EQ(1, mock_client_->candidate_mlineindex());
  EXPECT_EQ(kDummySdp, mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnRenegotiationNeeded) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackOnRenegotiationNeeded(pc_handler_.get()));
  EXPECT_CALL(*mock_client_.get(), NegotiationNeeded());
  pc_handler_->observer()->OnRenegotiationNeeded();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDataChannel) {
  blink::WebString label = "d1";
  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDataChannel(pc_handler_.get(),
                                     testing::NotNull(),
                                     PeerConnectionTracker::SOURCE_LOCAL));
  std::unique_ptr<blink::WebRTCDataChannelHandler> channel(
      pc_handler_->CreateDataChannel("d1", blink::WebRTCDataChannelInit()));
  EXPECT_TRUE(channel.get() != nullptr);
  EXPECT_EQ(label, channel->Label());
  channel->SetClient(nullptr);
}

TEST_F(RTCPeerConnectionHandlerTest, IdIsOfExpectedFormat) {
  const std::string id = pc_handler_->Id().Ascii();
  constexpr size_t expected_length = 32u;
  EXPECT_EQ(id.length(), expected_length);
  EXPECT_EQ(id.length(), strspn(id.c_str(), "0123456789ABCDEF"));
}

TEST_F(RTCPeerConnectionHandlerTest, IdIsNotRepeated) {
  const auto other_pc_handler_ = CreateRTCPeerConnectionHandlerUnderTest();
  EXPECT_NE(pc_handler_->Id(), other_pc_handler_->Id());
}

}  // namespace content
