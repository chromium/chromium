// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/bridge_ice_controller.h"

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/peerconnection/fake_connection_test_base.h"
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

namespace cricket {
// This is an opaque type for the purposes of this test, so a forward
// declaration suffices
struct IceConfig;
}  // namespace cricket

namespace {

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
using ::testing::Combine;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithArgs;
using ::testing::WithParamInterface;

using ::blink::ConnectionEq;
using ::blink::PingProposalEq;
using ::blink::PruneProposalEq;
using ::blink::SwitchProposalEq;

using ::base::test::SingleThreadTaskEnvironment;
using ::base::test::TaskEnvironment;

static const std::string kIp = "1.2.3.4";
static const std::string kIpTwo = "1.3.5.7";
static const std::string kIpThree = "1.4.7.10";
static const int kPort = 6745;

static const IceConfig kIceConfig;

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

class BridgeIceControllerTest : public ::blink::FakeConnectionTestBase {};

enum class ProposalResponse {
  ACCEPT,
  REJECT,
};
using PingProposalResponse = ProposalResponse;
using SwitchProposalResponse = ProposalResponse;
using PruneProposalResponse = ProposalResponse;

class BridgeIceControllerProposalTest
    : public BridgeIceControllerTest,
      public WithParamInterface<std::tuple<PingProposalResponse,
                                           SwitchProposalResponse,
                                           PruneProposalResponse>> {
 protected:
  BridgeIceControllerProposalTest()
      : should_accept_ping_proposal(std::get<0>(GetParam()) ==
                                    ProposalResponse::ACCEPT),
        should_accept_switch_proposal(std::get<1>(GetParam()) ==
                                      ProposalResponse::ACCEPT),
        should_accept_prune_proposal(std::get<2>(GetParam()) ==
                                     ProposalResponse::ACCEPT) {}

  const bool should_accept_ping_proposal;
  const bool should_accept_switch_proposal;
  const bool should_accept_prune_proposal;
};

std::string ToTestSuffix(std::string type, ProposalResponse response) {
  return base::StrCat(
      {(response == ProposalResponse::ACCEPT ? "Accept" : "Reject"), "", type});
}

std::string MakeTestName(
    const TestParamInfo<BridgeIceControllerProposalTest::ParamType>& info) {
  return base::StrCat({ToTestSuffix("Ping", std::get<0>(info.param)), "_",
                       ToTestSuffix("Switch", std::get<1>(info.param)), "_",
                       ToTestSuffix("Prune", std::get<2>(info.param))});
}

INSTANTIATE_TEST_SUITE_P(All,
                         BridgeIceControllerProposalTest,
                         Combine(Values(PingProposalResponse::ACCEPT,
                                        PingProposalResponse::REJECT),
                                 Values(SwitchProposalResponse::ACCEPT,
                                        SwitchProposalResponse::REJECT),
                                 Values(PruneProposalResponse::ACCEPT,
                                        PruneProposalResponse::REJECT)),
                         MakeTestName);

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
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);
  const Connection* conn_three = GetConnection(kIpThree, kPort);
  ASSERT_NE(conn_three, nullptr);

  EXPECT_CALL(*wrapped, SetIceConfig(Ref(kIceConfig)));
  controller.SetIceConfig(kIceConfig);

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

  EXPECT_CALL(*wrapped, AddConnection(conn_two));
  EXPECT_CALL(observer1, OnConnectionAdded).Times(0);
  EXPECT_CALL(observer2, OnConnectionAdded(ConnectionEq(conn_two)));
  controller.OnConnectionAdded(conn_two);

  EXPECT_CALL(*wrapped, SetSelectedConnection(conn_two));
  EXPECT_CALL(observer1, OnConnectionSwitched).Times(0);
  EXPECT_CALL(observer2, OnConnectionSwitched(ConnectionEq(conn_two)));
  controller.OnConnectionSwitched(conn_two);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(conn_two));
  EXPECT_CALL(observer1, OnConnectionDestroyed).Times(0);
  EXPECT_CALL(observer2, OnConnectionDestroyed(ConnectionEq(conn_two)));
  controller.OnConnectionDestroyed(conn_two);

  EXPECT_CALL(observer2, OnObserverDetached);
  controller.AttachObserver(nullptr);

  EXPECT_CALL(*wrapped, AddConnection(conn_three));
  EXPECT_CALL(observer1, OnConnectionAdded).Times(0);
  EXPECT_CALL(observer2, OnConnectionAdded).Times(0);
  controller.OnConnectionAdded(conn_three);

  EXPECT_CALL(*wrapped, SetSelectedConnection(conn_three));
  EXPECT_CALL(observer1, OnConnectionSwitched).Times(0);
  EXPECT_CALL(observer2, OnConnectionSwitched).Times(0);
  controller.OnConnectionSwitched(conn_three);

  EXPECT_CALL(*wrapped, OnConnectionDestroyed(conn_three));
  EXPECT_CALL(observer1, OnConnectionDestroyed).Times(0);
  EXPECT_CALL(observer2, OnConnectionDestroyed).Times(0);
  controller.OnConnectionDestroyed(conn_three);
}

