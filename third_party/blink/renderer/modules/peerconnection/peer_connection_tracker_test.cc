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
using ::testing::ElementsAre;

using PeerConnectionInfoPtr = ::blink::mojom::blink::PeerConnectionInfoPtr;

namespace blink {

const char* kDefaultTransceiverString =
    "{\"mid\":null,"
    "\"kind\":\"audio\","
    "\"sender\":{"
    "\"track\":\"senderTrackId\","
    "\"streams\":[\"senderStreamId\"],"
    "\"encodings\":[]"
    "},"
    "\"receiver\":{"
    "\"track\":\"receiverTrackId\","
    "\"streams\":[\"receiverStreamId\"]"
    "},"
    "\"direction\":\"sendonly\","
    "\"currentDirection\":null,"
    "\"reason\":\"setLocalDescription\","
    "\"transceiverIndex\":0"
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
  MOCK_METHOD2(WebRtcDataChannelLogWrite, void(int, const Vector<uint8_t>&));
  MOCK_METHOD2(AddStandardStats, void(int, base::Value::List));

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
  MOCK_METHOD0(StartDataChannelLog, void());
  MOCK_METHOD0(StopDataChannelLog, void());
  MOCK_METHOD1(StartEventLog, void(int));
  MOCK_METHOD0(StopEventLog, void());

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

