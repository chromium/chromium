// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the RTCIceTransport Blink bindings, IceTransportProxy and
// IceTransportHost by mocking out the underlying IceTransportAdapter.
// Everything is run on a single thread but with separate TestSimpleTaskRunners
// for the main thread / worker thread.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

namespace blink {
namespace {

using testing::_;
using testing::Assign;
using testing::AllOf;
using testing::DoDefault;
using testing::ElementsAre;
using testing::Field;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::StrEq;
using testing::StrNe;

constexpr char kRemoteUsernameFragment1[] = "usernameFragment";
constexpr char kRemotePassword1[] = "password";
constexpr char kRemoteUsernameFragment2[] = "secondUsernameFragment";
constexpr char kRemotePassword2[] = "secondPassword";

RTCIceParameters* CreateRemoteRTCIceParameters2() {
  RTCIceParameters* ice_parameters = RTCIceParameters::Create();
  ice_parameters->setUsernameFragment(kRemoteUsernameFragment2);
  ice_parameters->setPassword(kRemotePassword2);
  return ice_parameters;
}

constexpr char kLocalIceCandidateStr1[] =
    "candidate:a0+B/1 1 udp 2130706432 192.168.1.5 1234 typ host generation 2";
constexpr char kRemoteIceCandidateStr1[] =
    "candidate:a0+B/2 1 udp 2130706432 ::1 1238 typ host generation 2";
constexpr char kRemoteIceCandidateStr2[] =
    "candidate:a0+B/3 1 udp 2130706432 74.125.127.126 2345 typ srflx raddr "
    "192.168.1.5 rport 2346 generation 2";

RTCIceCandidate* RTCIceCandidateFromString(V8TestingScope& scope,
                                           const String& candidate_str) {
  RTCIceCandidateInit* init = RTCIceCandidateInit::Create();
  init->setCandidate(candidate_str);
  init->setSdpMid(String(""));
  return RTCIceCandidate::Create(scope.GetExecutionContext(), init,
                                 ASSERT_NO_EXCEPTION);
}

cricket::Candidate CricketCandidateFromString(
    const std::string& candidate_str) {
  cricket::Candidate candidate;
  bool success =
      webrtc::SdpDeserializeCandidate("", candidate_str, &candidate, nullptr);
  DCHECK(success);
  return candidate;
}

}  // namespace

// static
RTCIceParameters* RTCIceTransportTest::CreateRemoteRTCIceParameters1() {
  RTCIceParameters* ice_parameters = RTCIceParameters::Create();
  ice_parameters->setUsernameFragment(kRemoteUsernameFragment1);
  ice_parameters->setPassword(kRemotePassword1);
  return ice_parameters;
}

RTCIceTransportTest::RTCIceTransportTest()
    : main_thread_(new base::TestSimpleTaskRunner()),
      worker_thread_(new base::TestSimpleTaskRunner()) {}

RTCIceTransportTest::~RTCIceTransportTest() {
  // When the V8TestingScope is destroyed at the end of a test, it will call
  // ContextDestroyed on the RTCIceTransport which will queue a task to delete
  // the IceTransportAdapter. RunUntilIdle() here ensures that the task will
  // be executed and the IceTransportAdapter deleted before finishing the
  // test.
  RunUntilIdle();

  // Explicitly verify expectations of garbage collected mock objects.
  for (auto mock : mock_event_listeners_) {
    Mock::VerifyAndClear(mock);
  }
}

void RTCIceTransportTest::RunUntilIdle() {
  while (worker_thread_->HasPendingTask() || main_thread_->HasPendingTask()) {
    worker_thread_->RunPendingTasks();
    main_thread_->RunPendingTasks();
  }
}

RTCIceTransport* RTCIceTransportTest::CreateIceTransport(
    V8TestingScope& scope) {
  return CreateIceTransport(
      scope, std::make_unique<MockIceTransportAdapter>(
                 std::make_unique<MockP2PQuicPacketTransport>()));
}

RTCIceTransport* RTCIceTransportTest::CreateIceTransport(
    V8TestingScope& scope,
    IceTransportAdapter::Delegate** delegate_out) {
  return CreateIceTransport(scope, std::make_unique<MockIceTransportAdapter>(),
                            delegate_out);
}

RTCIceTransport* RTCIceTransportTest::CreateIceTransport(
    V8TestingScope& scope,
    std::unique_ptr<MockIceTransportAdapter> mock,
    IceTransportAdapter::Delegate** delegate_out) {
  if (delegate_out) {
    // Ensure the caller has not left the delegate_out value floating.
    DCHECK_EQ(nullptr, *delegate_out);
  }
  return RTCIceTransport::Create(
      scope.GetExecutionContext(), main_thread_, worker_thread_,
      std::make_unique<MockIceTransportAdapterCrossThreadFactory>(
          std::move(mock), delegate_out));
}

MockEventListener* RTCIceTransportTest::CreateMockEventListener() {
  MockEventListener* event_listener = MakeGarbageCollected<MockEventListener>();
  mock_event_listeners_.push_back(event_listener);
  return event_listener;
}

// Test that calling gather({}) calls StartGathering with non-empty local
// parameters.
TEST_F(RTCIceTransportTest, GatherStartsGatheringWithNonEmptyLocalParameters) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  auto ice_parameters_not_empty =
      AllOf(Field(&cricket::IceParameters::ufrag, StrNe("")),
            Field(&cricket::IceParameters::pwd, StrNe("")));
  EXPECT_CALL(*mock, StartGathering(ice_parameters_not_empty, _, _, _))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
  options->setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling gather({ gatherPolicy: 'all' }) calls StartGathering with
// IceTransportPolicy::kAll.
TEST_F(RTCIceTransportTest, GatherIceTransportPolicyAll) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, StartGathering(_, _, _, IceTransportPolicy::kAll))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
  options->setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling gather({ gatherPolicy: 'relay' }) calls StartGathering with
