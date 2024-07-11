// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_watcher.h"

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace key = ::policy::key;

using testing::_;

MATCHER_P(IsPolicies, dict, "") {
  bool equal = (*arg == *dict);
  if (!equal) {
    std::string actual_value;
    base::JSONWriter::WriteWithOptions(
        *arg, base::JSONWriter::OPTIONS_PRETTY_PRINT, &actual_value);

    std::string expected_value;
    base::JSONWriter::WriteWithOptions(
        *dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &expected_value);

    *result_listener << "Policies are not equal. ";
    *result_listener << "Expected policy: " << expected_value << ". ";
    *result_listener << "Actual policy: " << actual_value << ".";
  }
  return equal;
}

MATCHER_P(ContainsSubstring, substring, "") {
  const std::string& log_message = ::testing::get<0>(arg);
  return log_message.find(substring) != std::string::npos;
}

class MockPolicyCallback {
 public:
  MockPolicyCallback() = default;

  MockPolicyCallback(const MockPolicyCallback&) = delete;
  MockPolicyCallback& operator=(const MockPolicyCallback&) = delete;

  // TODO(lukasza): gmock cannot mock a method taking std::unique_ptr<T>...
  MOCK_METHOD1(OnPolicyUpdatePtr, void(const base::Value::Dict* policies));
  void OnPolicyUpdate(base::Value::Dict policies) {
    OnPolicyUpdatePtr(&policies);
  }

  MOCK_METHOD0(OnPolicyError, void());
};

