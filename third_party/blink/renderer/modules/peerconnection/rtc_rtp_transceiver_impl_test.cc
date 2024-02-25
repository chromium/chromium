// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver_impl.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class RTCRtpTransceiverImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_task_runner_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_map_ = base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
        dependency_factory_.Get(), main_task_runner_);
    peer_connection_ = new rtc::RefCountedObject<blink::MockPeerConnectionImpl>(
        dependency_factory_.Get(), nullptr);
  }

  void TearDown() override {
    // Syncing up with the signaling thread ensures any pending operations on
    // that thread are executed. If they post back to the main thread, such as
    // the sender or receiver destructor traits, this is allowed to execute
    // before the test shuts down the threads.
    SyncWithSignalingThread();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  // Wait for the signaling thread to perform any queued tasks, executing tasks
  // posted to the current thread in the meantime while waiting.
  void SyncWithSignalingThread() const {
    base::RunLoop run_loop;
    dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner() const {
    return dependency_factory_->GetWebRtcSignalingTaskRunner();
  }

  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  CreateLocalTrackAndAdapter(const std::string& id) {
    return track_map_->GetOrCreateLocalTrackAdapter(CreateLocalTrack(id));
  }

  std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  CreateRemoteTrackAndAdapter(const std::string& id) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track(
        blink::MockWebRtcAudioTrack::Create(id).get());
    std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
        track_ref;
    base::RunLoop run_loop;
    signaling_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCRtpTransceiverImplTest::
                           CreateRemoteTrackAdapterOnSignalingThread,
                       base::Unretained(this), std::move(webrtc_track),
                       base::Unretained(&track_ref),
                       base::Unretained(&run_loop)));
    run_loop.Run();
    DCHECK(track_ref);
    return track_ref;
  }

  rtc::scoped_refptr<blink::FakeRtpSender> CreateWebRtcSender(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::string& stream_id) {
    return rtc::scoped_refptr<blink::FakeRtpSender>(
        new rtc::RefCountedObject<blink::FakeRtpSender>(
            std::move(track), std::vector<std::string>({stream_id})));
  }

  rtc::scoped_refptr<blink::FakeRtpReceiver> CreateWebRtcReceiver(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::string& stream_id) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<blink::MockMediaStream>(stream_id));
    return rtc::scoped_refptr<blink::FakeRtpReceiver>(
        new rtc::RefCountedObject<blink::FakeRtpReceiver>(
            track,
            std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
                {remote_stream})));
  }

  rtc::scoped_refptr<blink::FakeRtpTransceiver> CreateWebRtcTransceiver(
      rtc::scoped_refptr<blink::FakeRtpSender> sender,
      rtc::scoped_refptr<blink::FakeRtpReceiver> receiver,
      std::optional<std::string> mid,
      webrtc::RtpTransceiverDirection direction,
      std::optional<webrtc::RtpTransceiverDirection> current_direction) {
    DCHECK(!sender->track() ||
           sender->track()->kind() == receiver->track()->kind());
    return rtc::scoped_refptr<blink::FakeRtpTransceiver>(
        new rtc::RefCountedObject<blink::FakeRtpTransceiver>(
            receiver->track()->kind() ==
                    webrtc::MediaStreamTrackInterface::kAudioKind
                ? cricket::MEDIA_TYPE_AUDIO
                : cricket::MEDIA_TYPE_VIDEO,
            std::move(sender), std::move(receiver), std::move(mid), false,
            direction, std::move(current_direction)));
  }

  RtpTransceiverState CreateTransceiverState(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver,
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          sender_track_ref,
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          receiver_track_ref) {
    std::vector<std::string> receiver_stream_ids;
    for (const auto& stream : webrtc_transceiver->receiver()->streams()) {
      receiver_stream_ids.push_back(stream->id());
    }
    return RtpTransceiverState(
        main_task_runner_, signaling_task_runner(), webrtc_transceiver.get(),
        blink::RtpSenderState(main_task_runner_, signaling_task_runner(),
                              webrtc_transceiver->sender(),
                              std::move(sender_track_ref),
                              webrtc_transceiver->sender()->stream_ids()),
        blink::RtpReceiverState(main_task_runner_, signaling_task_runner(),
                                webrtc_transceiver->receiver().get(),
                                std::move(receiver_track_ref),
                                std::move(receiver_stream_ids)),
        webrtc_transceiver->mid(), webrtc_transceiver->direction(),
        webrtc_transceiver->current_direction(),
        webrtc_transceiver->fired_direction(), {});
  }

 protected:
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

  void CreateRemoteTrackAdapterOnSignalingThread(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>*
          track_ref,
      base::RunLoop* run_loop) {
    *track_ref = track_map_->GetOrCreateRemoteTrackAdapter(webrtc_track.get());
    run_loop->Quit();
  }

 private:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

 protected:
  Persistent<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map_;
  rtc::scoped_refptr<blink::MockPeerConnectionImpl> peer_connection_;
};

