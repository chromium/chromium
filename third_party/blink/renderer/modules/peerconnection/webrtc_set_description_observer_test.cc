// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_set_description_observer.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/mock_peer_connection_interface.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/media/base/fake_media_engine.h"

using ::testing::Return;

namespace blink {

class WebRtcSetDescriptionObserverForTest
    : public WebRtcSetDescriptionObserver {
 public:
  bool called() const { return called_; }

  const WebRtcSetDescriptionObserver::States& states() const {
    DCHECK(called_);
    return states_;
  }
  const webrtc::RTCError& error() const {
    DCHECK(called_);
    return error_;
  }

  // WebRtcSetDescriptionObserver implementation.
  void OnSetDescriptionComplete(
      webrtc::RTCError error,
      WebRtcSetDescriptionObserver::States states) override {
    called_ = true;
    error_ = std::move(error);
    states_ = std::move(states);
  }

 private:
  ~WebRtcSetDescriptionObserverForTest() override {}

  bool called_ = false;
  webrtc::RTCError error_;
  WebRtcSetDescriptionObserver::States states_;
};

enum class ObserverHandlerType {
  kLocal,
  kRemote,
};

// Because webrtc observer interfaces are different classes,
// WebRtcSetLocalDescriptionObserverHandler and
// WebRtcSetRemoteDescriptionObserverHandler have different class hierarchies
// despite implementing the same behavior. This wrapper hides these differences.
class ObserverHandlerWrapper {
 public:
  ObserverHandlerWrapper(
      ObserverHandlerType handler_type,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
      scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
      scoped_refptr<WebRtcSetDescriptionObserver> observer)
      : signaling_task_runner_(std::move(signaling_task_runner)),
        handler_type_(handler_type),
        local_handler_(nullptr),
        remote_handler_(nullptr) {
    switch (handler_type_) {
      case ObserverHandlerType::kLocal:
        local_handler_ = WebRtcSetLocalDescriptionObserverHandler::Create(
            std::move(main_task_runner), signaling_task_runner_, std::move(pc),
            std::move(track_adapter_map), std::move(observer));
        break;
      case ObserverHandlerType::kRemote:
        remote_handler_ = WebRtcSetRemoteDescriptionObserverHandler::Create(
            std::move(main_task_runner), signaling_task_runner_, std::move(pc),
            std::move(track_adapter_map), std::move(observer));
        break;
    }
  }

  void InvokeOnComplete(webrtc::RTCError error) {
    switch (handler_type_) {
      case ObserverHandlerType::kLocal:
        if (error.ok())
          InvokeLocalHandlerOnSuccess();
        else
          InvokeLocalHandlerOnFailure(std::move(error));
        break;
      case ObserverHandlerType::kRemote:
        InvokeRemoteHandlerOnComplete(std::move(error));
        break;
    }
  }

 private:
  void InvokeLocalHandlerOnSuccess() {
    base::RunLoop run_loop;
    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ObserverHandlerWrapper::
                           InvokeLocalHandlerOnSuccessOnSignalingThread,
                       base::Unretained(this), base::Unretained(&run_loop)));
    run_loop.Run();
  }
  void InvokeLocalHandlerOnSuccessOnSignalingThread(base::RunLoop* run_loop) {
    local_handler_->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
    run_loop->Quit();
  }

  void InvokeLocalHandlerOnFailure(webrtc::RTCError error) {
    base::RunLoop run_loop;
    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ObserverHandlerWrapper::
                           InvokeLocalHandlerOnFailureOnSignalingThread,
                       base::Unretained(this), std::move(error),
                       base::Unretained(&run_loop)));
    run_loop.Run();
  }
  void InvokeLocalHandlerOnFailureOnSignalingThread(webrtc::RTCError error,
                                                    base::RunLoop* run_loop) {
    local_handler_->OnSetLocalDescriptionComplete(std::move(error));
    run_loop->Quit();
  }

  void InvokeRemoteHandlerOnComplete(webrtc::RTCError error) {
    base::RunLoop run_loop;
    signaling_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ObserverHandlerWrapper::
                           InvokeRemoteHandlerOnCompleteOnSignalingThread,
                       base::Unretained(this), std::move(error),
                       base::Unretained(&run_loop)));
    run_loop.Run();
  }
  void InvokeRemoteHandlerOnCompleteOnSignalingThread(webrtc::RTCError error,
                                                      base::RunLoop* run_loop) {
    remote_handler_->OnSetRemoteDescriptionComplete(std::move(error));
    run_loop->Quit();
  }

  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner_;
  ObserverHandlerType handler_type_;
  scoped_refptr<WebRtcSetLocalDescriptionObserverHandler> local_handler_;
  scoped_refptr<WebRtcSetRemoteDescriptionObserverHandler> remote_handler_;
};

