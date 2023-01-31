// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/bridge_ice_controller.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/webrtc/thread_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/peerconnection/webrtc_connection_matchers.h"

#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"
#include "third_party/webrtc/p2p/base/mock_ice_agent.h"
#include "third_party/webrtc/p2p/base/mock_ice_controller.h"

#include "third_party/webrtc_overrides/p2p/base/fake_connection_factory.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"
#include "third_party/webrtc_overrides/p2p/base/ice_interaction_interface.h"
#include "third_party/webrtc_overrides/p2p/base/ice_ping_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_switch_proposal.h"

namespace {

namespace cricket {
// This is an opaque type for the purposes of this test, so a forward
// declaration suffices
class IceConfig;
}  // namespace cricket

using ::blink::BridgeIceController;
using ::blink::FakeConnectionFactory;
using ::blink::IceConnection;
using ::blink::IceControllerObserverInterface;
using ::blink::IceInteractionInterface;
using ::blink::IcePingProposal;
using ::blink::IcePruneProposal;
using ::blink::IceSwitchProposal;

using ::cricket::Candidate;
using ::cricket::Connection;
using ::cricket::IceConfig;
using ::cricket::IceControllerFactoryArgs;
using ::cricket::IceControllerInterface;
using ::cricket::IceMode;
using ::cricket::IceRecheckEvent;
using ::cricket::IceSwitchReason;
using ::cricket::MockIceAgent;
using ::cricket::MockIceController;
using ::cricket::MockIceControllerFactory;
using ::cricket::NominationMode;

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::Test;
using ::testing::WithArgs;

using ::blink::ConnectionEq;
using ::blink::PingProposalEq;
using ::blink::PruneProposalEq;
using ::blink::SwitchProposalEq;

using ::base::test::SingleThreadTaskEnvironment;
using ::base::test::TaskEnvironment;

using NiceMockIceController = NiceMock<MockIceController>;

static const std::string kIp = "1.2.3.4";
static const std::string kIpTwo = "1.3.5.7";
static const std::string kIpThree = "1.4.7.10";
static const int kPort = 6745;

static const IceConfig* kIceConfig = reinterpret_cast<const IceConfig*>(0xfefe);

static const std::vector<const Connection*> kEmptyConnsList{};
static const IceControllerInterface::SwitchResult kEmptySwitchResult{};

static constexpr base::TimeDelta kTick = base::Milliseconds(1);

class MockIceControllerObserver : public IceControllerObserverInterface {
 public:
  MockIceControllerObserver() = default;
  ~MockIceControllerObserver() override = default;

