// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_policies_from_dict.h"

#include <optional>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

const SessionPolicies kFullSessionPolicies = {
    .clipboard_size_bytes = 1024,
    .allow_stun_connections = true,
    .allow_relayed_connections = false,
    .host_udp_port_range =
        {
            .min_port = 123,
            .max_port = 456,
        },
#if !BUILDFLAG(IS_CHROMEOS)
    .allow_file_transfer = true,
    .allow_uri_forwarding = false,
    .maximum_session_duration = base::Hours(20),
    .curtain_required = false,
#endif
};

const base::Value::Dict& GetFullSessionPolicyDict() {
  static const base::NoDestructor<base::Value::Dict> dict(
      base::Value::Dict()
          .Set(policy::key::kRemoteAccessHostClipboardSizeBytes, 1024)
          .Set(policy::key::kRemoteAccessHostFirewallTraversal, true)
          .Set(policy::key::kRemoteAccessHostAllowRelayedConnection, false)
          .Set(policy::key::kRemoteAccessHostUdpPortRange, "123-456")
#if !BUILDFLAG(IS_CHROMEOS)
          .Set(policy::key::kRemoteAccessHostAllowFileTransfer, true)
          .Set(policy::key::kRemoteAccessHostAllowUrlForwarding, false)
          .Set(policy::key::kRemoteAccessHostMaximumSessionDurationMinutes,
               1200)
          .Set(policy::key::kRemoteAccessHostRequireCurtain, false)
#endif
  );
  return *dict;
}

#if !BUILDFLAG(IS_CHROMEOS)
base::Value::Dict GetPolicyDictWithMaxDurationMins(int mins) {
  return GetFullSessionPolicyDict().Clone().Set(
      policy::key::kRemoteAccessHostMaximumSessionDurationMinutes, mins);
}
#endif

base::Value::Dict GetPolicyDictWithClipboardSize(int clipboard_size) {
  return GetFullSessionPolicyDict().Clone().Set(
      policy::key::kRemoteAccessHostClipboardSizeBytes, clipboard_size);
}

}  // namespace

TEST(SessionPoliciesFromDict, EmptyDict_CreatesEmptyPolicies) {
  std::optional<SessionPolicies> policies =
      SessionPoliciesFromDict(base::Value::Dict());
  EXPECT_EQ(*policies, SessionPolicies());
}

TEST(SessionPoliciesFromDict, FullDict_CreatesFullPolicies) {
  std::optional<SessionPolicies> policies =
      SessionPoliciesFromDict(GetFullSessionPolicyDict());
  EXPECT_EQ(*policies, kFullSessionPolicies);
}

TEST(SessionPoliciesFromDict, PartialDict_CreatesPartialPolicies) {
  base::Value::Dict policy_dict = GetFullSessionPolicyDict().Clone();
  policy_dict.Remove(policy::key::kRemoteAccessHostClipboardSizeBytes);
  policy_dict.Remove(policy::key::kRemoteAccessHostUdpPortRange);

  std::optional<SessionPolicies> policies =
      SessionPoliciesFromDict(policy_dict);

  SessionPolicies expected_policies = kFullSessionPolicies;
  expected_policies.clipboard_size_bytes.reset();
  expected_policies.host_udp_port_range.reset();
  EXPECT_EQ(*policies, expected_policies);
}

TEST(SessionPoliciesFromDict,
     FirewallTraversalDisabled_DisablesStunAndRelayedConnections) {
  base::Value::Dict policy_dict =
      GetFullSessionPolicyDict()
          .Clone()
          .Set(policy::key::kRemoteAccessHostFirewallTraversal, false)
          .Set(policy::key::kRemoteAccessHostAllowRelayedConnection, true);

  std::optional<SessionPolicies> policies =
      SessionPoliciesFromDict(policy_dict);

  SessionPolicies expected_policies = kFullSessionPolicies;
  expected_policies.allow_stun_connections = false;
  expected_policies.allow_relayed_connections = false;
  EXPECT_EQ(*policies, expected_policies);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST(SessionPoliciesFromDict, InvalidMaxSessionDuration_ReturnsNullopt) {
  EXPECT_EQ(SessionPoliciesFromDict(GetPolicyDictWithMaxDurationMins(-1)),
            std::nullopt);
  EXPECT_EQ(SessionPoliciesFromDict(GetPolicyDictWithMaxDurationMins(10)),
            std::nullopt);
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
TEST(SessionPoliciesFromDict, ZeroMaxSessionDuration_FieldIsNullopt) {
  SessionPolicies expected_policies = kFullSessionPolicies;
  expected_policies.maximum_session_duration.reset();
  EXPECT_EQ(SessionPoliciesFromDict(GetPolicyDictWithMaxDurationMins(0)),
            expected_policies);
}
#endif

TEST(SessionPoliciesFromDict, InvalidHostUdpPortRange_ReturnsNullopt) {
  base::Value::Dict policy_dict = GetFullSessionPolicyDict().Clone().Set(
      policy::key::kRemoteAccessHostUdpPortRange, "456-123");
  EXPECT_EQ(SessionPoliciesFromDict(policy_dict), std::nullopt);
}

TEST(SessionPoliciesFromDict, NegativeClipboardSize_FieldIsNullopt) {
  SessionPolicies expected_policies = kFullSessionPolicies;
  expected_policies.clipboard_size_bytes.reset();
  EXPECT_EQ(SessionPoliciesFromDict(GetPolicyDictWithClipboardSize(-1)),
            expected_policies);
}

TEST(SessionPoliciesFromDict, ZeroClipboardSize_FieldIsZero) {
  SessionPolicies expected_policies = kFullSessionPolicies;
  expected_policies.clipboard_size_bytes = 0;
  EXPECT_EQ(SessionPoliciesFromDict(GetPolicyDictWithClipboardSize(0)),
            expected_policies);
}

}  // namespace remoting
