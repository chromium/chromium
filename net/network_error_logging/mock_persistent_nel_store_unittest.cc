// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/network_error_logging/mock_persistent_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

NetworkErrorLoggingService::NelPolicy MakePolicy(
    const url::Origin& origin,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  NetworkErrorLoggingService::NelPolicy policy;
  policy.key = NetworkErrorLoggingService::NelPolicyKey(
      network_anonymization_key, origin);
  policy.expires = base::Time();
  policy.last_used = base::Time();

  return policy;
}

void RunClosureOnNelPoliciesLoaded(
    base::OnceClosure closure,
    std::vector<NetworkErrorLoggingService::NelPolicy>* policies_out,
    std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies) {
  std::move(closure).Run();
  loaded_policies.swap(*policies_out);
}

// Makes a NelPoliciesLoadedCallback that will fail if it's never run before
// destruction.
MockPersistentNelStore::NelPoliciesLoadedCallback
MakeExpectedRunNelPoliciesLoadedCallback(
    std::vector<NetworkErrorLoggingService::NelPolicy>* policies_out) {
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  return base::BindOnce(&RunClosureOnNelPoliciesLoaded, std::move(closure),
                        policies_out);
}

class MockPersistentNelStoreTest : public testing::Test {
 public:
  MockPersistentNelStoreTest() = default;
  ~MockPersistentNelStoreTest() override = default;

 protected:
  const url::Origin origin_ =
      url::Origin::Create(GURL("https://example.test/"));
  const NetworkAnonymizationKey network_anonymization_key_ =
      NetworkAnonymizationKey::CreateCrossSite(
          SchemefulSite(GURL("https://foo.test/")));
  const NetworkErrorLoggingService::NelPolicy nel_policy_ =
      MakePolicy(origin_, network_anonymization_key_);
};

// Test that FinishLoading() runs the callback.
TEST_F(MockPersistentNelStoreTest, FinishLoading) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));

  // Test should not crash because the callback has been run.
}

TEST_F(MockPersistentNelStoreTest, PreStoredPolicies) {
  const url::Origin origin_ =
      url::Origin::Create(GURL("https://example.test/"));

  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      nel_policy_};
  store.SetPrestoredPolicies(std::move(prestored_policies));
  EXPECT_EQ(1, store.StoredPoliciesCount());

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  store.FinishLoading(true /* load_success */);
  ASSERT_EQ(1u, loaded_policies.size());
  EXPECT_EQ(origin_, loaded_policies[0].key.origin);
  EXPECT_EQ(network_anonymization_key_,
            loaded_policies[0].key.network_anonymization_key);

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

// Failed load should yield empty vector of policies.
TEST_F(MockPersistentNelStoreTest, FailedLoad) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      nel_policy_};
  store.SetPrestoredPolicies(std::move(prestored_policies));
  EXPECT_EQ(1, store.StoredPoliciesCount());

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  store.FinishLoading(false /* load_success */);
  // The pre-stored policy is not returned because loading failed.
  EXPECT_EQ(0u, loaded_policies.size());

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

TEST_F(MockPersistentNelStoreTest, Add) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NelPolicy policy = nel_policy_;
  store.AddNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy);
  // Add operation will be queued; the policy has not actually been stored yet
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());

  EXPECT_EQ(3u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

TEST_F(MockPersistentNelStoreTest, AddThenDelete) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NelPolicy policy = nel_policy_;
  store.AddNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.DeleteNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy);
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(4u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

TEST_F(MockPersistentNelStoreTest, AddFlushThenDelete) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NelPolicy policy = nel_policy_;
  store.AddNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.DeleteNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::DELETE_NEL_POLICY, policy);
  EXPECT_EQ(4u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(5u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

TEST_F(MockPersistentNelStoreTest, AddThenUpdate) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::NelPolicy policy = nel_policy_;
  store.AddNelPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.UpdateNelPolicyAccessTime(policy);
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::UPDATE_NEL_POLICY, policy);
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNelStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());
  EXPECT_EQ(4u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
}

}  // namespace

}  // namespace net