  MOCK_METHOD(void,
              OnObserverAttached,
              (scoped_refptr<IceInteractionInterface> agent),
              (override));
  MOCK_METHOD(void, OnObserverDetached, (), (override));
  MOCK_METHOD(void,
              OnConnectionAdded,
              (const IceConnection& connection),
              (override));
  MOCK_METHOD(void,
              OnConnectionUpdated,
              (const IceConnection& connection),
              (override));
  MOCK_METHOD(void,
              OnConnectionSwitched,
              (const IceConnection& connection),
              (override));
  MOCK_METHOD(void,
              OnConnectionDestroyed,
              (const IceConnection& connection),
              (override));
  MOCK_METHOD(void,
              OnPingProposal,
              (const IcePingProposal& ping_proposal),
              (override));
  MOCK_METHOD(void,
              OnSwitchProposal,
              (const IceSwitchProposal& switch_proposal),
              (override));
  MOCK_METHOD(void,
              OnPruneProposal,
              (const IcePruneProposal& prune_proposal),
              (override));
};

class BridgeIceControllerTest : public Test {
 protected:
  BridgeIceControllerTest() {
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
    EXPECT_NE(webrtc::ThreadWrapper::current(), nullptr);

    base::WaitableEvent ready(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    connection_factory_ = std::make_unique<FakeConnectionFactory>(
        webrtc::ThreadWrapper::current(), &ready);
    connection_factory_->Prepare();
    ready.Wait();
  }

  const Connection* GetConnection(base::StringPiece remote_ip,
                                  int remote_port) {
    return connection_factory_->CreateConnection(
        FakeConnectionFactory::CandidateType::LOCAL, remote_ip, remote_port);
  }

  SingleThreadTaskEnvironment env{TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<FakeConnectionFactory> connection_factory_;
};

TEST_F(BridgeIceControllerTest, ObserverAttached) {
  MockIceAgent agent;
  MockIceControllerObserver observer1;
  MockIceControllerObserver observer2;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer1, OnObserverAttached).WillOnce(WithArgs<0>([&](auto ia) {
    interaction_agent = std::move(ia);
  }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer1,
                                 &agent, std::move(will_move));
  EXPECT_NE(interaction_agent, nullptr);

  EXPECT_CALL(observer1, OnObserverDetached);
  EXPECT_CALL(observer2, OnObserverAttached(_));
  controller.AttachObserver(&observer2);

  EXPECT_CALL(observer2, OnObserverDetached);
  controller.AttachObserver(nullptr);
}

TEST_F(BridgeIceControllerTest, PassthroughIceControllerInterface) {
  MockIceAgent agent;
  MockIceControllerObserver observer1;
  MockIceControllerObserver observer2;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  EXPECT_CALL(observer1, OnObserverAttached(_));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer1,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* connTwo = GetConnection(kIpTwo, kPort);
  ASSERT_NE(connTwo, nullptr);
  const Connection* connThree = GetConnection(kIpThree, kPort);
  ASSERT_NE(connThree, nullptr);

  EXPECT_CALL(*wrapped, SetIceConfig(Ref(*kIceConfig)));
  controller.SetIceConfig(*kIceConfig);

  EXPECT_CALL(*wrapped, GetUseCandidateAttr(conn, NominationMode::AGGRESSIVE,
                                            IceMode::ICEMODE_LITE))
      .WillOnce(Return(true));
  EXPECT_TRUE(controller.GetUseCandidateAttribute(
      conn, NominationMode::AGGRESSIVE, IceMode::ICEMODE_LITE));

  EXPECT_CALL(*wrapped, AddConnection(conn));
  EXPECT_CALL(observer1, OnConnectionAdded(ConnectionEq(conn)));
  controller.OnConnectionAdded(conn);

  EXPECT_CALL(*wrapped, SetSelectedConnection(conn));
  EXPECT_CALL(observer1, OnConnectionSwitched(ConnectionEq(conn)));
  controller.OnConnectionSwitched(conn);

  EXPECT_CALL(*wrapped, MarkConnectionPinged(conn));
  controller.OnConnectionPinged(conn);

  EXPECT_CALL(*wrapped, FindNextPingableConnection()).WillOnce(Return(conn));
  EXPECT_EQ(controller.FindNextPingableConnection(), conn);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(conn));
  EXPECT_CALL(observer1, OnConnectionDestroyed(ConnectionEq(conn)));
  controller.OnConnectionDestroyed(conn);

  EXPECT_CALL(observer1, OnObserverDetached);
  EXPECT_CALL(observer2, OnObserverAttached(_));
  controller.AttachObserver(&observer2);

  EXPECT_CALL(*wrapped, AddConnection(connTwo));
  EXPECT_CALL(observer1, OnConnectionAdded).Times(0);
  EXPECT_CALL(observer2, OnConnectionAdded(ConnectionEq(connTwo)));
  controller.OnConnectionAdded(connTwo);

  EXPECT_CALL(*wrapped, SetSelectedConnection(connTwo));
  EXPECT_CALL(observer1, OnConnectionSwitched).Times(0);
  EXPECT_CALL(observer2, OnConnectionSwitched(ConnectionEq(connTwo)));
  controller.OnConnectionSwitched(connTwo);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(connTwo));
  EXPECT_CALL(observer1, OnConnectionDestroyed).Times(0);
  EXPECT_CALL(observer2, OnConnectionDestroyed(ConnectionEq(connTwo)));
  controller.OnConnectionDestroyed(connTwo);

  EXPECT_CALL(observer2, OnObserverDetached);
  controller.AttachObserver(nullptr);

  EXPECT_CALL(*wrapped, AddConnection(connThree));
  EXPECT_CALL(observer1, OnConnectionAdded).Times(0);
  EXPECT_CALL(observer2, OnConnectionAdded).Times(0);
  controller.OnConnectionAdded(connThree);

