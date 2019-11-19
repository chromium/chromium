// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_set_description_observer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/test/mock_peerconnectioninterface.h"
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
      scoped_refptr<webrtc::PeerConnectionInterface> pc,
      scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
      scoped_refptr<WebRtcSetDescriptionObserver> observer,
      bool surface_receivers_only)
      : signaling_task_runner_(std::move(signaling_task_runner)),
        handler_type_(handler_type),
        local_handler_(nullptr),
        remote_handler_(nullptr) {
    switch (handler_type_) {
      case ObserverHandlerType::kLocal:
        local_handler_ = WebRtcSetLocalDescriptionObserverHandler::Create(
            std::move(main_task_runner), signaling_task_runner_, std::move(pc),
            std::move(track_adapter_map), std::move(observer),
            surface_receivers_only);
        break;
      case ObserverHandlerType::kRemote:
        remote_handler_ = WebRtcSetRemoteDescriptionObserverHandler::Create(
            std::move(main_task_runner), signaling_task_runner_, std::move(pc),
            std::move(track_adapter_map), std::move(observer),
            surface_receivers_only);
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
    local_handler_->OnSuccess();
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
    local_handler_->OnFailure(std::move(error));
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

using TestVariety = std::tuple<ObserverHandlerType, StateSurfacerType>;

struct PrintToStringTestVariety {
  std::string operator()(
      const testing::TestParamInfo<TestVariety>& info) const {
    ObserverHandlerType handler_type = std::get<0>(info.param);
    StateSurfacerType surfacer_type = std::get<1>(info.param);
    std::string str;
    switch (handler_type) {
      case ObserverHandlerType::kLocal:
        str += "LocalDescriptionWith";
        break;
      case ObserverHandlerType::kRemote:
        str += "RemoteDescriptionWith";
        break;
    }
    switch (surfacer_type) {
      case StateSurfacerType::kTransceivers:
        str += "TransceiverStates";
        break;
      case StateSurfacerType::kReceiversOnly:
        str += "ReceiverStates";
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
//
// Each handler is testable under two modes of operation: surfacing state
// information about transceivers (includes both senders and receivers), or only
// surfacing receiver state information. Unified Plan requires the former and
// Plan B requires the latter.
//
// Parameterization allows easily running the same tests for each handler and
// mode, as specified by the TestVariety.
class WebRtcSetDescriptionObserverHandlerTest
    : public ::testing::TestWithParam<TestVariety> {
 public:
  WebRtcSetDescriptionObserverHandlerTest()
      : handler_type_(std::get<0>(GetParam())),
        surfacer_type_(std::get<1>(GetParam())) {}

  void SetUp() override {
    pc_ = new webrtc::MockPeerConnectionInterface;
    dependency_factory_.reset(new blink::MockPeerConnectionDependencyFactory());
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_adapter_map_ =
        base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
            dependency_factory_.get(), main_thread_);
    observer_ = base::MakeRefCounted<WebRtcSetDescriptionObserverForTest>();
    observer_handler_ = std::make_unique<ObserverHandlerWrapper>(
        handler_type_, main_thread_,
        dependency_factory_->GetWebRtcSignalingTaskRunner(), pc_,
        track_adapter_map_, observer_,
        surfacer_type_ == StateSurfacerType::kReceiversOnly);
  }

  void TearDown() override { blink::WebHeap::CollectAllGarbageForTesting(); }

  blink::WebMediaStreamTrack CreateLocalTrack(const std::string& id) {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(
        blink::WebString::FromUTF8(id), blink::WebMediaStreamSource::kTypeAudio,
        blink::WebString::FromUTF8("local_audio_track"), false);
    blink::MediaStreamAudioSource* audio_source =
        new blink::MediaStreamAudioSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    // Takes ownership of |audio_source|.
    web_source.SetPlatformSource(base::WrapUnique(audio_source));

    blink::WebMediaStreamTrack web_track;
    web_track.Initialize(web_source.Id(), web_source);
    audio_source->ConnectToTrack(web_track);
    return web_track;
  }

  void CreateTransceivers() {
    ASSERT_EQ(StateSurfacerType::kTransceivers, surfacer_type_);

    auto web_local_track = CreateLocalTrack("local_track");
    auto local_track_adapter =
        track_adapter_map_->GetOrCreateLocalTrackAdapter(web_local_track);
    scoped_refptr<webrtc::MediaStreamTrackInterface> local_track =
        local_track_adapter->webrtc_track();
    rtc::scoped_refptr<blink::FakeRtpSender> sender(
        new rtc::RefCountedObject<blink::FakeRtpSender>(
            local_track.get(), std::vector<std::string>({"local_stream"})));
    // A requirement of WebRtcSet[Local/Remote]DescriptionObserverHandler is
    // that local tracks have existing track adapters when the callback is
    // invoked. In practice this would be ensured by RTCPeerConnectionHandler.
    // Here in testing, we ensure it by adding it to |local_track_adapters_|.
    local_track_adapters_.push_back(std::move(local_track_adapter));

    scoped_refptr<blink::MockWebRtcAudioTrack> remote_track =
        blink::MockWebRtcAudioTrack::Create("remote_track");
    scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<blink::MockMediaStream>("remote_stream"));
    rtc::scoped_refptr<blink::FakeRtpReceiver> receiver(
        new rtc::RefCountedObject<blink::FakeRtpReceiver>(
            remote_track.get(),
            std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
                {remote_stream.get()})));
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver(
        new rtc::RefCountedObject<blink::FakeRtpTransceiver>(
            cricket::MEDIA_TYPE_AUDIO, sender, receiver, base::nullopt, false,
            webrtc::RtpTransceiverDirection::kSendRecv, base::nullopt));
    transceivers_.push_back(transceiver);
    EXPECT_CALL(*pc_, GetTransceivers()).WillRepeatedly(Return(transceivers_));
  }

  void ExpectMatchingTransceivers() {
    ASSERT_EQ(StateSurfacerType::kTransceivers, surfacer_type_);
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
    EXPECT_TRUE(
        blink::OptionalEquals(transceiver_state.mid(), transceiver->mid()));
    EXPECT_EQ(transceiver_state.stopped(), transceiver->stopped());
    EXPECT_TRUE(transceiver_state.direction() == transceiver->direction());
    EXPECT_TRUE(blink::OptionalEquals(transceiver_state.current_direction(),
                                      transceiver->current_direction()));
    EXPECT_TRUE(blink::OptionalEquals(transceiver_state.fired_direction(),
                                      transceiver->fired_direction()));
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

  void CreateReceivers() {
    ASSERT_EQ(StateSurfacerType::kReceiversOnly, surfacer_type_);

    scoped_refptr<blink::MockWebRtcAudioTrack> remote_track =
        blink::MockWebRtcAudioTrack::Create("remote_track");
    scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<blink::MockMediaStream>("remote_stream"));
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver(
        new rtc::RefCountedObject<blink::FakeRtpReceiver>(
            remote_track.get(),
            std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
                {remote_stream.get()})));
    receivers_.push_back(receiver);
    EXPECT_CALL(*pc_, GetReceivers()).WillRepeatedly(Return(receivers_));
  }

  void ExpectMatchingReceivers() {
    ASSERT_EQ(StateSurfacerType::kReceiversOnly, surfacer_type_);
    ASSERT_EQ(1u, receivers_.size());

    auto receiver = receivers_[0];
    EXPECT_EQ(1u, observer_->states().transceiver_states.size());
    const blink::RtpTransceiverState& transceiver_state =
        observer_->states().transceiver_states[0];
    EXPECT_FALSE(transceiver_state.sender_state());
    EXPECT_TRUE(transceiver_state.receiver_state());
    const blink::RtpReceiverState& receiver_state =
        *transceiver_state.receiver_state();
    EXPECT_TRUE(receiver_state.is_initialized());
    EXPECT_EQ(receiver.get(), receiver_state.webrtc_receiver());
    EXPECT_EQ(receiver->track(), receiver_state.track_ref()->webrtc_track());
    EXPECT_EQ(receiver->stream_ids(), receiver_state.stream_ids());
  }

 protected:
  scoped_refptr<webrtc::MockPeerConnectionInterface> pc_;
  std::unique_ptr<blink::MockPeerConnectionDependencyFactory>
      dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  scoped_refptr<WebRtcSetDescriptionObserverForTest> observer_;

  ObserverHandlerType handler_type_;
  StateSurfacerType surfacer_type_;
  std::unique_ptr<ObserverHandlerWrapper> observer_handler_;

  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      transceivers_;
  std::vector<
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>>
      local_track_adapters_;
  // Used instead of |transceivers_| when |surfacer_type_| is
  // StateSurfacerType::kReceiversOnly.
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receivers_;
};

TEST_P(WebRtcSetDescriptionObserverHandlerTest, OnSuccess) {
  switch (surfacer_type_) {
    case StateSurfacerType::kTransceivers:
      CreateTransceivers();
      break;
    case StateSurfacerType::kReceiversOnly:
      CreateReceivers();
      break;
  }

  EXPECT_CALL(*pc_, signaling_state())
      .WillRepeatedly(Return(webrtc::PeerConnectionInterface::kStable));

  observer_handler_->InvokeOnComplete(webrtc::RTCError::OK());
  EXPECT_TRUE(observer_->called());
  EXPECT_TRUE(observer_->error().ok());

  EXPECT_EQ(webrtc::PeerConnectionInterface::kStable,
            observer_->states().signaling_state);

  switch (surfacer_type_) {
    case StateSurfacerType::kTransceivers:
      ExpectMatchingTransceivers();
      break;
    case StateSurfacerType::kReceiversOnly:
      ExpectMatchingReceivers();
      break;
  }
}

TEST_P(WebRtcSetDescriptionObserverHandlerTest, OnFailure) {
  switch (surfacer_type_) {
    case StateSurfacerType::kTransceivers:
      CreateTransceivers();
      break;
    case StateSurfacerType::kReceiversOnly:
      CreateReceivers();
      break;
  }

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

  switch (surfacer_type_) {
    case StateSurfacerType::kTransceivers:
      ExpectMatchingTransceivers();
      break;
    case StateSurfacerType::kReceiversOnly:
      ExpectMatchingReceivers();
      break;
  }
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
  switch (surfacer_type_) {
    case StateSurfacerType::kTransceivers:
      CreateTransceivers();
      break;
    case StateSurfacerType::kReceiversOnly:
      CreateReceivers();
      break;
  }

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

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebRtcSetDescriptionObserverHandlerTest,
    ::testing::Values(std::make_tuple(ObserverHandlerType::kLocal,
                                      StateSurfacerType::kTransceivers),
                      std::make_tuple(ObserverHandlerType::kRemote,
                                      StateSurfacerType::kTransceivers),
                      std::make_tuple(ObserverHandlerType::kLocal,
                                      StateSurfacerType::kReceiversOnly),
                      std::make_tuple(ObserverHandlerType::kRemote,
                                      StateSurfacerType::kReceiversOnly)),
    PrintToStringTestVariety());

}  // namespace blink