class PolicyWatcherTest : public testing::Test {
 public:
  PolicyWatcherTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    // We expect no callbacks unless explicitly specified by individual tests.
    EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(testing::_)).Times(0);
    EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(0);

    // Retaining a raw pointer to keep control over policy contents.
    policy_loader_ = new policy::FakeAsyncPolicyLoader(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    policy_watcher_ = PolicyWatcher::CreateFromPolicyLoaderForTesting(
        base::WrapUnique(policy_loader_.get()));

    policy_watcher_default_values_ = PolicyWatcher::GetDefaultPolicies();

    base::Value::List host_domain;
    host_domain.Append(kHostDomain);
    base::Value::List client_domain;
    client_domain.Append(kClientDomain);
    base::Value::List multiple_host_domains;
    multiple_host_domains.Append("a.com");
    multiple_host_domains.Append("b.com");
    multiple_host_domains.Append("c.com");
    base::Value::List multiple_client_domains;
    multiple_client_domains.Append("d.com");
    multiple_client_domains.Append("e.com");
    multiple_client_domains.Append("f.com");

    nat_true_.Set(key::kRemoteAccessHostFirewallTraversal, true);
    nat_false_.Set(key::kRemoteAccessHostFirewallTraversal, false);
    nat_one_.Set(key::kRemoteAccessHostFirewallTraversal, 1);
    nat_one_domain_full_.Set(key::kRemoteAccessHostFirewallTraversal, 1);
    nat_one_domain_full_.Set(key::kRemoteAccessHostDomainList,
                             host_domain.Clone());
    domain_empty_.Set(key::kRemoteAccessHostDomainList, base::Value::List());
    domain_full_.Set(key::kRemoteAccessHostDomainList, host_domain.Clone());
    SetDefaults(nat_true_others_default_);
    nat_true_others_default_.Set(key::kRemoteAccessHostFirewallTraversal, true);
    SetDefaults(nat_false_others_default_);
    nat_false_others_default_.Set(key::kRemoteAccessHostFirewallTraversal,
                                  false);
    SetDefaults(domain_empty_others_default_);
    domain_empty_others_default_.Set(key::kRemoteAccessHostDomainList,
                                     base::Value::List());
    SetDefaults(domain_full_others_default_);
    domain_full_others_default_.Set(key::kRemoteAccessHostDomainList,
                                    host_domain.Clone());
    nat_true_domain_empty_.Set(key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_empty_.Set(key::kRemoteAccessHostDomainList,
                               base::Value::List());
    nat_true_domain_full_.Set(key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_full_.Set(key::kRemoteAccessHostDomainList,
                              host_domain.Clone());
    nat_false_domain_empty_.Set(key::kRemoteAccessHostFirewallTraversal, false);
    nat_false_domain_empty_.Set(key::kRemoteAccessHostDomainList,
                                base::Value::List());
    nat_false_domain_full_.Set(key::kRemoteAccessHostFirewallTraversal, false);
    nat_false_domain_full_.Set(key::kRemoteAccessHostDomainList,
                               host_domain.Clone());
    SetDefaults(nat_true_domain_empty_others_default_);
    nat_true_domain_empty_others_default_.Set(
        key::kRemoteAccessHostFirewallTraversal, true);
    nat_true_domain_empty_others_default_.Set(key::kRemoteAccessHostDomainList,
                                              base::Value::List());
    unknown_policies_.Set("UnknownPolicyOne", std::string());
    unknown_policies_.Set("UnknownPolicyTwo", std::string());
    unknown_policies_.Set("RemoteAccessHostUnknownPolicyThree", true);

#if !BUILDFLAG(IS_CHROMEOS)
    pairing_true_.Set(key::kRemoteAccessHostAllowClientPairing, true);
    pairing_false_.Set(key::kRemoteAccessHostAllowClientPairing, false);
    gnubby_auth_true_.Set(key::kRemoteAccessHostAllowGnubbyAuth, true);
    gnubby_auth_false_.Set(key::kRemoteAccessHostAllowGnubbyAuth, false);
    curtain_true_.Set(key::kRemoteAccessHostRequireCurtain, true);
    curtain_false_.Set(key::kRemoteAccessHostRequireCurtain, false);
#endif
    relay_true_.Set(key::kRemoteAccessHostAllowRelayedConnection, true);
    relay_false_.Set(key::kRemoteAccessHostAllowRelayedConnection, false);
    port_range_full_.Set(key::kRemoteAccessHostUdpPortRange, kPortRange);
    port_range_empty_.Set(key::kRemoteAccessHostUdpPortRange, std::string());
    port_range_malformed_.Set(key::kRemoteAccessHostUdpPortRange, "malformed");
    port_range_malformed_domain_full_.Merge(port_range_malformed_.Clone());
    port_range_malformed_domain_full_.Set(key::kRemoteAccessHostDomainList,
                                          host_domain.Clone());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    username_true_.Set(key::kRemoteAccessHostMatchUsername, true);
    username_false_.Set(key::kRemoteAccessHostMatchUsername, false);
#endif

#if BUILDFLAG(IS_WIN)
    remote_assistance_uiaccess_true_.Set(
        key::kRemoteAccessHostAllowUiAccessForRemoteAssistance, true);
    remote_assistance_uiaccess_false_.Set(
        key::kRemoteAccessHostAllowUiAccessForRemoteAssistance, false);
#endif

    deprecated_policies_.Set(key::kRemoteAccessHostDomain, kHostDomain);
    deprecated_policies_.Set(key::kRemoteAccessHostClientDomain, kClientDomain);
    // Deprecated policies should get converted if new ones aren't present.
    SetDefaults(deprecated_policies_expected_);
    deprecated_policies_expected_.Set(key::kRemoteAccessHostDomainList,
                                      host_domain.Clone());
    deprecated_policies_expected_.Set(key::kRemoteAccessHostClientDomainList,
                                      client_domain.Clone());

    deprecated_and_new_policies_.Set(key::kRemoteAccessHostDomain, kHostDomain);
    deprecated_and_new_policies_.Set(key::kRemoteAccessHostClientDomain,
                                     kClientDomain);
    deprecated_and_new_policies_.Set(key::kRemoteAccessHostDomainList,
                                     multiple_host_domains.Clone());
    deprecated_and_new_policies_.Set(key::kRemoteAccessHostClientDomainList,
                                     multiple_client_domains.Clone());
    // Deprecated policies should just be dropped in new ones are present.
    SetDefaults(deprecated_and_new_policies_expected_);
    deprecated_and_new_policies_expected_.Set(key::kRemoteAccessHostDomainList,
                                              multiple_host_domains.Clone());
    deprecated_and_new_policies_expected_.Set(
        key::kRemoteAccessHostClientDomainList,
        multiple_client_domains.Clone());

    // Empty strings should be treated as not set.
    deprecated_empty_strings_.Set(key::kRemoteAccessHostDomain, "");
    deprecated_empty_strings_.Set(key::kRemoteAccessHostClientDomain, "");
  }

  void TearDown() override {
    policy_watcher_.reset();
    policy_loader_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void StartWatching() {
    policy_watcher_->StartWatching(
        base::BindRepeating(&MockPolicyCallback::OnPolicyUpdate,
                            base::Unretained(&mock_policy_callback_)),
        base::BindRepeating(&MockPolicyCallback::OnPolicyError,
                            base::Unretained(&mock_policy_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  void SetPolicies(const base::Value::Dict& dict) {
    // Copy |dict| into |policy_bundle|.
    policy::PolicyNamespace policy_namespace =
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
    policy::PolicyBundle policy_bundle;
    policy::PolicyMap& policy_map = policy_bundle.Get(policy_namespace);
    policy_map.LoadFrom(dict, policy::POLICY_LEVEL_MANDATORY,
                        policy::POLICY_SCOPE_MACHINE,
                        policy::POLICY_SOURCE_CLOUD);

    // Simulate a policy file/registry/preference update.
    policy_loader_->SetPolicies(policy_bundle);
    policy_loader_->PostReloadOnBackgroundThread(true /* force reload asap */);
    base::RunLoop().RunUntilIdle();
  }

  const policy::Schema* GetPolicySchema() {
    return policy_watcher_->GetPolicySchema();
  }

  const base::Value::Dict& GetDefaultValues() {
    return policy_watcher_default_values_;
  }

  MOCK_METHOD0(PostPolicyWatcherShutdown, void());

  static const char* kHostDomain;
  static const char* kClientDomain;
  static const char* kPortRange;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockPolicyCallback mock_policy_callback_;

  // |policy_loader_| is owned by |policy_watcher_|. PolicyWatcherTest retains
  // a raw pointer to |policy_loader_| in order to control the simulated / faked
  // policy contents.
  raw_ptr<policy::FakeAsyncPolicyLoader> policy_loader_;
  std::unique_ptr<PolicyWatcher> policy_watcher_;

  base::Value::Dict empty_;
  base::Value::Dict nat_true_;
  base::Value::Dict nat_false_;
  base::Value::Dict nat_one_;
  base::Value::Dict nat_one_domain_full_;
  base::Value::Dict domain_empty_;
  base::Value::Dict domain_full_;
  base::Value::Dict nat_true_others_default_;
  base::Value::Dict nat_false_others_default_;
  base::Value::Dict domain_empty_others_default_;
  base::Value::Dict domain_full_others_default_;
  base::Value::Dict nat_true_domain_empty_;
  base::Value::Dict nat_true_domain_full_;
  base::Value::Dict nat_false_domain_empty_;
  base::Value::Dict nat_false_domain_full_;
  base::Value::Dict nat_true_domain_empty_others_default_;
  base::Value::Dict unknown_policies_;
  base::Value::Dict pairing_true_;
  base::Value::Dict pairing_false_;
  base::Value::Dict gnubby_auth_true_;
  base::Value::Dict gnubby_auth_false_;
  base::Value::Dict relay_true_;
  base::Value::Dict relay_false_;
  base::Value::Dict port_range_full_;
  base::Value::Dict port_range_empty_;
  base::Value::Dict port_range_malformed_;
  base::Value::Dict port_range_malformed_domain_full_;
  base::Value::Dict curtain_true_;
  base::Value::Dict curtain_false_;
  base::Value::Dict username_true_;
  base::Value::Dict username_false_;
  base::Value::Dict remote_assistance_uiaccess_true_;
  base::Value::Dict remote_assistance_uiaccess_false_;
  base::Value::Dict deprecated_policies_;
  base::Value::Dict deprecated_policies_expected_;
  base::Value::Dict deprecated_and_new_policies_;
  base::Value::Dict deprecated_and_new_policies_expected_;
  base::Value::Dict deprecated_empty_strings_;

 private:
  void SetDefaults(base::Value::Dict& dict) {
    dict.Set(key::kRemoteAccessHostFirewallTraversal, true);
    dict.Set(key::kRemoteAccessHostAllowRelayedConnection, true);
    dict.Set(key::kRemoteAccessHostUdpPortRange, "");
    dict.Set(key::kRemoteAccessHostClientDomainList, base::Value::List());
    dict.Set(key::kRemoteAccessHostDomainList, base::Value::List());
    dict.Set(key::kRemoteAccessHostClipboardSizeBytes, -1);
    dict.Set(key::kRemoteAccessHostAllowRemoteSupportConnections, true);
#if BUILDFLAG(IS_CHROMEOS)
    dict.Set(key::kRemoteAccessHostAllowEnterpriseRemoteSupportConnections,
             true);
    dict.Set(key::kRemoteAccessHostAllowEnterpriseFileTransfer, false);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
    dict.Set(key::kRemoteAccessHostMatchUsername, false);
#endif
#if !BUILDFLAG(IS_CHROMEOS)
    dict.Set(key::kRemoteAccessHostRequireCurtain, false);
    dict.Set(key::kRemoteAccessHostAllowClientPairing, true);
    dict.Set(key::kRemoteAccessHostAllowGnubbyAuth, true);
    dict.Set(key::kRemoteAccessHostAllowFileTransfer, true);
    dict.Set(key::kRemoteAccessHostAllowUrlForwarding, true);
    dict.Set(key::kRemoteAccessHostEnableUserInterface, true);
    dict.Set(key::kRemoteAccessHostAllowRemoteAccessConnections, true);
    dict.Set(key::kRemoteAccessHostMaximumSessionDurationMinutes, 0);
    dict.Set(key::kRemoteAccessHostAllowPinAuthentication, base::Value());
#endif
#if BUILDFLAG(IS_WIN)
    dict.Set(key::kRemoteAccessHostAllowUiAccessForRemoteAssistance, false);
#endif

    ASSERT_THAT(&dict, IsPolicies(&GetDefaultValues()))
        << "Sanity check that defaults expected by the test code "
        << "match what is stored in PolicyWatcher::default_values_";
  }

  base::Value::Dict policy_watcher_default_values_;
};

const char* PolicyWatcherTest::kHostDomain = "google.com";
const char* PolicyWatcherTest::kClientDomain = "client.com";
const char* PolicyWatcherTest::kPortRange = "12400-12409";

TEST_F(PolicyWatcherTest, None) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(nat_true_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatFalse) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_others_default_)));

  SetPolicies(nat_false_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatWrongType) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError());

  SetPolicies(nat_one_);
  StartWatching();
}

// This test verifies that a mistyped policy value is still detected
// even though it doesn't change during the second SetPolicies call.
TEST_F(PolicyWatcherTest, NatWrongTypeThenIrrelevantChange) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(2);

  SetPolicies(nat_one_);
  StartWatching();
  SetPolicies(nat_one_domain_full_);
}