// IceTransportPolicy::kRelay.
TEST_F(RTCIceTransportTest, GatherIceTransportPolicyRelay) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, StartGathering(_, _, _, IceTransportPolicy::kRelay))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
  options->setGatherPolicy("relay");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
}

// Test that calling stop() deletes the underlying IceTransportAdapter.
TEST_F(RTCIceTransportTest, StopDeletesIceTransportAdapter) {
  V8TestingScope scope;

  bool mock_deleted = false;
  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(*mock, Die()).WillOnce(Assign(&mock_deleted, true));

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
  options->setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);

  ice_transport->stop();
  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that the IceTransportAdapter is deleted on ContextDestroyed.
TEST_F(RTCIceTransportTest, ContextDestroyedDeletesIceTransportAdapter) {
  bool mock_deleted = false;
  {
    V8TestingScope scope;

    auto mock = std::make_unique<MockIceTransportAdapter>();
    EXPECT_CALL(*mock, Die()).WillOnce(Assign(&mock_deleted, true));

    Persistent<RTCIceTransport> ice_transport =
        CreateIceTransport(scope, std::move(mock));
    RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
    options->setGatherPolicy("all");
    ice_transport->gather(options, ASSERT_NO_EXCEPTION);
  }  // ContextDestroyed when V8TestingScope goes out of scope.

  RunUntilIdle();

  EXPECT_TRUE(mock_deleted);
}

// Test that calling OnGatheringStateChanged(complete) on the delegate fires a
// null icecandidate event and a gatheringstatechange event.
TEST_F(RTCIceTransportTest, OnGatheringStateChangedCompleteFiresEvents) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  RTCIceGatherOptions* options = RTCIceGatherOptions::Create();
  options->setGatherPolicy("all");
  ice_transport->gather(options, ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> ice_candidate_listener =
      CreateMockEventListener();
  Persistent<MockEventListener> gathering_state_change_listener =
      CreateMockEventListener();
  {
    InSequence dummy;
    EXPECT_CALL(*ice_candidate_listener, Invoke(_, _))
        .WillOnce(
            testing::Invoke([ice_transport](ExecutionContext*, Event* event) {
              auto* ice_event = static_cast<RTCPeerConnectionIceEvent*>(event);
              EXPECT_EQ(nullptr, ice_event->candidate());
            }));
    EXPECT_CALL(*gathering_state_change_listener, Invoke(_, _))
        .WillOnce(InvokeWithoutArgs([ice_transport] {
          EXPECT_EQ("complete", ice_transport->gatheringState());
        }));
  }
  ice_transport->addEventListener(event_type_names::kIcecandidate,
                                  ice_candidate_listener);
  ice_transport->addEventListener(event_type_names::kGatheringstatechange,
                                  gathering_state_change_listener);
  delegate->OnGatheringStateChanged(cricket::kIceGatheringComplete);

  RunUntilIdle();
}

// Test that calling start() calls Start on the IceTransportAdapter with the
// correct arguments when no remote candidates had previously been added.
TEST_F(RTCIceTransportTest,
       StartPassesRemoteParametersAndRoleAndInitialRemoteCandidates) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  auto ice_parameters_equal = AllOf(
      Field(&cricket::IceParameters::ufrag, StrEq(kRemoteUsernameFragment1)),
      Field(&cricket::IceParameters::pwd, StrEq(kRemotePassword1)));
  EXPECT_CALL(*mock, Start(ice_parameters_equal, cricket::ICEROLE_CONTROLLING,
                           ElementsAre()))
      .Times(1);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
}

