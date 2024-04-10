// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"

#include <memory>

#include "base/run_loop.h"
#include "base/types/pass_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/peerconnection/fake_rtc_rtp_transceiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_offer_options_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using ::testing::_;

namespace blink {

const char* kDefaultTransceiverString =
    "getTransceivers()[0]:{\n"
    "  mid:null,\n"
    "  kind:'audio',\n"
    "  sender:{\n"
    "    track:'senderTrackId',\n"
    "    streams:['senderStreamId'],\n"
    "  },\n"
    "  receiver:{\n"
    "    track:'receiverTrackId',\n"
    "    streams:['receiverStreamId'],\n"
    "  },\n"
    "  direction:'sendonly',\n"
    "  currentDirection:null,\n"
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
               void(int, bool, bool, const String&, const String&));
  MOCK_METHOD4(GetUserMediaSuccess,
               void(int, const String&, const String&, const String&));
  MOCK_METHOD3(GetUserMediaFailure, void(int, const String&, const String&));
  MOCK_METHOD5(GetDisplayMedia,
               void(int, bool, bool, const String&, const String&));
  MOCK_METHOD4(GetDisplayMediaSuccess,
               void(int, const String&, const String&, const String&));
  MOCK_METHOD3(GetDisplayMediaFailure, void(int, const String&, const String&));
  MOCK_METHOD2(WebRtcEventLogWrite, void(int, const Vector<uint8_t>&));
  MOCK_METHOD2(AddStandardStats, void(int, base::Value::List));
  MOCK_METHOD2(AddLegacyStats, void(int, base::Value::List));

  mojo::PendingRemote<blink::mojom::blink::PeerConnectionTrackerHost>
  CreatePendingRemoteAndBind() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  mojo::Receiver<blink::mojom::blink::PeerConnectionTrackerHost> receiver_{
      this};
};

// Creates a transceiver that is expected to be logged as
// |kDefaultTransceiverString|.
//
// This is used in unittests that don't care about the specific attributes of
// the transceiver.
std::unique_ptr<RTCRtpTransceiverPlatform> CreateDefaultTransceiver() {
  std::unique_ptr<RTCRtpTransceiverPlatform> transceiver;
  blink::FakeRTCRtpSenderImpl sender(
      "senderTrackId", {"senderStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  blink::FakeRTCRtpReceiverImpl receiver(
      "receiverTrackId", {"receiverStreamId"},
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  transceiver = std::make_unique<blink::FakeRTCRtpTransceiverImpl>(
      String(), std::move(sender), std::move(receiver),
      webrtc::RtpTransceiverDirection::kSendOnly /* direction */,
      std::nullopt /* current_direction */);
  return transceiver;
}

namespace {

// TODO(https://crbug.com/868868): Move this into a separate file.
class MockPeerConnectionHandler : public RTCPeerConnectionHandler {
 public:
  MockPeerConnectionHandler()
      : MockPeerConnectionHandler(
            MakeGarbageCollected<MockPeerConnectionDependencyFactory>(),
            MakeGarbageCollected<MockRTCPeerConnectionHandlerClient>()) {}
  MOCK_METHOD0(CloseClientPeerConnection, void());
  MOCK_METHOD1(OnThermalStateChange, void(mojom::blink::DeviceThermalState));
  MOCK_METHOD1(OnSpeedLimitChange, void(int));

 private:
  explicit MockPeerConnectionHandler(
      MockPeerConnectionDependencyFactory* factory,
      MockRTCPeerConnectionHandlerClient* client)
      : RTCPeerConnectionHandler(
            client,
            factory,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*encoded_insertable_streams=*/false),
        factory_(factory),
        client_(client) {}

  Persistent<MockPeerConnectionDependencyFactory> factory_;
  Persistent<MockRTCPeerConnectionHandlerClient> client_;
};

webrtc::PeerConnectionInterface::RTCConfiguration DefaultConfig() {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  return config;
}

}  // namespace

class PeerConnectionTrackerTest : public ::testing::Test {
 public:
  void CreateTrackerWithMocks() {
    mock_host_ = std::make_unique<MockPeerConnectionTrackerHost>();
    tracker_ = MakeGarbageCollected<PeerConnectionTracker>(
        mock_host_->CreatePendingRemoteAndBind(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        base::PassKey<PeerConnectionTrackerTest>());
  }

  void CreateAndRegisterPeerConnectionHandler() {
    mock_handler_ = std::make_unique<MockPeerConnectionHandler>();
    EXPECT_CALL(*mock_host_, AddPeerConnection(_));
    tracker_->RegisterPeerConnection(mock_handler_.get(), DefaultConfig(),
                                     nullptr);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPeerConnectionTrackerHost> mock_host_;
  Persistent<PeerConnectionTracker> tracker_;
  std::unique_ptr<MockPeerConnectionHandler> mock_handler_;
};

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

TEST_F(PeerConnectionTrackerTest, OnSpeedLimitChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, OnSpeedLimitChange(22));
  tracker_->OnSpeedLimitChange(22);
  EXPECT_CALL(*mock_handler_, OnSpeedLimitChange(33));
  tracker_->OnSpeedLimitChange(33);
}

TEST_F(PeerConnectionTrackerTest, ReportInitialThermalState) {
  MockPeerConnectionHandler handler0;
  MockPeerConnectionHandler handler1;
  MockPeerConnectionHandler handler2;
  CreateTrackerWithMocks();

  // Nothing is reported by default.
  EXPECT_CALL(handler0, OnThermalStateChange(_)).Times(0);
  EXPECT_CALL(*mock_host_, AddPeerConnection(_)).Times(1);
  tracker_->RegisterPeerConnection(&handler0, DefaultConfig(), nullptr);
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
  tracker_->RegisterPeerConnection(&handler1, DefaultConfig(), nullptr);
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
  tracker_->RegisterPeerConnection(&handler2, DefaultConfig(), nullptr);
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
      "  kind:'audio',\n"
      "  sender:{\n"
      "    track:'senderTrackId',\n"
      "    streams:['streamIdA','streamIdB'],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:['streamIdC'],\n"
      "  },\n"
      "  direction:'sendrecv',\n"
      "  currentDirection:'inactive',\n"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesNull) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      String(),
      blink::FakeRTCRtpSenderImpl(
          std::nullopt, {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      blink::FakeRTCRtpReceiverImpl(
          "receiverTrackId", {},
          blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
      webrtc::RtpTransceiverDirection::kInactive /* direction */,
      std::nullopt /* current_direction */);
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
      "  kind:'audio',\n"
      "  sender:{\n"
      "    track:null,\n"
      "    streams:[],\n"
      "  },\n"
      "  receiver:{\n"
      "    track:'receiverTrackId',\n"
      "    streams:[],\n"
      "  },\n"
      "  direction:'inactive',\n"
      "  currentDirection:null,\n"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, ModifyTransceiver) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
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

TEST_F(PeerConnectionTrackerTest, IceCandidateError) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
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
