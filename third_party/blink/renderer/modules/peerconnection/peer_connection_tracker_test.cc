// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"

#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/mediastream/media_constraints.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"

using ::testing::_;

namespace blink {

const char* kDefaultTransceiverString =
    "getTransceivers()[0]:{\n"
    "  mid:null,\n"
    "  sender:{\n"
    "    track:'senderTrackId',\n"
    "    streams:['senderStreamId'],\n"
    "  },\n"
    "  receiver:{\n"
    "    track:'receiverTrackId',\n"
    "    streams:['receiverStreamId'],\n"
    "  },\n"
    "  stopped:false,\n"
    "  direction:'sendonly',\n"
    "  currentDirection:null,\n"
    "}";

const char* kDefaultSenderString =
    "getSenders()[0]:{\n"
    "  track:'senderTrackId',\n"
    "  streams:['senderStreamId'],\n"
    "}";

const char* kDefaultReceiverString =
    "getReceivers()[0]:{\n"
    "  track:'receiverTrackId',\n"
    "  streams:['receiverStreamId'],\n"
    "}";

class MockPeerConnectionTrackerHost
    : public blink::mojom::blink::PeerConnectionTrackerHost {
 public:
  MockPeerConnectionTrackerHost() {}
  MOCK_METHOD3(UpdatePeerConnection, void(int, const String&, const String&));
  MOCK_METHOD1(AddPeerConnection,
               void(blink::mojom::blink::PeerConnectionInfoPtr));
  MOCK_METHOD1(RemovePeerConnection, void(int));
  MOCK_METHOD2(OnPeerConnectionSessionIdSet, void(int, const String&));
  MOCK_METHOD5(GetUserMedia,
               void(const String&, bool, bool, const String&, const String&));
  MOCK_METHOD2(WebRtcEventLogWrite, void(int, const Vector<uint8_t>&));
  MOCK_METHOD2(AddStandardStats, void(int, base::Value));
  MOCK_METHOD2(AddLegacyStats, void(int, base::Value));

  mojo::Remote<blink::mojom::blink::PeerConnectionTrackerHost>
  CreatePendingRemoteAndBind() {
    receiver_.reset();
    return mojo::Remote<blink::mojom::blink::PeerConnectionTrackerHost>(
        receiver_.BindNewPipeAndPassRemote(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  }

  mojo::Receiver<blink::mojom::blink::PeerConnectionTrackerHost> receiver_{
      this};
};

// Creates a transceiver that is expected to be logged as
// |kDefaultTransceiverString|, |kDefaultSenderString| or
// |kDefaultReceiverString| depending on if |implementation_type| refers to a
// fully implemented, sender-only or receiver-only transceiver.
//
// This is used in unittests that don't care about the specific attributes of
// the transceiver.
std::unique_ptr<RTCRtpTransceiverPlatform> CreateDefaultTransceiver(
    RTCRtpTransceiverPlatformImplementationType implementation_type) {
  std::unique_ptr<RTCRtpTransceiverPlatform> transceiver;
  blink::FakeRTCRtpSenderImpl sender(
      "senderTrackId", {"senderStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  blink::FakeRTCRtpReceiverImpl receiver(
      "receiverTrackId", {"receiverStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  if (implementation_type ==
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver) {
    transceiver = std::make_unique<blink::FakeRTCRtpTransceiverImpl>(
        base::nullopt, std::move(sender), std::move(receiver),
        false /* stopped */,
        webrtc::RtpTransceiverDirection::kSendOnly /* direction */,
        base::nullopt /* current_direction */);
  } else if (implementation_type ==
             RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly) {
    transceiver = std::make_unique<blink::RTCRtpSenderOnlyTransceiver>(
        std::make_unique<blink::FakeRTCRtpSenderImpl>(sender));
  } else {
    DCHECK_EQ(implementation_type,
              RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
    transceiver = std::make_unique<blink::RTCRtpReceiverOnlyTransceiver>(
        std::make_unique<blink::FakeRTCRtpReceiverImpl>(receiver));
  }
  return transceiver;
}

namespace {

// TODO(https://crbug.com/868868): Move this into a separate file.
class MockPeerConnectionHandler : public RTCPeerConnectionHandler {
 public:
  MockPeerConnectionHandler()
      : RTCPeerConnectionHandler(
            &client_,
            &dependency_factory_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*force_encoded_audio_insertable_streams=*/false,
            /*force_encoded_video_insertable_streams=*/false) {}
  MOCK_METHOD0(CloseClientPeerConnection, void());
  MOCK_METHOD1(OnThermalStateChange, void(mojom::blink::DeviceThermalState));

 private:
  blink::MockPeerConnectionDependencyFactory dependency_factory_;
  MockRTCPeerConnectionHandlerClient client_;
};

}  // namespace

class PeerConnectionTrackerTest : public ::testing::Test {
 public:
  void CreateTrackerWithMocks() {
    mock_host_.reset(new MockPeerConnectionTrackerHost());
    tracker_.reset(new PeerConnectionTracker(
        mock_host_->CreatePendingRemoteAndBind(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting()));
  }

  void CreateAndRegisterPeerConnectionHandler() {
    mock_handler_.reset(new MockPeerConnectionHandler());
    EXPECT_CALL(*mock_host_, AddPeerConnection(_));
    tracker_->RegisterPeerConnection(
        mock_handler_.get(),
        webrtc::PeerConnectionInterface::RTCConfiguration(), MediaConstraints(),
        nullptr);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<MockPeerConnectionTrackerHost> mock_host_;
  std::unique_ptr<PeerConnectionTracker> tracker_;
  std::unique_ptr<MockPeerConnectionHandler> mock_handler_;
};

TEST_F(PeerConnectionTrackerTest, CreatingObject) {
  PeerConnectionTracker tracker(
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

TEST_F(PeerConnectionTrackerTest, TrackCreateOffer) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  // Note: blink::RTCOfferOptionsPlatform is not mockable. So we can't write
  // tests for anything but a null options parameter.
  RTCOfferOptionsPlatform* options =
      MakeGarbageCollected<RTCOfferOptionsPlatform>(0, 0, false, false);
  EXPECT_CALL(
      *mock_host_,
      UpdatePeerConnection(
          _, String("createOffer"),
          String("options: {offerToReceiveVideo: 0, offerToReceiveAudio: 0, "
                 "voiceActivityDetection: false, iceRestart: false}")));
  tracker_->TrackCreateOffer(mock_handler_.get(), options);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, OnSuspend) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  EXPECT_CALL(*mock_handler_, CloseClientPeerConnection());
  tracker_->OnSuspend();
}

TEST_F(PeerConnectionTrackerTest, OnThermalStateChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kUnknown))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kUnknown);

  EXPECT_CALL(*mock_handler_,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kNominal))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kNominal);

  EXPECT_CALL(*mock_handler_,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kFair))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kFair);

  EXPECT_CALL(*mock_handler_,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kSerious))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kSerious);

  EXPECT_CALL(*mock_handler_,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kCritical))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kCritical);
}