MATCHER_P(SerializedIceCandidateEq, candidate_str, "") {
  std::string arg_str = webrtc::SdpSerializeCandidate(arg);
  *result_listener << "Expected ICE candidate that serializes to: "
                   << candidate_str << "; got: " << arg_str;
  return arg_str == candidate_str;
}

// Test that remote candidates are not passed to the IceTransportAdapter until
// start() is called.
TEST_F(RTCIceTransportTest, RemoteCandidatesNotPassedUntilStartCalled) {
  V8TestingScope scope;

  auto mock = std::make_unique<MockIceTransportAdapter>();
  EXPECT_CALL(
      *mock,
      Start(_, _,
            ElementsAre(SerializedIceCandidateEq(kRemoteIceCandidateStr1))))
      .Times(1);
  EXPECT_CALL(*mock, AddRemoteCandidate(_)).Times(0);

  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, std::move(mock));
  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
}

// Test that receiving an OnStateChanged callback with the connected state
// updates the RTCIceTransport state to 'connected' and fires a statechange
// event.
TEST_F(RTCIceTransportTest, OnStateChangedConnectedUpdatesStateAndFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs(
          [ice_transport] { EXPECT_EQ("connected", ice_transport->state()); }));
  ice_transport->addEventListener(event_type_names::kStatechange,
                                  event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));

  delegate->OnStateChanged(webrtc::IceTransportState::kConnected);

  RunUntilIdle();
}

// Test that receiving an OnStateChanged callback with the completed state
// updates the RTCIceTransport state to 'completed' and fires a statechange
// event.
TEST_F(RTCIceTransportTest, OnStateChangedCompletedUpdatesStateAndFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs(
          [ice_transport] { EXPECT_EQ("completed", ice_transport->state()); }));
  ice_transport->addEventListener(event_type_names::kStatechange,
                                  event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));

  delegate->OnStateChanged(webrtc::IceTransportState::kCompleted);

  RunUntilIdle();
}

// Test that receiving an OnStateChanged callback with the disconnected state
// updates the RTCIceTransport state to 'disconnected' and fires a statechange
// event.
TEST_F(RTCIceTransportTest,
       OnStateChangedDisconnectedUpdatesStateAndFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        EXPECT_EQ("disconnected", ice_transport->state());
      }));
  ice_transport->addEventListener(event_type_names::kStatechange,
                                  event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));

  delegate->OnStateChanged(webrtc::IceTransportState::kDisconnected);

  RunUntilIdle();
}
// Test that receiving an OnStateChanged callback with the failed state updates
// the RTCIceTransport state to 'failed' and fires a statechange event.
TEST_F(RTCIceTransportTest, OnStateChangedFailedUpdatesStateAndFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  // Due to the quick fix for crbug.com/957487 (should go to "disconnected"
  // state when end-of-candidates is signalled), this is accepting
  // that the end state is 'disconnected' rather than 'failed'.
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        EXPECT_EQ("disconnected", ice_transport->state());
      }));
  ice_transport->addEventListener(event_type_names::kStatechange,
                                  event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));

  delegate->OnStateChanged(webrtc::IceTransportState::kFailed);

  RunUntilIdle();
}

// Test that calling OnSelectedCandidatePairChanged the first time fires the
// selectedcandidatepairchange event and sets the selected candidate pair.
TEST_F(RTCIceTransportTest, InitialOnSelectedCandidatePairChangedFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        RTCIceCandidatePair* selected_candidate_pair =
            ice_transport->getSelectedCandidatePair();
        ASSERT_TRUE(selected_candidate_pair);
        EXPECT_EQ(ice_transport->getLocalCandidates()[0]->candidate(),
                  selected_candidate_pair->local()->candidate());
        EXPECT_EQ(ice_transport->getRemoteCandidates()[0]->candidate(),
                  selected_candidate_pair->remote()->candidate());
      }));
  ice_transport->addEventListener(
      event_type_names::kSelectedcandidatepairchange, event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));
  delegate->OnSelectedCandidatePairChanged(
      std::make_pair(CricketCandidateFromString(kLocalIceCandidateStr1),
                     CricketCandidateFromString(kRemoteIceCandidateStr1)));

  RunUntilIdle();
}

// Test that calling OnSelectedCandidatePairChanged with a different remote
// candidate fires the event and updates the selected candidate pair.
TEST_F(RTCIceTransportTest,
       OnSelectedCandidatePairChangedWithDifferentRemoteCandidateFiresEvent) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(DoDefault())  // First event is already tested above.
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        RTCIceCandidatePair* selected_candidate_pair =
            ice_transport->getSelectedCandidatePair();
        ASSERT_TRUE(selected_candidate_pair);
        EXPECT_EQ(ice_transport->getLocalCandidates()[0]->candidate(),
                  selected_candidate_pair->local()->candidate());
        EXPECT_EQ(ice_transport->getRemoteCandidates()[1]->candidate(),
                  selected_candidate_pair->remote()->candidate());
      }));
  ice_transport->addEventListener(
      event_type_names::kSelectedcandidatepairchange, event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));

  delegate->OnSelectedCandidatePairChanged(
      std::make_pair(CricketCandidateFromString(kLocalIceCandidateStr1),
                     CricketCandidateFromString(kRemoteIceCandidateStr1)));

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr2),
      ASSERT_NO_EXCEPTION);
  delegate->OnSelectedCandidatePairChanged(
      std::make_pair(CricketCandidateFromString(kLocalIceCandidateStr1),
                     CricketCandidateFromString(kRemoteIceCandidateStr2)));

  RunUntilIdle();
}