// This test verifies that a malformed policy value is still detected
// even though it doesn't change during the second SetPolicies call.
TEST_F(PolicyWatcherTest, PortRangeMalformedThenIrrelevantChange) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError()).Times(2);

  SetPolicies(port_range_malformed_);
  StartWatching();
  SetPolicies(port_range_malformed_domain_full_);
}

TEST_F(PolicyWatcherTest, DomainEmpty) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_others_default_)));

  SetPolicies(domain_empty_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, DomainFull) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_others_default_)));

  SetPolicies(domain_full_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, NatNoneThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrue) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  SetPolicies(nat_true_);
}

TEST_F(PolicyWatcherTest, NatNoneThenTrueThenTrueThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_true_);
  SetPolicies(nat_true_);
  SetPolicies(nat_false_);
}

TEST_F(PolicyWatcherTest, NatNoneThenFalse) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_false_);
}

TEST_F(PolicyWatcherTest, NatNoneThenFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_, OnPolicyUpdatePtr(IsPolicies(&nat_true_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(nat_false_);
  SetPolicies(nat_true_);
}

TEST_F(PolicyWatcherTest, ChangeOneRepeatedlyThenTwo) {
  testing::InSequence sequence;
  EXPECT_CALL(
      mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_empty_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_empty_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_full_)));

  SetPolicies(nat_true_domain_empty_);
  StartWatching();
  SetPolicies(nat_true_domain_full_);
  SetPolicies(nat_false_domain_full_);
  SetPolicies(nat_false_domain_empty_);
  SetPolicies(nat_true_domain_full_);
}