TEST_F(BridgeIceControllerTest, HandlesImmediateSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);

  // Set default native ICE controller behaviour.
  const std::vector<const Connection*> connection_set{conn, conn_two};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));
  EXPECT_CALL(*wrapped, HasPingableConnection).WillRepeatedly(Return(false));

  const IceSwitchReason reason = IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE;
  const std::vector<const Connection*> conns_to_forget{conn_two};
  const int recheck_delay_ms = 10;
  const IceControllerInterface::SwitchResult switch_result{
      conn,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  // ICE controller should switch to given connection immediately.
  Sequence check_then_switch;
  EXPECT_CALL(*wrapped, ShouldSwitchConnection(reason, conn))
      .InSequence(check_then_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(observer, OnSwitchProposal(SwitchProposalEq(
                            reason, switch_result, /*reply_expected*/ false)))
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
  EXPECT_CALL(observer, OnSwitchProposal(SwitchProposalEq(
                            IceSwitchReason::ICE_CONTROLLER_RECHECK,
                            kEmptySwitchResult, /*reply_expected*/ false)))
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

TEST_P(BridgeIceControllerProposalTest, HandlesImmediateSortAndSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);
  const Connection* conn_three = GetConnection(kIpThree, kPort);
  ASSERT_NE(conn_three, nullptr);

  // Set default native ICE controller behaviour.
  const std::vector<const Connection*> connection_set{conn, conn_two,
                                                      conn_three};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));
  EXPECT_CALL(*wrapped, HasPingableConnection).WillRepeatedly(Return(false));

  const IceSwitchReason reason =
      IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE;
  const std::vector<const Connection*> conns_to_forget{conn_two};
  const std::vector<const Connection*> conns_to_prune{conn_three};
  const int recheck_delay_ms = 10;
  const IceControllerInterface::SwitchResult switch_result{
      conn,
      IceRecheckEvent(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms),
      conns_to_forget};

  Sequence sort_and_switch;
  EXPECT_CALL(agent, UpdateConnectionStates()).InSequence(sort_and_switch);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(reason))
      .InSequence(sort_and_switch)
      .WillOnce(Return(switch_result));
  EXPECT_CALL(observer, OnSwitchProposal(_))
      .InSequence(sort_and_switch)
      .WillOnce(WithArgs<0>([&](auto switch_proposal) {
        EXPECT_THAT(switch_proposal, SwitchProposalEq(reason, switch_result,
                                                      /*reply_expected*/ true));
        if (should_accept_switch_proposal) {
          interaction_agent->AcceptSwitchProposal(switch_proposal);
        } else {
          interaction_agent->RejectSwitchProposal(switch_proposal);
        }
      }));
  // Only expect a switch to occur if switch proposal is accepted. Further state
  // update occurs regardless.
  if (should_accept_switch_proposal) {
    EXPECT_CALL(agent, SwitchSelectedConnection(conn, reason))
        .InSequence(sort_and_switch);
  }
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(sort_and_switch)
      .WillOnce(Return(conns_to_prune));
  EXPECT_CALL(observer, OnPruneProposal(_))
      .InSequence(sort_and_switch)
      .WillOnce(WithArgs<0>([&](auto prune_proposal) {
        EXPECT_THAT(prune_proposal,
                    PruneProposalEq(conns_to_prune, /*reply_expected*/ true));
        if (should_accept_prune_proposal) {
          interaction_agent->AcceptPruneProposal(prune_proposal);
        } else {
          interaction_agent->RejectPruneProposal(prune_proposal);
        }
      }));
  // Only expect a pruning to occur if prune proposal is accepted. Recheck
  // occurs regardless.
  if (should_accept_prune_proposal) {
    EXPECT_CALL(agent, PruneConnections(ElementsAreArray(conns_to_prune)))
        .InSequence(sort_and_switch);
  }

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
  EXPECT_CALL(observer, OnSwitchProposal(SwitchProposalEq(
                            IceSwitchReason::ICE_CONTROLLER_RECHECK,
                            kEmptySwitchResult, /*reply_expected*/ false)))
      .InSequence(recheck_sort);
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(recheck_sort)
      .WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(recheck_sort);

  env.FastForwardBy(kTick);
}