TEST_F(RTCRtpTransceiverImplTest, InitializeTransceiverState) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      CreateWebRtcSender(local_track_adapter->webrtc_track(), "local_stream"),
      CreateWebRtcReceiver(remote_track_adapter->webrtc_track(),
                           "remote_stream"),
      std::nullopt, webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt);
  RtpTransceiverState transceiver_state =
      CreateTransceiverState(webrtc_transceiver, std::move(local_track_adapter),
                             std::move(remote_track_adapter));
  EXPECT_FALSE(transceiver_state.is_initialized());
  transceiver_state.Initialize();

  EXPECT_TRUE(transceiver_state.is_initialized());
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
  // Inspect transceiver states.
  EXPECT_EQ(transceiver_state.mid(), webrtc_transceiver->mid());
  EXPECT_TRUE(transceiver_state.direction() == webrtc_transceiver->direction());
  EXPECT_EQ(transceiver_state.current_direction(),
            webrtc_transceiver->current_direction());
  EXPECT_EQ(transceiver_state.fired_direction(),
            webrtc_transceiver->fired_direction());
}

TEST_F(RTCRtpTransceiverImplTest, CreateTranceiver) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      CreateWebRtcSender(local_track_adapter->webrtc_track(), "local_stream"),
      CreateWebRtcReceiver(remote_track_adapter->webrtc_track(),
                           "remote_stream"),
      std::nullopt, webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt);
  RtpTransceiverState transceiver_state =
      CreateTransceiverState(webrtc_transceiver, std::move(local_track_adapter),
                             std::move(remote_track_adapter));
  EXPECT_FALSE(transceiver_state.is_initialized());
  transceiver_state.Initialize();

  RTCRtpTransceiverImpl transceiver(peer_connection_, track_map_,
                                    std::move(transceiver_state),
                                    /*encoded_insertable_streams=*/false,
                                    /*decode_metronome=*/nullptr);
  EXPECT_TRUE(transceiver.Mid().IsNull());
  EXPECT_TRUE(transceiver.Sender());
  EXPECT_TRUE(transceiver.Receiver());
  EXPECT_EQ(transceiver.Direction(),
            webrtc::RtpTransceiverDirection::kSendRecv);
  EXPECT_FALSE(transceiver.CurrentDirection());
  EXPECT_FALSE(transceiver.FiredDirection());
}

TEST_F(RTCRtpTransceiverImplTest, ModifyTransceiver) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_sender =
      CreateWebRtcSender(local_track_adapter->webrtc_track(), "local_stream");
  auto webrtc_receiver = CreateWebRtcReceiver(
      remote_track_adapter->webrtc_track(), "remote_stream");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      webrtc_sender, webrtc_receiver, std::nullopt,
      webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt);

  // Create initial state.
  RtpTransceiverState initial_transceiver_state =
      CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                             remote_track_adapter->Copy());
  EXPECT_FALSE(initial_transceiver_state.is_initialized());
  initial_transceiver_state.Initialize();

  // Modify the webrtc transceiver and create a new state object for the
  // modified state.
  webrtc_transceiver->ReplaceWith(
      *CreateWebRtcTransceiver(webrtc_sender, webrtc_receiver, "MidyMacMidface",
                               webrtc::RtpTransceiverDirection::kInactive,
                               webrtc::RtpTransceiverDirection::kSendRecv));
  RtpTransceiverState modified_transceiver_state =
      CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                             remote_track_adapter->Copy());
  EXPECT_FALSE(modified_transceiver_state.is_initialized());
  modified_transceiver_state.Initialize();

  // Modifying the webrtc transceiver after the initial state was created should
  // not have affected the transceiver state.
  RTCRtpTransceiverImpl transceiver(peer_connection_, track_map_,
                                    std::move(initial_transceiver_state),
                                    /*encoded_insertable_streams=*/false,
                                    /*decode_metronome=*/nullptr);
  EXPECT_TRUE(transceiver.Mid().IsNull());
  EXPECT_TRUE(transceiver.Sender());
  EXPECT_TRUE(transceiver.Receiver());
  EXPECT_EQ(transceiver.Direction(),
            webrtc::RtpTransceiverDirection::kSendRecv);
  EXPECT_FALSE(transceiver.CurrentDirection());
  EXPECT_FALSE(transceiver.FiredDirection());

  // Setting the state should make the transceiver state up-to-date.
  transceiver.set_state(std::move(modified_transceiver_state),
                        TransceiverStateUpdateMode::kAll);
  EXPECT_EQ(transceiver.Mid(), "MidyMacMidface");
  EXPECT_TRUE(transceiver.Sender());
  EXPECT_TRUE(transceiver.Receiver());
  EXPECT_EQ(transceiver.Direction(),
            webrtc::RtpTransceiverDirection::kInactive);
  EXPECT_TRUE(transceiver.CurrentDirection() ==
              webrtc::RtpTransceiverDirection::kSendRecv);
  EXPECT_FALSE(transceiver.FiredDirection());
}