TEST_F(PolicyWatcherTest, FilterUnknownPolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(unknown_policies_);
  SetPolicies(empty_);
}

class MisspelledPolicyTest : public PolicyWatcherTest,
                             public ::testing::WithParamInterface<const char*> {
};

// Verify that a misspelled policy causes a warning written to the log.
TEST_P(MisspelledPolicyTest, WarningLogged) {
  const char* misspelled_policy_name = GetParam();
  base::test::MockLog mock_log;

  ON_CALL(mock_log, Log(_, _, _, _, _)).WillByDefault(testing::Return(true));

#if BUILDFLAG(IS_WIN)
  // The PolicyWatcher on Windows tries to open a handle to the Chrome policy
  // registry key on Windows which fails on the Chromium bots. The warning that
  // gets logged cases the subsequent log assertion to fail so this check was
  // added so the test runs locally and in the bot environment.
  EXPECT_CALL(mock_log, Log(logging::LOGGING_WARNING, _, _, _, _))
      .With(testing::Args<4>(
          ContainsSubstring("Failed to open Chrome policy registry key")))
      .Times(testing::AtMost(1));
#endif

  EXPECT_CALL(mock_log, Log(logging::LOGGING_WARNING, _, _, _, _))
      .With(testing::Args<4>(ContainsSubstring(misspelled_policy_name)))
      .Times(1);

  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));

  base::Value::Dict misspelled_policies;
  misspelled_policies.Set(misspelled_policy_name, "some test value");
  mock_log.StartCapturingLogs();

  SetPolicies(misspelled_policies);
  StartWatching();

  mock_log.StopCapturingLogs();
}