TEST_P(BridgeIceControllerProposalTest, HandlesSortAndSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);

  // Set default native ICE controller behaviour.
  const std::vector<const Connection*> connection_set{conn, conn_two};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));
  EXPECT_CALL(*wrapped, HasPingableConnection).WillRepeatedly(Return(false));

  const IceSwitchReason reason = IceSwitchReason::NETWORK_PREFERENCE_CHANGE;

  // No action should occur immediately
  EXPECT_CALL(agent, UpdateConnectionStates()).Times(0);
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(_)).Times(0);
  EXPECT_CALL(observer, OnSwitchProposal(_)).Times(0);
  EXPECT_CALL(agent, SwitchSelectedConnection(_, _)).Times(0);

  controller.OnSortAndSwitchRequest(reason);

  const std::vector<const Connection*> conns_to_forget{conn_two};
  const int recheck_delay_ms = 10;
  const IceControllerInterface::SwitchResult switch_result{
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
  EXPECT_CALL(observer, OnSwitchProposal(_))
      .InSequence(sort_and_switch)
      .WillOnce(WithArgs<0>([&](auto switch_proposal) {
        EXPECT_THAT(switch_proposal, SwitchProposalEq(reason, switch_result,
                                                      /*reply_expected*/ true));
        if (should_accept_switch_proposal) {
          interaction_agent->AcceptSwitchProposal(switch_proposal);
        } else {
          interaction_agent->RejectSwitchProposal(switch_proposal);
        }
      }));
  // Only expect a switch to occur if switch proposal is accepted. Further state
  // update occurs regardless.
  if (should_accept_switch_proposal) {
    EXPECT_CALL(agent, SwitchSelectedConnection(conn, reason))
        .InSequence(sort_and_switch);
  }
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(sort_and_switch)
      .WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(sort_and_switch);

  // Pick up the first task.
  env.FastForwardBy(kTick);
}

