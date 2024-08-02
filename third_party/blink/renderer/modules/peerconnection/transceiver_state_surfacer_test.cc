// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/transceiver_state_surfacer.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::AnyNumber;
using testing::Return;

namespace blink {

// To avoid name collision on jumbo builds.
namespace transceiver_state_surfacer_test {

class MockSctpTransport : public webrtc::SctpTransportInterface {
 public:
  MOCK_CONST_METHOD0(dtls_transport,
                     rtc::scoped_refptr<webrtc::DtlsTransportInterface>());
  MOCK_CONST_METHOD0(Information, webrtc::SctpTransportInformation());
  MOCK_METHOD1(RegisterObserver, void(webrtc::SctpTransportObserverInterface*));
  MOCK_METHOD0(UnregisterObserver, void());
};

class TransceiverStateSurfacerTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_task_runner_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_adapter_map_ =
        base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
            dependency_factory_.Get(), main_task_runner_);
    surfacer_ = std::make_unique<TransceiverStateSurfacer>(
        main_task_runner_, signaling_task_runner());
    DummyExceptionStateForTesting exception_state;
    peer_connection_ = dependency_factory_->CreatePeerConnection(
        webrtc::PeerConnectionInterface::RTCConfiguration(), nullptr, nullptr,
        exception_state, /*rtp_transport=*/nullptr);
    EXPECT_CALL(
        *(static_cast<blink::MockPeerConnectionImpl*>(peer_connection_.get())),
        GetSctpTransport())
        .Times(AnyNumber());
  }

  void TearDown() override {
    // Make sure posted tasks get a chance to execute or else the stuff is
    // teared down while things are in flight.
    base::RunLoop().RunUntilIdle();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner() const {
    return dependency_factory_->GetWebRtcSignalingTaskRunner();
  }

  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  CreateLocalTrackAndAdapter(const std::string& id) {
    return track_adapter_map_->GetOrCreateLocalTrackAdapter(
        CreateLocalTrack(id));
  }

  rtc::scoped_refptr<blink::FakeRtpTransceiver> CreateWebRtcTransceiver(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> local_track,
      const std::string& local_stream_id,
      const std::string& remote_track_id,
      const std::string& remote_stream_id,
      rtc::scoped_refptr<webrtc::DtlsTransportInterface> transport) {
    rtc::scoped_refptr<blink::FakeRtpTransceiver> transceiver(
        new rtc::RefCountedObject<blink::FakeRtpTransceiver>(
            local_track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind
                ? cricket::MEDIA_TYPE_AUDIO
                : cricket::MEDIA_TYPE_VIDEO,
            CreateWebRtcSender(local_track, local_stream_id),
            CreateWebRtcReceiver(remote_track_id, remote_stream_id),
            std::nullopt, false, webrtc::RtpTransceiverDirection::kSendRecv,
            std::nullopt));
    if (transport.get()) {
      transceiver->SetTransport(transport);
    }
    return transceiver;
  }

  rtc::scoped_refptr<blink::FakeRtpSender> CreateWebRtcSender(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::string& stream_id) {
    return rtc::scoped_refptr<blink::FakeRtpSender>(
        new rtc::RefCountedObject<blink::FakeRtpSender>(
            std::move(track), std::vector<std::string>({stream_id})));
  }

  rtc::scoped_refptr<blink::FakeRtpReceiver> CreateWebRtcReceiver(
      const std::string& track_id,
      const std::string& stream_id) {
    rtc::scoped_refptr<webrtc::AudioTrackInterface> remote_track(
        blink::MockWebRtcAudioTrack::Create(track_id).get());
    rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<blink::MockMediaStream>(stream_id));
    return rtc::scoped_refptr<blink::FakeRtpReceiver>(
        new rtc::RefCountedObject<blink::FakeRtpReceiver>(
            remote_track,
            std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
                {remote_stream})));
  }

  // Initializes the surfacer on the signaling thread and signals the waitable
  // event when done. The WaitableEvent's Wait() blocks the main thread until
  // initialization occurs.
  std::unique_ptr<base::WaitableEvent> AsyncInitializeSurfacerWithWaitableEvent(
      std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          transceivers) {
    std::unique_ptr<base::WaitableEvent> waitable_event(new base::WaitableEvent(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED));
    signaling_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TransceiverStateSurfacerTest::
                AsyncInitializeSurfacerWithWaitableEventOnSignalingThread,
            base::Unretained(this), std::move(transceivers),
            waitable_event.get()));
    return waitable_event;
  }

  // Initializes the surfacer on the signaling thread and posts back to the main
  // thread to execute the callback when done. The RunLoop quits after the
  // callback is executed. Use the RunLoop's Run() method to allow the posted
  // task (such as the callback) to be executed while waiting. The caller must
  // let the loop Run() before destroying it.
  std::unique_ptr<base::RunLoop> AsyncInitializeSurfacerWithCallback(
      std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          transceivers,
      base::OnceCallback<void()> callback) {
    std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop());
    signaling_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TransceiverStateSurfacerTest::
                           AsyncInitializeSurfacerWithCallbackOnSignalingThread,
                       base::Unretained(this), std::move(transceivers),
                       std::move(callback), run_loop.get()));
    return run_loop;
  }

  void ObtainStatesAndExpectInitialized(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver) {
    // Inspect SCTP transport
    auto sctp_snapshot = surfacer_->SctpTransportSnapshot();
    EXPECT_EQ(peer_connection_->GetSctpTransport(), sctp_snapshot.transport);
    if (peer_connection_->GetSctpTransport()) {
      EXPECT_EQ(peer_connection_->GetSctpTransport()->dtls_transport(),
                sctp_snapshot.sctp_transport_state.dtls_transport());
    }
    // Inspect transceivers
    auto transceiver_states = surfacer_->ObtainStates();
    EXPECT_EQ(1u, transceiver_states.size());
    auto& transceiver_state = transceiver_states[0];
    EXPECT_EQ(transceiver_state.webrtc_transceiver().get(),
              webrtc_transceiver.get());
    // Inspect sender states.
    const auto& sender_state = transceiver_state.sender_state();
    EXPECT_TRUE(sender_state);
    EXPECT_TRUE(sender_state->is_initialized());
    const auto& webrtc_sender = webrtc_transceiver->sender();
    EXPECT_EQ(sender_state->webrtc_sender().get(), webrtc_sender.get());
    EXPECT_TRUE(sender_state->track_ref()->is_initialized());
    EXPECT_EQ(sender_state->track_ref()->webrtc_track(),
              webrtc_sender->track().get());
    EXPECT_EQ(sender_state->stream_ids(), webrtc_sender->stream_ids());
    EXPECT_EQ(sender_state->webrtc_dtls_transport(),
              webrtc_sender->dtls_transport());
    if (webrtc_sender->dtls_transport()) {
      EXPECT_EQ(webrtc_sender->dtls_transport()->Information().state(),
                sender_state->webrtc_dtls_transport_information().state());
    } else {
      EXPECT_EQ(webrtc::DtlsTransportState::kNew,
                sender_state->webrtc_dtls_transport_information().state());
    }
    // Inspect receiver states.
    const auto& receiver_state = transceiver_state.receiver_state();
    EXPECT_TRUE(receiver_state);
    EXPECT_TRUE(receiver_state->is_initialized());
    const auto& webrtc_receiver = webrtc_transceiver->receiver();
    EXPECT_EQ(receiver_state->webrtc_receiver().get(), webrtc_receiver.get());
    EXPECT_TRUE(receiver_state->track_ref()->is_initialized());
    EXPECT_EQ(receiver_state->track_ref()->webrtc_track(),
              webrtc_receiver->track().get());
    std::vector<std::string> receiver_stream_ids;
    for (const auto& stream : webrtc_receiver->streams()) {
      receiver_stream_ids.push_back(stream->id());
    }
    EXPECT_EQ(receiver_state->stream_ids(), receiver_stream_ids);
    EXPECT_EQ(receiver_state->webrtc_dtls_transport(),
              webrtc_receiver->dtls_transport());
    if (webrtc_receiver->dtls_transport()) {
      EXPECT_EQ(webrtc_receiver->dtls_transport()->Information().state(),
                receiver_state->webrtc_dtls_transport_information().state());
    } else {
      EXPECT_EQ(webrtc::DtlsTransportState::kNew,
                receiver_state->webrtc_dtls_transport_information().state());
    }
    // Inspect transceiver states.
    EXPECT_EQ(transceiver_state.mid(), webrtc_transceiver->mid());
    EXPECT_TRUE(transceiver_state.direction() ==
                webrtc_transceiver->direction());
    EXPECT_EQ(transceiver_state.current_direction(),
              webrtc_transceiver->current_direction());
  }

 private:
  MediaStreamComponent* CreateLocalTrack(const std::string& id) {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), true);
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

  void AsyncInitializeSurfacerWithWaitableEventOnSignalingThread(
      std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          transceivers,
      base::WaitableEvent* waitable_event) {
    DCHECK(signaling_task_runner()->BelongsToCurrentThread());
    surfacer_->Initialize(peer_connection_, track_adapter_map_,
                          std::move(transceivers));
    waitable_event->Signal();
  }

  void AsyncInitializeSurfacerWithCallbackOnSignalingThread(
      std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          transceivers,
      base::OnceCallback<void()> callback,
      base::RunLoop* run_loop) {
    DCHECK(signaling_task_runner()->BelongsToCurrentThread());
    surfacer_->Initialize(peer_connection_, track_adapter_map_,
                          std::move(transceivers));
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TransceiverStateSurfacerTest::
                           AsyncInitializeSurfacerWithCallbackOnMainThread,
                       base::Unretained(this), std::move(callback), run_loop));
  }

  void AsyncInitializeSurfacerWithCallbackOnMainThread(
      base::OnceCallback<void()> callback,
      base::RunLoop* run_loop) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    DCHECK(surfacer_->is_initialized());
    std::move(callback).Run();
    run_loop->Quit();
  }

 protected:
  test::TaskEnvironment task_environment_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  CrossThreadPersistent<MockPeerConnectionDependencyFactory>
      dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  std::unique_ptr<TransceiverStateSurfacer> surfacer_;
};

