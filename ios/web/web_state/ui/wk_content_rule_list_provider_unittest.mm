// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <WebKit/WebKit.h>

#import <set>
#import <string>
#import <vector>

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::TestFuture;

namespace web {
namespace {

// A valid, but minimal, content rule list for testing.
constexpr char kValidTestRuleListJson[] = R"([
  {
    "trigger": {"url-filter": ".*"},
    "action": {"type": "block"}
  }
])";
// An invalid JSON string for testing compilation failures.
constexpr char kInvalidTestRuleListJson[] = "this is not valid json";

// Helper to check for a rule list's presence in the store and wait.
// Returns a gtest AssertionResult for use with EXPECT_TRUE.
[[nodiscard]] testing::AssertionResult CheckStoreForRuleListSync(
    const std::string& identifier,
    bool expect_found = true) {
  TestFuture<WKContentRuleList*, NSError*> future;
  void (^completion_block)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(future.GetCallback());
  [WKContentRuleListStore.defaultStore
      lookUpContentRuleListForIdentifier:base::SysUTF8ToNSString(identifier)
                       completionHandler:completion_block];
  auto [rule_list, error] = future.Get();

  const bool was_found = (rule_list != nil);

  // First, handle any unexpected WebKit errors. This is always a failure.
  if (error && (![error.domain isEqualToString:WKErrorDomain] ||
                error.code != WKErrorContentRuleListStoreLookUpFailed)) {
    return testing::AssertionFailure()
           << "Unexpected error looking up content rule list with identifier '"
           << identifier << "': " << base::SysNSStringToUTF8(error.description);
  }

  // Now, check if the outcome matches the expectation.
  if (was_found == expect_found) {
    return testing::AssertionSuccess();
  }

  // The outcome did not match the expectation, return a specific failure.
  if (expect_found) {
    return testing::AssertionFailure()
           << "Expected to find content rule list with identifier '"
           << identifier << "', but it was not found.";
  } else {
    return testing::AssertionFailure()
           << "Expected content rule list with identifier '" << identifier
           << "' to be absent, but it was found.";
  }
}

class WKContentRuleListProviderTest : public PlatformTest {
 public:
  WKContentRuleListProviderTest()
      : provider_(std::make_unique<WKContentRuleListProvider>()) {}

 protected:
  void TearDown() override {
    // Ensure any lists created during tests are cleaned up.
    for (const auto& key : tracked_keys_) {
      NSString* identifier = base::SysUTF8ToNSString(key);
      TestFuture<NSError*> future;
      [WKContentRuleListStore.defaultStore
          removeContentRuleListForIdentifier:identifier
                           completionHandler:base::CallbackToBlock(
                                                 future.GetCallback())];
      // Wait for cleanup but ignore potential errors (e.g., if the list
      // was already removed by the test).
      std::ignore = future.Wait();
    }
    PlatformTest::TearDown();
  }

  // Helper to create or update a rule list and verify the operation's success.
  [[nodiscard]] testing::AssertionResult UpdateRuleListSync(
      const WKContentRuleListProvider::RuleListKey& key,
      const std::string& rules_list) {
    tracked_keys_.insert(key);
    TestFuture<NSError*> future;
    provider_->UpdateRuleList(key, rules_list, future.GetCallback());
    NSError* error = future.Get();
    if (error) {
      return testing::AssertionFailure()
             << "UpdateRuleList failed with error: "
             << base::SysNSStringToUTF8(error.description);
    }
    return testing::AssertionSuccess();
  }

  // Helper to remove a rule list and verify the operation's success.
  [[nodiscard]] testing::AssertionResult RemoveRuleListSync(
      const WKContentRuleListProvider::RuleListKey& key) {
    tracked_keys_.erase(key);
    TestFuture<NSError*> future;
    provider_->RemoveRuleList(key, future.GetCallback());
    NSError* error = future.Get();
    if (error) {
      return testing::AssertionFailure()
             << "RemoveRuleList failed with error: "
             << base::SysNSStringToUTF8(error.description);
    }
    return testing::AssertionSuccess();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<WKContentRuleListProvider> provider_;
  std::set<WKContentRuleListProvider::RuleListKey> tracked_keys_;
};

// Tests that a new list can be successfully created and then removed.
TEST_F(WKContentRuleListProviderTest, CreationAndRemoval) {
  const std::string key = "test_key";
  // 1. Create a valid list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Remove the list.
  EXPECT_TRUE(RemoveRuleListSync(key));
  EXPECT_TRUE(CheckStoreForRuleListSync(key, /*expect_found=*/false));
}

// Tests that an existing list can be successfully updated.
TEST_F(WKContentRuleListProviderTest, UpdateExistingList) {
  const std::string key = "test_key";
  // 1. Create an initial list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  ASSERT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Update the list with new (but still valid) rules.
  EXPECT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that attempting to create an invalid list fails gracefully.
TEST_F(WKContentRuleListProviderTest, CreationFailure) {
  const std::string key = "test_key";
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());

  NSError* error = future.Get();
  ASSERT_NE(nil, error);
  EXPECT_NSEQ(WKErrorDomain, error.domain);
  EXPECT_EQ(WKErrorContentRuleListStoreCompileFailed, error.code);
  EXPECT_TRUE(CheckStoreForRuleListSync(key, /*expect_found=*/false));
}

// Tests that a compilation failure for one list does not affect other existing
// lists.
TEST_F(WKContentRuleListProviderTest, UpdateFailureDoesNotAffectExistingLists) {
  const std::string key = "valid_key";
  // 1. Create a valid list.
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));
  ASSERT_TRUE(CheckStoreForRuleListSync(key));

  // 2. Attempt to update the valid list with invalid JSON.
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());

  // 3. Verify the outcome.
  // The update should fail.
  ASSERT_NE(nil, future.Get());
  // The original valid list should still be present in the store.
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Static List Compilation Tests