  PeerConnectionInfoPtr CreateAndRegisterPeerConnectionHandler() {
    mock_handler_ = std::make_unique<MockPeerConnectionHandler>();
    PeerConnectionInfoPtr res;
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_host_, AddPeerConnection)
        .WillOnce([&res, &run_loop](PeerConnectionInfoPtr info) {
          res = std::move(info);
          run_loop.Quit();
        });
    tracker_->RegisterPeerConnection(mock_handler_.get(), DefaultConfig(),
                                     nullptr);
    run_loop.Run();
    return res;
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
      MakeGarbageCollected<RTCOfferOptionsPlatform>(1, 1, true, true);
  EXPECT_CALL(
      *mock_host_,
      UpdatePeerConnection(
          _, String("createOffer"),
          String("{\"offerToReceiveAudio\":true,\"offerToReceiveVideo\":true,"
                 "\"voiceActivityDetection\":true,\"iceRestart\":true}")));
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

TEST_F(PeerConnectionTrackerTest, StartDataChannelLogCalled) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StartDataChannelLog);
  tracker_->StartDataChannelLog(info->lid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest,
       StartDataChannelLogNotCalledIfMismatchBetweenLidAndPeerConnection) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StartDataChannelLog).Times(0);
  tracker_->StartDataChannelLog(info->lid + 1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, StopDataChannelLogCalled) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StopDataChannelLog);
  tracker_->StopDataChannelLog(info->lid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest,
       StopDataChannelLogNotCalledIfMismatchBetweenLidAndPeerConnection) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StopDataChannelLog).Times(0);
  tracker_->StopDataChannelLog(info->lid + 1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, DataChannelLoggingWrite) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_host_,
              WebRtcDataChannelLogWrite(info->lid, ElementsAre(1, 2, 3)));
  tracker_->TrackRtcDataChannelLogWrite(mock_handler_.get(), {1, 2, 3});
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, StartEventLogCalled) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StartEventLog(123));
  tracker_->StartEventLog(info->lid, 123);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest,
       StartEventLogNotCalledIfMismatchBetweenLidAndPeerConnection) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StartEventLog).Times(0);
  tracker_->StartEventLog(info->lid + 1, 321);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, StopEventLogCalled) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StopEventLog);
  tracker_->StopEventLog(info->lid);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest,
       StopEventLogNotCalledIfMismatchBetweenLidAndPeerConnection) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_handler_, StopEventLog).Times(0);
  tracker_->StopEventLog(info->lid + 1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PeerConnectionTrackerTest, EventLoggingWrite) {
  CreateTrackerWithMocks();
  PeerConnectionInfoPtr info = CreateAndRegisterPeerConnectionHandler();

  EXPECT_CALL(*mock_host_,
              WebRtcEventLogWrite(info->lid, ElementsAre(1, 2, 3)));
  tracker_->TrackRtcEventLogWrite(mock_handler_.get(), {1, 2, 3});
  base::RunLoop().RunUntilIdle();
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
      "{\"mid\":\"midValue\","
      "\"kind\":\"audio\","
      "\"sender\":{"
      "\"track\":\"senderTrackId\","
      "\"streams\":[\"streamIdA\",\"streamIdB\"],"
      "\"encodings\":[]"
      "},"
      "\"receiver\":{"
      "\"track\":\"receiverTrackId\","
      "\"streams\":[\"streamIdC\"]"
      "},"
      "\"direction\":\"sendrecv\","
      "\"currentDirection\":\"inactive\","
      "\"reason\":\"addTrack\","
      "\"transceiverIndex\":0"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, AddTransceiverWithOptionalValuesNull) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  blink::FakeRTCRtpTransceiverImpl transceiver(
      String(),
      blink::FakeRTCRtpSenderImpl(
          String(), {},
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
      "{\"mid\":null,"
      "\"kind\":\"audio\","
      "\"sender\":{"
      "\"track\":null,"
      "\"streams\":[],"
      "\"encodings\":[]"
      "},"
      "\"receiver\":{"
      "\"track\":\"receiverTrackId\","
      "\"streams\":[]"
      "},"
      "\"direction\":\"inactive\","
      "\"currentDirection\":null,"
      "\"reason\":\"addTransceiver\","
      "\"transceiverIndex\":1"
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
  String expected_value(kDefaultTransceiverString);
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, OnSignalingStateChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("onsignalingstatechange"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackSignalingStateChange(
      mock_handler_.get(),
      webrtc::PeerConnectionInterface::SignalingState::kStable);
  base::RunLoop().RunUntilIdle();
  String expected_value("\"stable\"");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, OnIceGatheringStateChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("onicegatheringstatechange"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackIceGatheringStateChange(
      mock_handler_.get(), webrtc::PeerConnectionInterface::IceGatheringState::
                               kIceGatheringComplete);
  base::RunLoop().RunUntilIdle();
  String expected_value("\"complete\"");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, OnIceConnectionStateChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("oniceconnectionstatechange"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackIceConnectionStateChange(
      mock_handler_.get(), webrtc::PeerConnectionInterface::IceConnectionState::
                               kIceConnectionDisconnected);
  base::RunLoop().RunUntilIdle();
  String expected_value("\"disconnected\"");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, OnConnectionStateChange) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("onconnectionstatechange"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackConnectionStateChange(
      mock_handler_.get(),
      webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected);
  base::RunLoop().RunUntilIdle();
  String expected_value("\"disconnected\"");
  EXPECT_EQ(expected_value, update_value);
}

TEST_F(PeerConnectionTrackerTest, OnIceCandidateError) {
  CreateTrackerWithMocks();
  CreateAndRegisterPeerConnectionHandler();
  auto transceiver = CreateDefaultTransceiver();
  String update_value;
  EXPECT_CALL(*mock_host_,
              UpdatePeerConnection(_, String("onicecandidateerror"), _))
      .WillOnce(testing::SaveArg<2>(&update_value));
  tracker_->TrackIceCandidateError(mock_handler_.get(), "1.1.1.1", 15, "[::1]",
                                   "test url", 404, "test error");
  base::RunLoop().RunUntilIdle();
  String expected_value(
      "{"
      "\"url\":\"test url\","
      "\"address\":\"1.1.1.1\","
      "\"port\":15,"
      "\"host_candidate\":\"[::1]\","
      "\"error_text\":\"test error\","
      "\"error_code\":404"
      "}");
  EXPECT_EQ(expected_value, update_value);
}

// TODO(hta): Write tests for the other tracking functions.

}  // namespace blink