TEST_P(BridgeIceControllerProposalTest, StartPingingAfterSortAndSwitch) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);

  // Set default native ICE controller behaviour.
  const std::vector<const Connection*> connection_set{conn};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));

  // Pinging does not start automatically, unless triggered through a sort.
  EXPECT_CALL(*wrapped, HasPingableConnection()).Times(0);
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(observer, OnPingProposal(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);

  // Pinging does not start if no pingable connection.
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(IceSwitchReason::DATA_RECEIVED))
      .WillOnce(Return(kEmptySwitchResult));
  EXPECT_CALL(observer, OnSwitchProposal(SwitchProposalEq(
                            IceSwitchReason::DATA_RECEIVED, kEmptySwitchResult,
                            /*reply_expected*/ false)));
  EXPECT_CALL(*wrapped, PruneConnections()).WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty()));
  EXPECT_CALL(*wrapped, HasPingableConnection()).WillOnce(Return(false));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(_)).Times(0);
  EXPECT_CALL(observer, OnPingProposal(_)).Times(0);
  EXPECT_CALL(agent, OnStartedPinging()).Times(0);

  // Pick up the first task.
  env.FastForwardBy(kTick);

  const int recheck_delay_ms = 10;
  const IceControllerInterface::PingResult ping_result(conn, recheck_delay_ms);
  const IceControllerInterface::PingResult empty_ping_result(nullptr,
                                                             recheck_delay_ms);

  // Pinging starts when there is a pingable connection.
  Sequence start_pinging;
  EXPECT_CALL(*wrapped, SortAndSwitchConnection(IceSwitchReason::DATA_RECEIVED))
      .InSequence(start_pinging)
      .WillOnce(Return(kEmptySwitchResult));
  EXPECT_CALL(observer, OnSwitchProposal(SwitchProposalEq(
                            IceSwitchReason::DATA_RECEIVED, kEmptySwitchResult,
                            /*reply_expected*/ false)))
      .InSequence(start_pinging);
  EXPECT_CALL(*wrapped, PruneConnections())
      .InSequence(start_pinging)
      .WillOnce(Return(kEmptyConnsList));
  // No need to propose pruning if nothing to do.
  EXPECT_CALL(observer, OnPruneProposal).Times(0);
  EXPECT_CALL(agent, PruneConnections(IsEmpty())).InSequence(start_pinging);
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
  EXPECT_CALL(observer, OnPingProposal(_))
      .InSequence(start_pinging)
      .WillOnce(WithArgs<0>([&](auto ping_proposal) {
        EXPECT_THAT(ping_proposal, PingProposalEq(ping_result,
                                                  /*reply_expected*/ true));
        if (should_accept_ping_proposal) {
          interaction_agent->AcceptPingProposal(ping_proposal);
        } else {
          interaction_agent->RejectPingProposal(ping_proposal);
        }
      }));
  // Only expect a ping to occur if ping proposal is accepted. Recheck occurs
  // regardless.
  if (should_accept_ping_proposal) {
    EXPECT_CALL(agent, SendPingRequest(conn)).InSequence(start_pinging);
  }

  controller.OnSortAndSwitchRequest(IceSwitchReason::DATA_RECEIVED);
  env.FastForwardBy(kTick);

  // ICE controller should recheck and ping after the recheck delay.
  // No ping should be sent if no connection selected to ping.
  EXPECT_CALL(agent, GetLastPingSentMs()).WillOnce(Return(456));
  EXPECT_CALL(*wrapped, SelectConnectionToPing(456))
      .WillOnce(Return(empty_ping_result));
  EXPECT_CALL(observer,
              OnPingProposal(PingProposalEq(empty_ping_result,
                                            /*reply_expected*/ false)));
  EXPECT_CALL(agent, SendPingRequest(conn)).Times(0);

  env.FastForwardBy(base::Milliseconds(recheck_delay_ms));
}