TEST_F(TransceiverStateSurfacerTest, SurfaceTransceiverBlockingly) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      local_track_adapter->webrtc_track(), "local_stream", "remote_track",
      "remote_stream", nullptr);
  auto waitable_event =
      AsyncInitializeSurfacerWithWaitableEvent({webrtc_transceiver});
  waitable_event->Wait();
  ObtainStatesAndExpectInitialized(webrtc_transceiver);
}

TEST_F(TransceiverStateSurfacerTest, SurfaceTransceiverInCallback) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      local_track_adapter->webrtc_track(), "local_stream", "remote_track",
      "remote_stream", nullptr);
  auto run_loop = AsyncInitializeSurfacerWithCallback(
      {webrtc_transceiver},
      base::BindOnce(
          &TransceiverStateSurfacerTest::ObtainStatesAndExpectInitialized,
          base::Unretained(this), webrtc_transceiver));
  run_loop->Run();
}

TEST_F(TransceiverStateSurfacerTest, SurfaceTransceiverWithTransport) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      local_track_adapter->webrtc_track(), "local_stream", "remote_track",
      "remote_stream",
      rtc::scoped_refptr<webrtc::DtlsTransportInterface>(
          new rtc::RefCountedObject<blink::FakeDtlsTransport>()));
  auto run_loop = AsyncInitializeSurfacerWithCallback(
      {webrtc_transceiver},
      base::BindOnce(
          &TransceiverStateSurfacerTest::ObtainStatesAndExpectInitialized,
          base::Unretained(this), webrtc_transceiver));
  run_loop->Run();
}