  EXPECT_CALL(*wrapped, SetSelectedConnection(connThree));
  EXPECT_CALL(observer1, OnConnectionSwitched).Times(0);
  EXPECT_CALL(observer2, OnConnectionSwitched).Times(0);
  controller.OnConnectionSwitched(connThree);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(connThree));
  EXPECT_CALL(observer1, OnConnectionDestroyed).Times(0);
  EXPECT_CALL(observer2, OnConnectionDestroyed).Times(0);
  controller.OnConnectionDestroyed(connThree);
}

TEST_F(BridgeIceControllerTest, HandlesImmediateSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  EXPECT_CALL(observer, OnObserverAttached(_));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* connTwo = GetConnection(kIpTwo, kPort);
  ASSERT_NE(connTwo, nullptr);

  IceSwitchReason reason = IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE;
  std::vector<const Connection*> conns_to_forget{connTwo};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      conn,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  // ICE controller should switch to given connection immediately.
  Sequence check_then_switch;
  EXPECT_CALL(*wrapped, ShouldSwitchConnection(reason, conn))
      .InSequence(check_then_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(reason, switch_result)))
      .InSequence(check_then_switch);
  EXPECT_CALL(agent, SwitchSelectedConnection(conn, reason))
      .InSequence(check_then_switch);
  EXPECT_CALL(agent, ForgetLearnedStateForConnections(
                         ElementsAreArray(conns_to_forget)));

  EXPECT_TRUE(controller.OnImmediateSwitchRequest(reason, conn));

  // No rechecks before recheck delay.
  env.FastForwardBy(base::Milliseconds(recheck_delay_ms - 1));

  // ICE controller should recheck for best connection after the recheck delay.
  Sequence recheck_sort;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(recheck_sort);
  EXPECT_CALL(*wrapped,
              SortAndSwitchConnection(IceSwitchReason::ICE_CONTROLLER_RECHECK))
      .InSequence(recheck_sort)
      .WillOnce(Return(kEmptySwitchResult));
  // Empty switch proposal could be eliminated, but reason may be interesting.
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(
                  IceSwitchReason::ICE_CONTROLLER_RECHECK, kEmptySwitchResult)))
      .InSequence(recheck_sort);
  EXPECT_CALL(agent, ForgetLearnedStateForConnections(IsEmpty()))
      .InSequence(recheck_sort);
  // Recheck should check if anything needs pruning.
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(recheck_sort)
      .WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(recheck_sort);

  env.FastForwardBy(kTick);
}

TEST_F(BridgeIceControllerTest, HandlesImmediateSortAndSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  EXPECT_CALL(observer, OnObserverAttached(_));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* connTwo = GetConnection(kIpTwo, kPort);
  ASSERT_NE(connTwo, nullptr);
  const Connection* connThree = GetConnection(kIpThree, kPort);
  ASSERT_NE(connThree, nullptr);

  IceSwitchReason reason = IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
  std::vector<const Connection*> conns_to_forget{connTwo};
  std::vector<const Connection*> conns_to_prune{connThree};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      conn,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  Sequence sort_and_switch;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(reason))
      .InSequence(sort_and_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(reason, switch_result)))
      .InSequence(sort_and_switch);
  EXPECT_CALL(agent, SwitchSelectedConnection(conn, reason))
      .InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(sort_and_switch)
      .WillOnce(Return(conns_to_prune));
  EXPECT_CALL(observer, OnPruneProposal(PruneProposalEq(conns_to_prune)))
      .InSequence(sort_and_switch);
  EXPECT_CALL(agent, PruneConnections(ElementsAreArray(conns_to_prune)))
      .InSequence(sort_and_switch);

  controller.OnImmediateSortAndSwitchRequest(reason);

  // No rechecks before recheck delay.
  env.FastForwardBy(base::Milliseconds(recheck_delay_ms - 1));

  // ICE controller should recheck for best connection after the recheck
  // delay.
  Sequence recheck_sort;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(recheck_sort);
  EXPECT_CALL(*wrapped,
              SortAndSwitchConnection(IceSwitchReason::ICE_CONTROLLER_RECHECK))
      .InSequence(recheck_sort)
      .WillOnce(Return(IceControllerInterface::SwitchResult{}));
  // Empty switch proposal could be eliminated, but reason may be interesting.
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(
                  IceSwitchReason::ICE_CONTROLLER_RECHECK, kEmptySwitchResult)))
      .InSequence(recheck_sort);
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(recheck_sort)
      .WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(recheck_sort);

  env.FastForwardBy(kTick);
}