// Tests that the JSON from CreateLocalBlockingJsonRuleList compiles.
TEST_F(WKContentRuleListProviderTest, StaticBlockLocalListCompiles) {
  const std::string key = "static_block_key";
  std::string rules =
      base::SysNSStringToUTF8(CreateLocalBlockingJsonRuleList());
  EXPECT_TRUE(UpdateRuleListSync(key, rules));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that the JSON from CreateMixedContentAutoUpgradeJsonRuleList compiles.
TEST_F(WKContentRuleListProviderTest, StaticMixedContentListCompiles) {
  const std::string key = "static_mixed_content_key";
  std::string rules =
      base::SysNSStringToUTF8(CreateMixedContentAutoUpgradeJsonRuleList());
  EXPECT_TRUE(UpdateRuleListSync(key, rules));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that multiple concurrent update requests all succeed.
TEST_F(WKContentRuleListProviderTest, MultipleConcurrentUpdatesSucceed) {
  const int kUpdateCount = 3;
  std::vector<TestFuture<NSError*>> futures(kUpdateCount);
  std::vector<std::string> keys;

  // Start all updates without waiting.
  for (int i = 0; i < kUpdateCount; ++i) {
    std::string key = "key_" + base::NumberToString(i);
    keys.push_back(key);
    tracked_keys_.insert(key);
    provider_->UpdateRuleList(key, kValidTestRuleListJson,
                              futures[i].GetCallback());
  }

  // Verify that all updates completed successfully.
  for (int i = 0; i < kUpdateCount; ++i) {
    EXPECT_EQ(nil, futures[i].Get());
    EXPECT_TRUE(CheckStoreForRuleListSync(keys[i]));
  }
}

// Tests that the provider can be safely destroyed while operations are pending.
// This is safe because of the weak pointer used in the completion handler.
TEST_F(WKContentRuleListProviderTest, ProviderDestroyedWithPendingOperations) {
  const std::string key = "pending_op_key";
  TestFuture<NSError*> future;
  // Start an update but don't wait for it.
  provider_->UpdateRuleList(key, kValidTestRuleListJson, future.GetCallback());
  // Track key for TearDown, in case the operation completes before the test
  // finishes.
  tracked_keys_.insert(key);

  // Destroy the provider immediately.
  provider_.reset();

  // The test passes if it does not crash. We can also verify that the callback
  // was never invoked.
  EXPECT_FALSE(future.IsReady());
}

// Tests that the idle callback fires correctly when set while the provider
// is already idle.
TEST_F(WKContentRuleListProviderTest, IdleCallbackFiresWhenSetWhileIdle) {
  TestFuture<void> idle_future;
  provider_->SetIdleCallbackForTesting(idle_future.GetRepeatingCallback());

  // The callback for an already-idle provider should fire. Wait for it.
  EXPECT_TRUE(idle_future.Wait());
}

// Tests that the idle callback fires correctly when the provider becomes idle.
TEST_F(WKContentRuleListProviderTest,
       IdleCallbackFiresWhenProviderBecomesIdle) {
  const std::string key = "idle_test_key";
  // Start an update but do not wait for it, ensuring the provider is busy.
  TestFuture<NSError*> update_future;
  provider_->UpdateRuleList(key, kValidTestRuleListJson,
                            update_future.GetCallback());
  tracked_keys_.insert(key);

  TestFuture<void> idle_future;
  provider_->SetIdleCallbackForTesting(idle_future.GetRepeatingCallback());

  // The provider is busy, so the idle callback should not have fired yet.
  EXPECT_FALSE(idle_future.IsReady());

  // Wait for the pending update to complete.
  ASSERT_EQ(nil, update_future.Get());

  // The provider should now be idle, and the callback should have been posted.
  // Wait for it to execute.
  EXPECT_TRUE(idle_future.Wait());
}

}  // namespace
}  // namespace web