INSTANTIATE_TEST_SUITE_P(
    PolicyWatcherTest,
    MisspelledPolicyTest,
    ::testing::Values("RemoteAccessHostDomainX",
                      "XRemoteAccessHostDomain",
                      "RemoteAccessHostdomain",
                      "RemoteAccessHostPolicyForFutureVersion"));

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PolicyWatcherTest, PairingFalseThenTrue) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&pairing_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&pairing_true_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(pairing_false_);
  SetPolicies(pairing_true_);
}

TEST_F(PolicyWatcherTest, GnubbyAuth) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&gnubby_auth_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&gnubby_auth_true_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(gnubby_auth_false_);
  SetPolicies(gnubby_auth_true_);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PolicyWatcherTest, RemoteAssistanceUiAccess) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
#if BUILDFLAG(IS_WIN)
  // This setting only affects Windows, it is ignored on other platforms so the
  // 2 SetPolicies calls won't result in any calls to OnPolicyUpdate.
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&remote_assistance_uiaccess_true_)));
  EXPECT_CALL(
      mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&remote_assistance_uiaccess_false_)));
#endif  // BUILDFLAG(IS_WIN)

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(remote_assistance_uiaccess_true_);
  SetPolicies(remote_assistance_uiaccess_false_);
}

TEST_F(PolicyWatcherTest, Relay) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&relay_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&relay_true_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(relay_false_);
  SetPolicies(relay_true_);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PolicyWatcherTest, Curtain) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&curtain_true_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&curtain_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(curtain_true_);
  SetPolicies(curtain_false_);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
TEST_F(PolicyWatcherTest, MatchUsername) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&username_true_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&username_false_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(username_true_);
  SetPolicies(username_false_);
}
#endif
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PolicyWatcherTest, UdpPortRange) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&port_range_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&port_range_empty_)));

  SetPolicies(empty_);
  StartWatching();
  SetPolicies(port_range_full_);
  SetPolicies(port_range_empty_);
}

TEST_F(PolicyWatcherTest, PolicySchemaAndPolicyWatcherShouldBeInSync) {
  // This test verifies that
  // 1) policy schema (generated out of policy_templates.json)
  // and
  // 2) PolicyWatcher's code (i.e. contents of the |default_values_| field)
  // are kept in-sync.

  std::map<std::string, base::Value::Type> expected_schema;
#if BUILDFLAG(IS_CHROMEOS)
  base::flat_set<std::string> policies_with_no_default_values;
#else
  base::flat_set<std::string> policies_with_no_default_values = {
      policy::key::kRemoteAccessHostAllowPinAuthentication};
#endif
  for (auto i : GetDefaultValues()) {
    if (policies_with_no_default_values.contains(i.first)) {
      // This policy has no default value, so we need to explicitly set the
      // expected type.
      expected_schema[i.first] = base::Value::Type::BOOLEAN;
    } else {
      expected_schema[i.first] = i.second.type();
    }
  }

  std::map<std::string, base::Value::Type> actual_schema;
  const policy::Schema* schema = GetPolicySchema();
  ASSERT_TRUE(schema->valid());
  for (auto it = schema->GetPropertiesIterator(); !it.IsAtEnd(); it.Advance()) {
    std::string key = it.key();
    if (key.find("RemoteAccessHost") == std::string::npos) {
      // For now PolicyWatcher::GetPolicySchema() mixes Chrome and Chromoting
      // policies, so we have to skip them here.
      continue;
    }
    if (key == policy::key::kRemoteAccessHostDomain ||
        key == policy::key::kRemoteAccessHostClientDomain) {
      // These policies are deprecated and get removed during normalization
      continue;
    }
    actual_schema[key] = it.schema().type();
  }

  EXPECT_THAT(actual_schema, testing::ContainerEq(expected_schema));
}