TEST_F(RTCRtpTransceiverImplTest, ShallowCopy) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_sender =
      CreateWebRtcSender(local_track_adapter->webrtc_track(), "local_stream");
  auto webrtc_receiver = CreateWebRtcReceiver(
      remote_track_adapter->webrtc_track(), "remote_stream");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      webrtc_sender, webrtc_receiver, std::nullopt,
      webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt);

  std::unique_ptr<RTCRtpTransceiverImpl> transceiver;
  // Create transceiver.
  {
    RtpTransceiverState transceiver_state =
        CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                               remote_track_adapter->Copy());
    EXPECT_FALSE(transceiver_state.is_initialized());
    transceiver_state.Initialize();
    transceiver = std::make_unique<RTCRtpTransceiverImpl>(
        peer_connection_, track_map_, std::move(transceiver_state),
        /*encoded_insertable_streams=*/false, /*decode_metronome=*/nullptr);
  }
  DCHECK(transceiver);

  std::unique_ptr<RTCRtpTransceiverImpl> shallow_copy =
      transceiver->ShallowCopy();
  // Modifying the shallow copy should modify the original too since they have a
  // shared internal state.
  {
    // Modify webrtc transceiver to be inactive.
    webrtc_transceiver->ReplaceWith(*CreateWebRtcTransceiver(
        webrtc_sender, webrtc_receiver, std::nullopt,
        webrtc::RtpTransceiverDirection::kInactive, std::nullopt));
    RtpTransceiverState transceiver_state =
        CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                               remote_track_adapter->Copy());
    EXPECT_FALSE(transceiver_state.is_initialized());
    transceiver_state.Initialize();
    // Set the state of the shallow copy.
    shallow_copy->set_state(std::move(transceiver_state),
                            TransceiverStateUpdateMode::kAll);
  }
  EXPECT_EQ(shallow_copy->Direction(), transceiver->Direction());
}

TEST_F(RTCRtpTransceiverImplTest, TransceiverStateUpdateModeSetDescription) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_sender =
      CreateWebRtcSender(local_track_adapter->webrtc_track(), "local_stream");
  auto webrtc_receiver = CreateWebRtcReceiver(
      remote_track_adapter->webrtc_track(), "remote_stream");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      webrtc_sender, webrtc_receiver, std::nullopt,
      webrtc::RtpTransceiverDirection::kSendRecv, std::nullopt);

  // Create initial state.
  RtpTransceiverState initial_transceiver_state =
      CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                             remote_track_adapter->Copy());
  EXPECT_FALSE(initial_transceiver_state.is_initialized());
  initial_transceiver_state.Initialize();

  // Modify the webrtc transceiver and create a new state object for the
  // modified state.
  webrtc_sender->SetTrack(nullptr);
  webrtc_transceiver->ReplaceWith(
      *CreateWebRtcTransceiver(webrtc_sender, webrtc_receiver, "MidyMacMidface",
                               webrtc::RtpTransceiverDirection::kInactive,
                               webrtc::RtpTransceiverDirection::kSendRecv));
  RtpTransceiverState modified_transceiver_state =
      CreateTransceiverState(webrtc_transceiver, local_track_adapter->Copy(),
                             remote_track_adapter->Copy());
  EXPECT_FALSE(modified_transceiver_state.is_initialized());
  modified_transceiver_state.Initialize();

  // Construct a transceiver from the initial state.
  RTCRtpTransceiverImpl transceiver(peer_connection_, track_map_,
                                    std::move(initial_transceiver_state),
                                    /*encoded_insertable_streams=*/false,
                                    /*decode_metronome=*/nullptr);
  // Setting the state with TransceiverStateUpdateMode::kSetDescription should
  // make the transceiver state up-to-date, except leaving
  // "transceiver.direction" and "transceiver.sender.track" unmodified.
  transceiver.set_state(std::move(modified_transceiver_state),
                        TransceiverStateUpdateMode::kSetDescription);
  EXPECT_EQ(transceiver.Mid(), "MidyMacMidface");
  EXPECT_TRUE(transceiver.Sender());
  EXPECT_TRUE(transceiver.Receiver());
  EXPECT_TRUE(transceiver.CurrentDirection() ==
              webrtc::RtpTransceiverDirection::kSendRecv);
  EXPECT_FALSE(transceiver.FiredDirection());
  // The sender still has a track, even though the modified state doesn't.
  EXPECT_TRUE(transceiver.Sender()->Track());
  // The direction still "sendrecv", even though the modified state has
  // "inactive".
  EXPECT_EQ(transceiver.Direction(),
            webrtc::RtpTransceiverDirection::kSendRecv);
}

}  // namespace blink