// Test that receiving an OnStateChange callback to the failed state once a
// connection has been established clears the selected candidate pair without
// firing the selectedcandidatepairchange event.
// Disabled while sorting out the use of "failed" vs "disconnected"
// (crbug.com/957847).
TEST_F(RTCIceTransportTest,
       DISABLED_OnStateChangeFailedAfterConnectedClearsSelectedCandidatePair) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> state_change_event_listener =
      CreateMockEventListener();
  EXPECT_CALL(*state_change_event_listener, Invoke(_, _))
      .WillOnce(DoDefault())  // First event is for 'connected'.
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        EXPECT_EQ("failed", ice_transport->state());
        RTCIceCandidatePair* selected_candidate_pair =
            ice_transport->getSelectedCandidatePair();
        EXPECT_EQ(nullptr, selected_candidate_pair);
      }));
  ice_transport->addEventListener(event_type_names::kStatechange,
                                  state_change_event_listener);

  Persistent<MockEventListener> selected_candidate_pair_change_event_listener =
      CreateMockEventListener();
  EXPECT_CALL(*selected_candidate_pair_change_event_listener, Invoke(_, _))
      .Times(1);  // First event is for the connected pair.
  ice_transport->addEventListener(
      event_type_names::kSelectedcandidatepairchange,
      selected_candidate_pair_change_event_listener);

  // Establish the connection
  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));
  delegate->OnStateChanged(webrtc::IceTransportState::kConnected);
  delegate->OnSelectedCandidatePairChanged(
      std::make_pair(CricketCandidateFromString(kLocalIceCandidateStr1),
                     CricketCandidateFromString(kRemoteIceCandidateStr1)));

  // Transition to failed.
  delegate->OnStateChanged(webrtc::IceTransportState::kFailed);

  RunUntilIdle();
}

// Test that receiving an OnSelectedCandidatePairChange callback after a remote
// ICE restart still updates the selected candidate pair.
TEST_F(RTCIceTransportTest,
       RemoteIceRestartRaceWithSelectedCandidatePairChange) {
  V8TestingScope scope;

  IceTransportAdapter::Delegate* delegate = nullptr;
  Persistent<RTCIceTransport> ice_transport =
      CreateIceTransport(scope, &delegate);
  ice_transport->start(CreateRemoteRTCIceParameters1(), "controlling",
                       ASSERT_NO_EXCEPTION);
  RunUntilIdle();
  ASSERT_TRUE(delegate);

  Persistent<MockEventListener> event_listener = CreateMockEventListener();
  EXPECT_CALL(*event_listener, Invoke(_, _))
      .WillOnce(InvokeWithoutArgs([ice_transport] {
        RTCIceCandidatePair* selected_candidate_pair =
            ice_transport->getSelectedCandidatePair();
        ASSERT_TRUE(selected_candidate_pair);
        EXPECT_EQ(kLocalIceCandidateStr1,
                  selected_candidate_pair->local()->candidate());
        EXPECT_EQ(kRemoteIceCandidateStr1,
                  selected_candidate_pair->remote()->candidate());
      }));
  ice_transport->addEventListener(
      event_type_names::kSelectedcandidatepairchange, event_listener);

  ice_transport->addRemoteCandidate(
      RTCIceCandidateFromString(scope, kRemoteIceCandidateStr1),
      ASSERT_NO_EXCEPTION);

  // Changing remote ICE parameters indicate a remote ICE restart. This clears
  // the stored list of remote candidates.
  ice_transport->start(CreateRemoteRTCIceParameters2(), "controlling",
                       ASSERT_NO_EXCEPTION);

  // These callbacks are part of the previous generation but should still take
  // effect.
  delegate->OnCandidateGathered(
      CricketCandidateFromString(kLocalIceCandidateStr1));
  delegate->OnStateChanged(webrtc::IceTransportState::kConnected);
  delegate->OnSelectedCandidatePairChanged(
      std::make_pair(CricketCandidateFromString(kLocalIceCandidateStr1),
                     CricketCandidateFromString(kRemoteIceCandidateStr1)));

  RunUntilIdle();
}

}  // namespace blink