enum class StateSurfacerType {
  kTransceivers,
  kReceiversOnly,
};

struct PrintToStringObserverHandlerType {
  std::string operator()(
      const testing::TestParamInfo<ObserverHandlerType>& info) const {
    ObserverHandlerType handler_type = info.param;
    std::string str;
    switch (handler_type) {
      case ObserverHandlerType::kLocal:
        str += "LocalDescription";
        break;
      case ObserverHandlerType::kRemote:
        str += "RemoteDescription";
        break;
    }
    return str;
  }
};

// Using parameterization, this class is used to test both
// WebRtcSetLocalDescriptionObserverHandler and
// WebRtcSetRemoteDescriptionObserverHandler. The handlers, used for
// setLocalDescription() and setRemoteDescription() respectively, are virtually
// identical in terms of functionality but have different class hierarchies due
// to webrtc observer interfaces being different classes.
class WebRtcSetDescriptionObserverHandlerTest
    : public ::testing::TestWithParam<ObserverHandlerType> {
 public:
  WebRtcSetDescriptionObserverHandlerTest() : handler_type_(GetParam()) {}

  void SetUp() override {
    pc_ = new MockPeerConnectionInterface;
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_adapter_map_ =
        base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
            dependency_factory_.Get(), main_thread_);
    observer_ = base::MakeRefCounted<WebRtcSetDescriptionObserverForTest>();
    observer_handler_ = std::make_unique<ObserverHandlerWrapper>(
        handler_type_, main_thread_,
        dependency_factory_->GetWebRtcSignalingTaskRunner(), pc_,
        track_adapter_map_, observer_);
  }

  void TearDown() override { blink::WebHeap::CollectAllGarbageForTesting(); }

  MediaStreamComponent* CreateLocalTrack(const std::string& id) {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    auto* audio_source_ptr = audio_source.get();
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8(id), MediaStreamSource::kTypeAudio,
        String::FromUTF8("local_audio_track"), false, std::move(audio_source));

    auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
        source->Id(), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    audio_source_ptr->ConnectToInitializedTrack(component);
    return component;
  }

  void CreateTransceivers() {
    auto* component = CreateLocalTrack("local_track");
    auto local_track_adapter =
        track_adapter_map_->GetOrCreateLocalTrackAdapter(component);
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> local_track =
        local_track_adapter->webrtc_track();
    rtc::scoped_refptr<blink::FakeRtpSender> sender(
        new rtc::RefCountedObject<blink::FakeRtpSender>(
            local_track, std::vector<std::string>({"local_stream"})));
    // A requirement of WebRtcSet[Local/Remote]DescriptionObserverHandler is
    // that local tracks have existing track adapters when the callback is
    // invoked. In practice this would be ensured by RTCPeerConnectionHandler.
    // Here in testing, we ensure it by adding it to |local_track_adapters_|.
    local_track_adapters_.push_back(std::move(local_track_adapter));

    scoped_refptr<blink::MockWebRtcAudioTrack> remote_track =
        blink::MockWebRtcAudioTrack::Create("remote_track");
    rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<blink::MockMediaStream>("remote_stream"));
    rtc::scoped_refptr<blink::FakeRtpReceiver> receiver(
        new rtc::RefCountedObject<blink::FakeRtpReceiver>(
            rtc::scoped_refptr<blink::MockWebRtcAudioTrack>(remote_track.get()),
            std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
                {remote_stream})));
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver(
        new rtc::RefCountedObject<blink::FakeRtpTransceiver>(
            cricket::MEDIA_TYPE_AUDIO, sender, receiver, std::nullopt, false,
            webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt));
    transceivers_.push_back(transceiver);
    EXPECT_CALL(*pc_, GetTransceivers()).WillRepeatedly(Return(transceivers_));
  }

  void ExpectMatchingTransceivers() {
    ASSERT_EQ(1u, transceivers_.size());
    auto transceiver = transceivers_[0];
    auto sender = transceiver->sender();
    auto receiver = transceiver->receiver();
    EXPECT_EQ(1u, observer_->states().transceiver_states.size());
    const blink::RtpTransceiverState& transceiver_state =
        observer_->states().transceiver_states[0];
    // Inspect transceiver states.
    EXPECT_TRUE(transceiver_state.is_initialized());
    EXPECT_EQ(transceiver.get(), transceiver_state.webrtc_transceiver());
    EXPECT_EQ(transceiver_state.mid(), transceiver->mid());
    EXPECT_TRUE(transceiver_state.direction() == transceiver->direction());
    EXPECT_EQ(transceiver_state.current_direction(),
              transceiver->current_direction());
    EXPECT_EQ(transceiver_state.fired_direction(),
              transceiver->fired_direction());
    // Inspect sender states.
    EXPECT_TRUE(transceiver_state.sender_state());
    const blink::RtpSenderState& sender_state =
        *transceiver_state.sender_state();
    EXPECT_TRUE(sender_state.is_initialized());
    EXPECT_EQ(sender.get(), sender_state.webrtc_sender());
    EXPECT_EQ(sender->track(), sender_state.track_ref()->webrtc_track());
    EXPECT_EQ(sender->stream_ids(), sender_state.stream_ids());
    // Inspect receiver states.
    EXPECT_TRUE(transceiver_state.receiver_state());
    const blink::RtpReceiverState& receiver_state =
        *transceiver_state.receiver_state();
    EXPECT_TRUE(receiver_state.is_initialized());
    EXPECT_EQ(receiver.get(), receiver_state.webrtc_receiver());
    EXPECT_EQ(receiver->track(), receiver_state.track_ref()->webrtc_track());
    EXPECT_EQ(receiver->stream_ids(), receiver_state.stream_ids());
  }

 protected:
  test::TaskEnvironment task_environment_;
  rtc::scoped_refptr<MockPeerConnectionInterface> pc_;
  Persistent<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  scoped_refptr<WebRtcSetDescriptionObserverForTest> observer_;

  ObserverHandlerType handler_type_;
  std::unique_ptr<ObserverHandlerWrapper> observer_handler_;

  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      transceivers_;
  std::vector<
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>>
      local_track_adapters_;
};

