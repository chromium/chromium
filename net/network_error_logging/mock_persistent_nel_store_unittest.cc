// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/location.h"
#include "base/test/bind_test_util.h"
#include "net/network_error_logging/mock_persistent_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {
const url::Origin kOrigin = url::Origin::Create(GURL("https://example.test/"));

NetworkErrorLoggingService::NelPolicy MakePolicyForOrigin(url::Origin origin) {
  NetworkErrorLoggingService::NelPolicy policy;
  policy.origin = std::move(origin);
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

// Test that FinishLoading() runs the callback.
TEST(MockPersistentNelStoreTest, FinishLoading) {
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
  EXPECT_EQ("LOAD; ", store.GetDebugString());

  // Test should not crash because the callback has been run.
}

TEST(MockPersistentNelStoreTest, PreStoredPolicies) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      MakePolicyForOrigin(kOrigin)};
  store.SetPrestoredPolicies(std::move(prestored_policies));
  EXPECT_EQ(1, store.StoredPoliciesCount());

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  store.FinishLoading(true /* load_success */);
  ASSERT_EQ(1u, loaded_policies.size());
  EXPECT_EQ(kOrigin, loaded_policies[0].origin);

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ", store.GetDebugString());
}

// Failed load should yield empty vector of policies.
TEST(MockPersistentNelStoreTest, FailedLoad) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NelPolicy> prestored_policies = {
      MakePolicyForOrigin(kOrigin)};
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
  EXPECT_EQ("LOAD; ", store.GetDebugString());
}

TEST(MockPersistentNelStoreTest, Add) {
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

  NetworkErrorLoggingService::NelPolicy policy = MakePolicyForOrigin(kOrigin);
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
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNelStoreTest, AddThenDelete) {
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

  NetworkErrorLoggingService::NelPolicy policy = MakePolicyForOrigin(kOrigin);
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
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); "
                "DELETE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNelStoreTest, AddFlushThenDelete) {
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

  NetworkErrorLoggingService::NelPolicy policy = MakePolicyForOrigin(kOrigin);
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
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); FLUSH; "
                "DELETE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNelStoreTest, AddThenUpdate) {
  MockPersistentNelStore store;
  MockPersistentNelStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NelPolicy> loaded_policies;

  store.LoadNelPolicies(
      MakeExpectedRunNelPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNelStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::NelPolicy policy = MakePolicyForOrigin(kOrigin);
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
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); "
                "UPDATE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

}  // namespace

}  // namespace net
