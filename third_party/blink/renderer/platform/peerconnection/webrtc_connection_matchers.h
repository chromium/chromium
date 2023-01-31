// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_CONNECTION_MATCHERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_CONNECTION_MATCHERS_H_

#include <ostream>

#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "third_party/webrtc/p2p/base/connection.h"
#include "third_party/webrtc/p2p/base/ice_controller_interface.h"
#include "third_party/webrtc/p2p/base/ice_switch_reason.h"

#include "third_party/webrtc_overrides/p2p/base/ice_switch_proposal.h"

namespace cricket {

// Pretty prints a connection object for tests.
void PrintTo(const Connection* conn, std::ostream* os) {
  *os << conn->ToString();
}

// Pretty prints a ping result for tests.
void PrintTo(const IceControllerInterface::PingResult& result,
             std::ostream* os) {
  *os << "PingResult["
      << (result.connection.has_value() ? result.connection.value()->ToString()
                                        : "<no connection>")
      << ":" << result.recheck_delay_ms << "]";
}

// Pretty prints an ICE switch reason for tests.
void PrintTo(const IceSwitchReason reason, std::ostream* os) {
  *os << IceSwitchReasonToString(reason);
}

// Pretty prints an ICE recheck event for tests.
void PrintTo(const IceRecheckEvent& event, std::ostream* os) {
  *os << event.ToString();
}

// Pretty prints a switch result for tests.
void PrintTo(const IceControllerInterface::SwitchResult& result,
             std::ostream* os) {
  *os << "SwitchResult["
      << (result.connection.has_value() ? result.connection.value()->ToString()
                                        : "<no connection>")
      << ":"
      << (result.recheck_event.has_value() ? result.recheck_event->ToString()
                                           : "<no recheck>");
  int ctr = 1;
  for (const Connection* conn : result.connections_to_forget_state_on) {
    *os << "(" << ctr++ << ":" << conn->ToString() << ")";
  }
  *os << "]";
}

}  // namespace cricket

namespace blink {

namespace {

using ::testing::ExplainMatchResult;
using ::testing::Optional;
using ::testing::PrintToString;
using ::testing::UnorderedPointwise;

}  // unnamed namespace

// Tests the equality of a blink::IceConnection and a cricket::Connection.
MATCHER_P(ConnectionEq,
          /* const blink::IceConnection& arg, */
          /* const cricket::Connection* */ conn,
          base::StrCat({negation ? "doesn't match " : "matches ",
                        PrintToString(conn)})) {
  return conn != nullptr && arg.id() == conn->id() &&
         arg.local_candidate() == conn->local_candidate() &&
         arg.remote_candidate() == conn->remote_candidate() &&
         arg.connected() == conn->connected() &&
         arg.selected() == conn->selected() &&
         arg.last_ping_sent() == conn->last_ping_sent() &&
         arg.last_ping_received() == conn->last_ping_received() &&
         arg.last_data_received() == conn->last_data_received() &&
         arg.last_ping_response_received() ==
             conn->last_ping_response_received() &&
         arg.num_pings_sent() == conn->num_pings_sent();
}
// Helper to test the equality of a (blink::IceConnection, cricket::Connection)
// tuple using ConnectionEq for use with container matchers.
MATCHER(CricketBlinkConnectionTupleEq,
        /* std::tuple<const blink::IceConnection&, const cricket::Connection*>
           arg, */
        "") {
  return ExplainMatchResult(ConnectionEq(std::get<1>(arg)), std::get<0>(arg),
                            result_listener);
}

// Tests the equality of a blink::IcePingProposal and a cricket::PingResult.
MATCHER_P(PingProposalEq,
          /* const blink::IcePingProposal& arg, */
          /* const cricket::IceControllerInterface::PingResult& */ result,
          base::StrCat({negation ? "doesn't match " : "matches ",
                        PrintToString(result)})) {
  if (result.connection.has_value() != arg.connection().has_value()) {
    return false;
  }
  if (result.connection.has_value() &&
      !ExplainMatchResult(Optional(ConnectionEq(result.connection.value())),
                          arg.connection(), result_listener)) {
    return false;
  }
  if (!ExplainMatchResult(Optional(result.recheck_delay_ms),
                          arg.recheck_delay_ms(), result_listener)) {
    return false;
  }
  return true;
}

// Tests the equality of a blink::IceRecheckEvent and a
// cricket::IceRecheckEvent.
MATCHER_P(RecheckEventEq,
          /* const blink::IceRecheckEvent& arg, */
          /* const cricket::IceRecheckEvent& */ event,
          base::StrCat({negation ? "doesn't match " : "matches ",
                        PrintToString(event)})) {
  return blink::ConvertFromWebrtcIceSwitchReason(event.reason) == arg.reason &&
         event.recheck_delay_ms == arg.recheck_delay_ms;
}

// Tests the equality of a blink::IceSwitchProposal and a cricket::SwitchResult.
MATCHER_P2(SwitchProposalEq,
           /* const blink::IceSwitchProposal& arg, */
           /* const cricket::IceSwitchReason */ reason,
           /* const cricket::IceControllerInterface::SwitchResult& */ result,
           base::StrCat({negation ? "doesn't match " : "matches ",
                         PrintToString(result)})) {
  if (blink::ConvertFromWebrtcIceSwitchReason(reason) != arg.reason()) {
    return false;
  }
  if (result.connection.has_value() != arg.connection().has_value()) {
    return false;
  }
  if (result.connection.has_value() &&
      !ExplainMatchResult(Optional(ConnectionEq(result.connection.value())),
                          arg.connection(), result_listener)) {
    return false;
  }
  if (result.recheck_event.has_value() != arg.recheck_event().has_value()) {
    return false;
  }
  if (result.recheck_event.has_value() &&
      !ExplainMatchResult(
          Optional(RecheckEventEq(result.recheck_event.value())),
          arg.recheck_event(), result_listener)) {
    return false;
  }
  return ExplainMatchResult(
      UnorderedPointwise(CricketBlinkConnectionTupleEq(),
                         result.connections_to_forget_state_on),
      arg.connections_to_forget_state_on(), result_listener);
}

// Tests the equality of a blink::IceOruneProposal and a collection of
// cricket::Connections selected for pruning.
MATCHER_P(PruneProposalEq,
          /* const blink::IcePruneProposal& arg, */
          /* std::vector<const cricket::Connection*> */ connections,
          base::StrCat({negation ? "doesn't match " : "matches ",
                        PrintToString(connections)})) {
  return ExplainMatchResult(
      UnorderedPointwise(CricketBlinkConnectionTupleEq(), connections),
      arg.connections_to_prune(), result_listener);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_CONNECTION_MATCHERS_H_