TEST_P(WebRtcSetDescriptionObserverHandlerTest, OnSuccess) {
  CreateTransceivers();

  EXPECT_CALL(*pc_, signaling_state())
      .WillRepeatedly(Return(webrtc::PeerConnectionInterface::kStable));

  observer_handler_->InvokeOnComplete(webrtc::RTCError::OK());
  EXPECT_TRUE(observer_->called());
  EXPECT_TRUE(observer_->error().ok());

  EXPECT_EQ(webrtc::PeerConnectionInterface::kStable,
            observer_->states().signaling_state);

  ExpectMatchingTransceivers();
}

TEST_P(WebRtcSetDescriptionObserverHandlerTest, OnFailure) {
  CreateTransceivers();

  EXPECT_CALL(*pc_, signaling_state())
      .WillRepeatedly(Return(webrtc::PeerConnectionInterface::kStable));

  observer_handler_->InvokeOnComplete(
      webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER, "Oh noes!"));
  EXPECT_TRUE(observer_->called());
  EXPECT_FALSE(observer_->error().ok());
  EXPECT_EQ(std::string("Oh noes!"), observer_->error().message());

  // Verify states were surfaced even though we got an error.
  EXPECT_EQ(webrtc::PeerConnectionInterface::kStable,
            observer_->states().signaling_state);

  ExpectMatchingTransceivers();
}

// Test coverage for https://crbug.com/897251. If the webrtc peer connection is
// implemented to invoke the callback with a delay it might already have been
// closed when the observer is invoked. A closed RTCPeerConnection is allowed to
// be garbage collected. In rare circumstances, the RTCPeerConnection,
// RTCPeerConnectionHandler and any local track adapters may thus have been
// deleted when the observer attempts to surface transceiver state information.
// This test insures that TransceiverStateSurfacer::Initialize() does not crash
// due to track adapters not existing.
TEST_P(WebRtcSetDescriptionObserverHandlerTest,
       ClosePeerConnectionBeforeCallback) {
  CreateTransceivers();

  // Simulate the peer connection having been closed and local track adapters
  // destroyed before the observer was invoked.
  EXPECT_CALL(*pc_, signaling_state())
      .WillRepeatedly(Return(webrtc::PeerConnectionInterface::kClosed));
  local_track_adapters_.clear();

  observer_handler_->InvokeOnComplete(webrtc::RTCError::OK());
  EXPECT_TRUE(observer_->called());
  EXPECT_TRUE(observer_->error().ok());

  EXPECT_EQ(webrtc::PeerConnectionInterface::kClosed,
            observer_->states().signaling_state);

  EXPECT_EQ(0u, observer_->states().transceiver_states.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcSetDescriptionObserverHandlerTest,
                         ::testing::Values(ObserverHandlerType::kLocal,
                                           ObserverHandlerType::kRemote),
                         PrintToStringObserverHandlerType());

}  // namespace blink