// Tests that verify correct handling of invalid proposals.
class BridgeIceControllerInvalidProposalTest : public BridgeIceControllerTest {
 protected:
  BridgeIceControllerInvalidProposalTest()
      : recheck_event(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                      recheck_delay_ms) {
    std::unique_ptr<StrictMock<MockIceController>> will_move =
        std::make_unique<StrictMock<MockIceController>>(
            IceControllerFactoryArgs{});
    wrapped_controller = will_move.get();

    EXPECT_CALL(observer, OnObserverAttached(_))
        .WillOnce(
            WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
    controller = std::make_unique<BridgeIceController>(
        env.GetMainThreadTaskRunner(), &observer, &agent, std::move(will_move));

    conn = GetConnection(kIp, kPort);
    EXPECT_NE(conn, nullptr);
    conn_two = GetConnection(kIpTwo, kPort);
    EXPECT_NE(conn_two, nullptr);

    // Exclude conn_two to be able to test for unknown connection in proposal.
    const std::vector<const Connection*> connection_set{conn};
    EXPECT_CALL(*wrapped_controller, GetConnections())
        .WillRepeatedly(Return(connection_set));

    // No expectations set on any mocks. Together with StrictMock, this ensures
    // that invalid proposal actions with side-effects will cause a test
    // failure.
  }

  void Recheck() { env.FastForwardBy(base::Milliseconds(recheck_delay_ms)); }

  const int recheck_delay_ms = 10;
  raw_ptr<const Connection> conn = nullptr;
  raw_ptr<const Connection> conn_two = nullptr;
  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION const std::vector<const Connection*>
      empty_conns_to_forget{};
  const IceSwitchReason reason = IceSwitchReason::DATA_RECEIVED;
  const IceRecheckEvent recheck_event;

  scoped_refptr<IceInteractionInterface> interaction_agent;
  StrictMock<MockIceAgent> agent;
  StrictMock<MockIceControllerObserver> observer;
  std::unique_ptr<BridgeIceController> controller;
  raw_ptr<StrictMock<MockIceController>> wrapped_controller;
};

// Alias for verifying DCHECKs. This test suite should be used for death tests.
using BridgeIceControllerDeathTest = BridgeIceControllerInvalidProposalTest;
// Alias for verifying no side-effects, without hitting a DCHECK.
using BridgeIceControllerNoopTest = BridgeIceControllerInvalidProposalTest;

TEST_F(BridgeIceControllerDeathTest, AcceptUnsolicitedPingProposal) {
  const IceControllerInterface::PingResult ping_result(conn, recheck_delay_ms);
  const IcePingProposal proposal(ping_result, /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->AcceptPingProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerDeathTest, RejectUnsolicitedPingProposal) {
  const IceControllerInterface::PingResult ping_result(conn, recheck_delay_ms);
  const IcePingProposal proposal(ping_result, /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->RejectPingProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerDeathTest, AcceptEmptyPingProposal) {
  const IceControllerInterface::PingResult null_ping_result(nullptr,
                                                            recheck_delay_ms);
  const IcePingProposal proposal(null_ping_result, /*reply_expected=*/true);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->AcceptPingProposal(proposal),
                           "without a connection");
}

TEST_F(BridgeIceControllerNoopTest, AcceptUnknownPingProposal) {
  const IceControllerInterface::PingResult ping_result(conn_two,
                                                       recheck_delay_ms);
  const IcePingProposal proposal(ping_result, /*reply_expected=*/true);
  interaction_agent->AcceptPingProposal(proposal);
  Recheck();
}

TEST_F(BridgeIceControllerDeathTest, AcceptUnsolicitedSwitchProposal) {
  const IceControllerInterface::SwitchResult switch_result{
      conn.get(), recheck_event, empty_conns_to_forget};
  const IceSwitchProposal proposal(reason, switch_result,
                                   /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->AcceptSwitchProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerDeathTest, RejectUnsolicitedSwitchProposal) {
  const IceControllerInterface::SwitchResult switch_result{
      conn.get(), recheck_event, empty_conns_to_forget};
  const IceSwitchProposal proposal(reason, switch_result,
                                   /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->RejectSwitchProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerDeathTest, AcceptEmptySwitchProposal) {
  const IceControllerInterface::SwitchResult switch_result{
      std::nullopt, recheck_event, empty_conns_to_forget};
  const IceSwitchProposal proposal(reason, switch_result,
                                   /*reply_expected=*/true);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->AcceptSwitchProposal(proposal),
                           "without a connection");
}

TEST_F(BridgeIceControllerDeathTest, AcceptNullSwitchProposal) {
  const IceControllerInterface::SwitchResult switch_result{
      std::optional<const Connection*>(nullptr), recheck_event,
      empty_conns_to_forget};
  const IceSwitchProposal proposal(reason, switch_result,
                                   /*reply_expected=*/true);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->AcceptSwitchProposal(proposal),
                           "without a connection");
}

TEST_F(BridgeIceControllerNoopTest, AcceptUnknownSwitchProposal) {
  const IceControllerInterface::SwitchResult switch_result{
      conn_two.get(), recheck_event, empty_conns_to_forget};
  const IceSwitchProposal proposal(reason, switch_result,
                                   /*reply_expected=*/true);
  interaction_agent->AcceptSwitchProposal(proposal);
  Recheck();
}

TEST_F(BridgeIceControllerDeathTest, AcceptUnsolicitedPruneProposal) {
  std::vector<const Connection*> conns_to_prune{conn};
  const IcePruneProposal proposal(conns_to_prune, /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->RejectPruneProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerDeathTest, RejectUnsolicitedPruneProposal) {
  std::vector<const Connection*> conns_to_prune{conn};
  const IcePruneProposal proposal(conns_to_prune, /*reply_expected=*/false);
  EXPECT_DCHECK_DEATH_WITH(interaction_agent->RejectPruneProposal(proposal),
                           "unsolicited");
}

TEST_F(BridgeIceControllerInvalidProposalTest, AcceptUnknownPruneProposal) {
  std::vector<const Connection*> conns_to_prune{conn_two};
  const IcePruneProposal proposal(conns_to_prune, /*reply_expected=*/true);
  EXPECT_CALL(agent, UpdateState);
  EXPECT_CALL(*wrapped_controller, HasPingableConnection);
  interaction_agent->RejectPruneProposal(proposal);
}

TEST_F(BridgeIceControllerTest, HandlesPingRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);

  // Exclude conn_two to be able to test for unknown connection in request.
  const std::vector<const Connection*> connection_set{conn};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));

