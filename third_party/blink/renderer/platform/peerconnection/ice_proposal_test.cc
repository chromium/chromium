// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/ice_ping_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_prune_proposal.h"
#include "third_party/webrtc_overrides/p2p/base/ice_switch_proposal.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/peerconnection/fake_connection_test_base.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_connection_matchers.h"

#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"

namespace {

using ::cricket::Connection;
using ::cricket::IceControllerInterface;
using ::cricket::IceRecheckEvent;
using ::cricket::IceSwitchReason;

using ::blink::IcePingProposal;
using ::blink::IcePruneProposal;
using ::blink::IceSwitchProposal;
using ::blink::PingProposalEq;
using ::blink::PruneProposalEq;
using ::blink::SwitchProposalEq;

static const std::string kIp = "1.2.3.4";
static const std::string kIpTwo = "1.3.5.7";
static const int kPort = 6745;

class IceProposalTest : public blink::FakeConnectionTestBase {};

TEST_F(IceProposalTest, ConstructIcePingProposal) {
  const Connection* conn = GetConnection(kIp, kPort);
  const int recheck_delay_ms = 10;
  const bool reply_expected = true;

  IceControllerInterface::PingResult ping_result(conn, recheck_delay_ms);
  EXPECT_THAT(IcePingProposal(ping_result, reply_expected),
              PingProposalEq(ping_result, reply_expected));

  IceControllerInterface::PingResult null_ping_result(nullptr,
                                                      recheck_delay_ms);
  EXPECT_THAT(IcePingProposal(null_ping_result, reply_expected),
              PingProposalEq(null_ping_result, reply_expected));
}

TEST_F(IceProposalTest, ConstructIceSwitchProposal) {
  const Connection* conn = GetConnection(kIp, kPort);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  const IceSwitchReason reason = IceSwitchReason::CONNECT_STATE_CHANGE;
  const int recheck_delay_ms = 10;
  const bool reply_expected = true;
  const IceRecheckEvent recheck_event(IceSwitchReason::ICE_CONTROLLER_RECHECK,
                                      recheck_delay_ms);
  std::vector<const Connection*> conns_to_forget{conn_two};
  std::vector<const Connection*> empty_conns_to_forget{};
  std::vector<const Connection*> null_conns_to_forget{nullptr};

  IceControllerInterface::SwitchResult switch_result{conn, recheck_event,
                                                     conns_to_forget};
  EXPECT_THAT(IceSwitchProposal(reason, switch_result, reply_expected),
              SwitchProposalEq(reason, switch_result, reply_expected));

  IceControllerInterface::SwitchResult empty_switch_result{
      std::nullopt, recheck_event, conns_to_forget};
  EXPECT_THAT(IceSwitchProposal(reason, empty_switch_result, reply_expected),
              SwitchProposalEq(reason, empty_switch_result, reply_expected));

  IceControllerInterface::SwitchResult null_switch_result{
      nullptr, recheck_event, conns_to_forget};
  EXPECT_THAT(IceSwitchProposal(reason, null_switch_result, reply_expected),
              SwitchProposalEq(reason, null_switch_result, reply_expected));

  IceControllerInterface::SwitchResult switch_result_no_recheck{
      conn, std::nullopt, conns_to_forget};
  EXPECT_THAT(
      IceSwitchProposal(reason, switch_result_no_recheck, reply_expected),
      SwitchProposalEq(reason, switch_result_no_recheck, reply_expected));

  IceControllerInterface::SwitchResult switch_result_empty_conns_to_forget{
      conn, recheck_event, empty_conns_to_forget};
  EXPECT_THAT(IceSwitchProposal(reason, switch_result_empty_conns_to_forget,
                                reply_expected),
              SwitchProposalEq(reason, switch_result_empty_conns_to_forget,
                               reply_expected));

  IceControllerInterface::SwitchResult switch_result_null_conns_to_forget{
      conn, recheck_event, null_conns_to_forget};
  EXPECT_THAT(IceSwitchProposal(reason, switch_result_null_conns_to_forget,
                                reply_expected),
              SwitchProposalEq(reason, switch_result_null_conns_to_forget,
                               reply_expected));
}

TEST_F(IceProposalTest, ConstructIcePruneProposal) {
  const Connection* conn = GetConnection(kIp, kPort);
  const Connection* conn_two = GetConnection(kIpTwo, kPort);
  const bool reply_expected = true;

  std::vector<const Connection*> conns_to_prune{conn, conn_two};
  EXPECT_THAT(IcePruneProposal(conns_to_prune, reply_expected),
              PruneProposalEq(conns_to_prune, reply_expected));

  std::vector<const Connection*> empty_conns_to_prune{};
  EXPECT_THAT(IcePruneProposal(empty_conns_to_prune, reply_expected),
              PruneProposalEq(empty_conns_to_prune, reply_expected));

  std::vector<const Connection*> null_conns_to_prune{nullptr};
  EXPECT_THAT(IcePruneProposal(null_conns_to_prune, reply_expected),
              PruneProposalEq(null_conns_to_prune, reply_expected));

  std::vector<const Connection*> mixed_conns_to_prune{nullptr, conn, nullptr};
  EXPECT_THAT(IcePruneProposal(mixed_conns_to_prune, reply_expected),
              PruneProposalEq(mixed_conns_to_prune, reply_expected));
}

}  // unnamed namespace