TEST_F(TransceiverStateSurfacerTest, SurfaceTransceiverWithSctpTransport) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      local_track_adapter->webrtc_track(), "local_stream", "remote_track",
      "remote_stream", nullptr);
  rtc::scoped_refptr<MockSctpTransport> mock_sctp_transport(
      new rtc::RefCountedObject<MockSctpTransport>());
  webrtc::SctpTransportInformation sctp_transport_info(
      webrtc::SctpTransportState::kNew);
  EXPECT_CALL(
      *(static_cast<blink::MockPeerConnectionImpl*>(peer_connection_.get())),
      GetSctpTransport())
      .WillRepeatedly(Return(mock_sctp_transport));
  EXPECT_CALL(*mock_sctp_transport.get(), Information())
      .WillRepeatedly(Return(sctp_transport_info));
  EXPECT_CALL(*mock_sctp_transport.get(), dtls_transport()).Times(AnyNumber());
  auto waitable_event =
      AsyncInitializeSurfacerWithWaitableEvent({webrtc_transceiver});
  waitable_event->Wait();
  EXPECT_TRUE(surfacer_->SctpTransportSnapshot().transport);
  ObtainStatesAndExpectInitialized(webrtc_transceiver);
}

}  // namespace transceiver_state_surfacer_test
}  // namespace blink