  EXPECT_CALL(agent, SendPingRequest(conn));
  EXPECT_EQ(interaction_agent->PingIceConnection(IceConnection(conn)).type(),
            webrtc::RTCErrorType::NONE);

  EXPECT_CALL(agent, SendPingRequest).Times(0);
  EXPECT_EQ(
      interaction_agent->PingIceConnection(IceConnection(conn_two)).type(),
      webrtc::RTCErrorType::INVALID_PARAMETER);
}

TEST_F(BridgeIceControllerTest, HandlesSwitchRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);

  // Exclude conn_two to be able to test for unknown connection in request.
  const std::vector<const Connection*> connection_set{conn};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));

  EXPECT_CALL(agent, SwitchSelectedConnection(
                         conn, IceSwitchReason::APPLICATION_REQUESTED));
  EXPECT_EQ(
      interaction_agent->SwitchToIceConnection(IceConnection(conn)).type(),
      webrtc::RTCErrorType::NONE);

  EXPECT_CALL(agent, SwitchSelectedConnection).Times(0);
  EXPECT_EQ(
      interaction_agent->SwitchToIceConnection(IceConnection(conn_two)).type(),
      webrtc::RTCErrorType::INVALID_PARAMETER);
}

TEST_F(BridgeIceControllerTest, HandlesPruneRequest) {
  NiceMock<MockIceAgent> agent;
  MockIceControllerObserver observer;
  std::unique_ptr<MockIceController> will_move =
      std::make_unique<MockIceController>(IceControllerFactoryArgs{});
  MockIceController* wrapped = will_move.get();

  scoped_refptr<IceInteractionInterface> interaction_agent = nullptr;
  EXPECT_CALL(observer, OnObserverAttached(_))
      .WillOnce(
          WithArgs<0>([&](auto ia) { interaction_agent = std::move(ia); }));
  BridgeIceController controller(env.GetMainThreadTaskRunner(), &observer,
                                 &agent, std::move(will_move));

  const Connection* conn = GetConnection(kIp, kPort);
  ASSERT_NE(conn, nullptr);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  ASSERT_NE(conn_two, nullptr);
  const Connection* conn_three = GetConnection(kIpThree, kPort);
  ASSERT_NE(conn_three, nullptr);

  // Exclude conn_three to be able to test for unknown connection in request.
  const std::vector<const Connection*> connection_set{conn, conn_two};
  EXPECT_CALL(*wrapped, GetConnections())
      .WillRepeatedly(Return(connection_set));

  const std::vector<const Connection*> conns_to_prune{conn};
  const std::vector<IceConnection> valid_ice_conns_to_prune{
      IceConnection(conn)};
  const std::vector<const Connection*> partial_conns_to_prune{conn_two};
  const std::vector<IceConnection> mixed_ice_conns_to_prune{
      IceConnection(conn_two), IceConnection(conn_three)};
  const std::vector<IceConnection> invalid_ice_conns_to_prune{
      IceConnection(conn_three)};

  EXPECT_CALL(agent, PruneConnections(ElementsAreArray(conns_to_prune)));
  EXPECT_EQ(
      interaction_agent->PruneIceConnections(valid_ice_conns_to_prune).type(),
      webrtc::RTCErrorType::NONE);

  // Invalid/unknown connections are ignored in a prune request, but the request
  // itself doesn't fail.

  EXPECT_CALL(agent,
              PruneConnections(ElementsAreArray(partial_conns_to_prune)));
  EXPECT_EQ(
      interaction_agent->PruneIceConnections(mixed_ice_conns_to_prune).type(),
      webrtc::RTCErrorType::NONE);

  EXPECT_CALL(agent, PruneConnections).Times(0);
  EXPECT_EQ(
      interaction_agent->PruneIceConnections(invalid_ice_conns_to_prune).type(),
      webrtc::RTCErrorType::NONE);
}

}  // unnamed namespace