TEST_F(PeerConnectionTrackerTest, ReportInitialThermalState) {
  MockPeerConnectionHandler handler0;
  MockPeerConnectionHandler handler1;
  MockPeerConnectionHandler handler2;
  CreateTrackerWithMocks();

  // Nothing is reported by default.
  EXPECT_CALL(handler0, OnThermalStateChange(_)).Times(0);
  EXPECT_CALL(*mock_host_, AddPeerConnection(_)).Times(1);
  tracker_->RegisterPeerConnection(
      &handler0, webrtc::PeerConnectionInterface::RTCConfiguration(),
      MediaConstraints(), nullptr);
  base::RunLoop().RunUntilIdle();

  // Report a known thermal state.
  EXPECT_CALL(handler0,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kNominal))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kNominal);

  // Handlers registered late will get the event upon registering.
  EXPECT_CALL(handler1,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kNominal))
      .Times(1);
  EXPECT_CALL(*mock_host_, AddPeerConnection(_)).Times(1);
  tracker_->RegisterPeerConnection(
      &handler1, webrtc::PeerConnectionInterface::RTCConfiguration(),
      MediaConstraints(), nullptr);
  base::RunLoop().RunUntilIdle();

  // Report the unknown thermal state.
  EXPECT_CALL(handler0,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kUnknown))
      .Times(1);
  EXPECT_CALL(handler1,
              OnThermalStateChange(mojom::blink::DeviceThermalState::kUnknown))
      .Times(1);
  tracker_->OnThermalStateChange(mojom::blink::DeviceThermalState::kUnknown);

  // Handlers registered late get no event.
  EXPECT_CALL(handler2, OnThermalStateChange(_)).Times(0);
  EXPECT_CALL(*mock_host_, AddPeerConnection(_)).Times(1);
  tracker_->RegisterPeerConnection(
      &handler2, webrtc::PeerConnectionInterface::RTCConfiguration(),
      MediaConstraints(), nullptr);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesPresent) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      "midValue",
      blink::FakeRTCRtpSenderImpl(
          "senderTrackId", {"streamIdA", "streamIdB"},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      blink::FakeRTCRtpReceiverImpl(
          "receiverTrackId", {"streamIdC"},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      true /* stopped */,
      webrtc::RtpTransceiverDirection::kSendRecv /* direction */,
      webrtc::RtpTransceiverDirection::kInactive /* current_direction */);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("transceiverAdded"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kAddTrack, transceiver,
      0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: addTrack\n"
      "\n"
      "getTransceivers()[0]:{\n"
      "  mid:'midValue',\n"
      "  sender:{\n"
      "    track:'senderTrackId',\n"
      "    streams:['streamIdA','streamIdB'],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:['streamIdC'],\n"
      "  },\n"
      "  stopped:true,\n"
      "  direction:'sendrecv',\n"
      "  currentDirection:'inactive',\n"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesNull) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      base::nullopt,
      blink::FakeRTCRtpSenderImpl(
          base::nullopt, {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      blink::FakeRTCRtpReceiverImpl(
          "receiverTrackId", {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      false /* stopped */,
      webrtc::RtpTransceiverDirection::kInactive /* direction */,
      base::nullopt /* current_direction */);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("transceiverAdded"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kAddTransceiver,
      transceiver, 1u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: addTransceiver\n"
      "\n"
      "getTransceivers()[1]:{\n"
      "  mid:null,\n"
      "  sender:{\n"
      "    track:null,\n"
      "    streams:[],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:[],\n"
      "  },\n"
      "  stopped:false,\n"
      "  direction:'inactive',\n"
      "  currentDirection:null,\n"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifyTransceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("transceiverModified"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription,
      *transceiver, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value("Caused by: setLocalDescription\n\n" +
                        String(kDefaultTransceiverString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveTransceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kFullTransceiver);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("transceiverRemoved"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kRemoveTrack,
      *transceiver, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: removeTrack\n"
      "\n" +
      String(kDefaultTransceiverString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, AddSender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly);
  String update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, String("senderAdded"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetLocalDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setLocalDescription\n"
      "\n" +
      String(kDefaultSenderString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifySender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly);
  String update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, String("senderModified"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setRemoteDescription\n"
      "\n" +
      String(kDefaultSenderString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveSender) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto sender_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBSenderOnly);
  String update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, String("senderRemoved"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *sender_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setRemoteDescription\n"
      "\n" +
      String(kDefaultSenderString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, AddReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
  String update_value;
  EXPECT_CALL(*mock_host_, UpdatePeerConnection(_, String("receiverAdded"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackAddTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setRemoteDescription\n"
      "\n" +
      String(kDefaultReceiverString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifyReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("receiverModified"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackModifyTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setRemoteDescription\n"
      "\n" +
      String(kDefaultReceiverString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, RemoveReceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("receiverRemoved"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackRemoveTransceiver(
      mock_handler_.get(),
      PeerConnectionTracker::TransceiverUpdatedReason::kSetRemoteDescription,
      *receiver_only, 0u);
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "Caused by: setRemoteDescription\n"
      "\n" +
      String(kDefaultReceiverString));
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, IceCandidateError) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto receiver_only = CreateDefaultTransceiver(
      RTCRtpTransceiverPlatformImplementationType::kPlanBReceiverOnly);
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("icecandidateerror"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackIceCandidateError(mock_handler_.get(), "1.1.1.1", 15, "[::1]",
                                   "test url", 404, "test error");
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "url: test url\n"
      "address: 1.1.1.1\n"
      "port: 15\n"
      "host_candidate: [::1]\n"
      "error_text: test error\n"
      "error_code: 404");
  EXPECT_EQ(expected_value, update_value);
}

// TODO(hta): Write tests for the other tracking functions.

}  // namespace blink