TEST_F(BridgeIceControllerTest, HandlesSortAndSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  EXPECT_CALL(observer, OnObserverAttached(_));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* connTwo = GetConnection(kIpTwo, kPort);
  ASSERT_NE(connTwo, nullptr);

  IceSwitchReason reason = IceSwitchReason::NETWORK_PREFERENCE_CHANGE;

  // No action should occur immediately
  EXPECT_CALL(agent, UpdateConnectionStates()).Times(0);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(_)).Times(0);
  EXPECT_CALL(observer, OnSwitchProposal(_)).Times(0);
  EXPECT_CALL(agent, SwitchSelectedConnection(_, _)).Times(0);

  controller.OnSortAndSwitchRequest(reason);

  std::vector<const Connection*> conns_to_forget{connTwo};
  int recheck_delay_ms = 10;
  IceControllerInterface::SwitchResult switch_result{
      conn,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  // Sort and switch should take place as the subsequent task.
  Sequence sort_and_switch;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(reason))
      .InSequence(sort_and_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(reason, switch_result)))
      .InSequence(sort_and_switch);
  EXPECT_CALL(agent, SwitchSelectedConnection(conn, reason))
      .InSequence(sort_and_switch);

  // Pick up the first task.
  env.FastForwardBy(kTick);
}

TEST_F(BridgeIceControllerTest, StartPingingAfterSortAndSwitch) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<NiceMockIceController> will_move =
      std::make_unique<NiceMockIceController>(IceControllerFactoryArgs{});
  NiceMockIceController* wrapped = will_move.get();
  EXPECT_CALL(observer, OnObserverAttached(_));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);

  // Pinging does not start automatically, unless triggered through a sort.
  EXPECT_CALL(*wrapped, HasPingableConnection()).Times(0);
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(observer, OnPingProposal(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);

  // Pinging does not start if no pingable connection.
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(IceSwitchReason::DATA_RECEIVED))
      .WillOnce(Return(kEmptySwitchResult));
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(IceSwitchReason::DATA_RECEIVED,
                                                kEmptySwitchResult)));
  EXPECT_CALL(*wrapped, HasPingableConnection()).WillOnce(Return(false));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(observer, OnPingProposal(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  // Pick up the first task.
  env.FastForwardBy(kTick);

  int recheck_delay_ms = 10;
  IceControllerInterface::PingResult ping_result(conn, recheck_delay_ms);
  IceControllerInterface::PingResult empty_ping_result(/* conn= */ nullptr,
                                                       recheck_delay_ms);

  // Pinging starts when there is a pingable connection.
  Sequence start_pinging;
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(IceSwitchReason::DATA_RECEIVED))
      .InSequence(start_pinging)
      .WillOnce(Return(kEmptySwitchResult));
  EXPECT_CALL(observer,
              OnSwitchProposal(SwitchProposalEq(IceSwitchReason::DATA_RECEIVED,
                                                kEmptySwitchResult)))
      .InSequence(start_pinging);
  EXPECT_CALL(*wrapped, HasPingableConnection())
      .InSequence(start_pinging)
      .WillOnce(Return(true));
  EXPECT_CALL(agent, OnStartedPinging()).InSequence(start_pinging);
  EXPECT_CALL(agent, GetLastPingSentMs())
      .InSequence(start_pinging)
      .WillOnce(Return(123));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(123))
      .InSequence(start_pinging)
      .WillOnce(Return(ping_result));
  EXPECT_CALL(observer, OnPingProposal(PingProposalEq(ping_result)))
      .InSequence(start_pinging);
  EXPECT_CALL(agent, SendPingRequest(conn)).InSequence(start_pinging);

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);
  env.FastForwardBy(kTick);

  // ICE controller should recheck and ping after the recheck delay.
  // No ping should be sent if no connection selected to ping.
  EXPECT_CALL(agent, GetLastPingSentMs()).WillOnce(Return(456));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(456))
      .WillOnce(Return(empty_ping_result));
  EXPECT_CALL(observer, OnPingProposal(PingProposalEq(empty_ping_result)));
  EXPECT_CALL(agent, SendPingRequest(conn)).Times(0);

  env.FastForwardBy(base::Milliseconds(recheck_delay_ms));
}

}  // unnamed namespace