TEST_F(PolicyWatcherTest, SchemaTypeCheck) {
  const policy::Schema* schema = GetPolicySchema();
  ASSERT_TRUE(schema->valid());

  // Check one, random "string" policy to see if the type propagated correctly
  // from policy_templates.json file.
  const policy::Schema string_schema =
      schema->GetKnownProperty("RemoteAccessHostUdpPortRange");
  EXPECT_TRUE(string_schema.valid());
  EXPECT_EQ(string_schema.type(), base::Value::Type::STRING);

  // Check one, random "integer" policy to see if the type propagated correctly
  // from policy_templates.json file.
  const policy::Schema int_schema =
      schema->GetKnownProperty("RemoteAccessHostClipboardSizeBytes");
  EXPECT_TRUE(int_schema.valid());
  EXPECT_EQ(int_schema.type(), base::Value::Type::INTEGER);

  // And check one, random "boolean" policy to see if the type propagated
  // correctly from policy_templates.json file.
  const policy::Schema boolean_schema =
      schema->GetKnownProperty("RemoteAccessHostAllowRelayedConnection");
  EXPECT_TRUE(boolean_schema.valid());
  EXPECT_EQ(boolean_schema.type(), base::Value::Type::BOOLEAN);
}

TEST_F(PolicyWatcherTest, DeprecatedOnly) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&deprecated_policies_expected_)));
  SetPolicies(deprecated_policies_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, DeprecatedAndNew) {
  EXPECT_CALL(
      mock_policy_callback_,
      OnPolicyUpdatePtr(IsPolicies(&deprecated_and_new_policies_expected_)));
  SetPolicies(deprecated_and_new_policies_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, DeprecatedEmpty) {
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&GetDefaultValues())));
  SetPolicies(deprecated_empty_strings_);
  StartWatching();
}

TEST_F(PolicyWatcherTest, GetEffectivePolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_others_default_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  StartWatching();
  SetPolicies(nat_false_);
  base::Value::Dict effective_policies =
      policy_watcher_->GetEffectivePolicies();
  ASSERT_TRUE(effective_policies == nat_false_others_default_);
}

TEST_F(PolicyWatcherTest, GetEffectivePoliciesError) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError());

  SetPolicies(nat_one_);
  StartWatching();
  base::Value::Dict effective_policies =
      policy_watcher_->GetEffectivePolicies();
  ASSERT_EQ(0u, effective_policies.size());
}

TEST_F(PolicyWatcherTest, GetPlatformPolicies) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&GetDefaultValues())));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));

  StartWatching();
  ASSERT_EQ(0u, policy_watcher_->GetPlatformPolicies().size());
  SetPolicies(nat_false_);
  ASSERT_EQ(1u, policy_watcher_->GetPlatformPolicies().size());
}

TEST_F(PolicyWatcherTest, GetPlatformPoliciesMultipleOverrides) {
  testing::InSequence sequence;
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&GetDefaultValues())));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&domain_full_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_false_)));
  EXPECT_CALL(mock_policy_callback_,
              OnPolicyUpdatePtr(IsPolicies(&nat_true_domain_empty_)));

  StartWatching();
  ASSERT_EQ(0u, policy_watcher_->GetPlatformPolicies().size());
  SetPolicies(domain_full_);
  ASSERT_EQ(1u, policy_watcher_->GetPlatformPolicies().size());
  SetPolicies(nat_false_domain_full_);
  ASSERT_EQ(2u, policy_watcher_->GetPlatformPolicies().size());
  SetPolicies(nat_true_domain_empty_);
  ASSERT_EQ(2u, policy_watcher_->GetPlatformPolicies().size());
}

TEST_F(PolicyWatcherTest, GetPlatformPoliciesError) {
  EXPECT_CALL(mock_policy_callback_, OnPolicyError());

  SetPolicies(nat_one_);
  StartWatching();
  ASSERT_EQ(0u, policy_watcher_->GetPlatformPolicies().size());
}

}  // namespace remoting
