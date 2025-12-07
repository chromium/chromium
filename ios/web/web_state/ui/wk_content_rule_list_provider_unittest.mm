// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"

#import <WebKit/WebKit.h>

#import <set>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/web/web_state/ui/wk_content_rule_list_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::TestFuture;

namespace web {

// A valid, but minimal, content rule list for testing.
constexpr char kValidTestRuleListJson[] = R"([
  {
    "trigger": {"url-filter": ".*"},
    "action": {"type": "block"}
  }
])";
// An invalid JSON string for testing compilation failures.
constexpr char kInvalidTestRuleListJson[] = "this is not valid json";

class WKContentRuleListProviderTest : public PlatformTest {
 public:
  WKContentRuleListProviderTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    provider_ =
        std::make_unique<WKContentRuleListProvider>(temp_dir_.GetPath());
  }

 protected:
  // Helper to check for a rule list's presence in the store and wait.
  // Returns a gtest AssertionResult for use with EXPECT_TRUE.
  [[nodiscard]] testing::AssertionResult CheckStoreForRuleListSync(
      const std::string& identifier,
      bool expect_found = true) {
    TestFuture<WKContentRuleList*, NSError*> future;
    // Create a temporary store that points to the same path as the provider's
    // store to independently verify the file system state.
    WKContentRuleListStore* store = [WKContentRuleListStore
        storeWithURL:base::apple::FilePathToNSURL(temp_dir_.GetPath())];
    CHECK(store);

    void (^completion_block)(WKContentRuleList*, NSError*) =
        base::CallbackToBlock(future.GetCallback());
    [store
        lookUpContentRuleListForIdentifier:base::SysUTF8ToNSString(identifier)
                         completionHandler:completion_block];
    auto [rule_list, error] = future.Get();

    const bool was_found = (rule_list != nil);

    // First, handle any unexpected WebKit errors. This is always a failure.
    if (error && (![error.domain isEqualToString:WKErrorDomain] ||
                  error.code != WKErrorContentRuleListStoreLookUpFailed)) {
      return testing::AssertionFailure()
             << "Unexpected error looking up content rule list with identifier "
                "'"
             << identifier
             << "': " << base::SysNSStringToUTF8(error.description);
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

  // Helper to create or update a rule list and verify the operation's success.
  [[nodiscard]] testing::AssertionResult UpdateRuleListSync(
      const WKContentRuleListProvider::RuleListKey& key,
      const std::string& rules_list) {
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
  base::ScopedTempDir temp_dir_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<WKContentRuleListProvider> provider_;
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

// Tests UMA logging for successful creation and removal.
TEST_F(WKContentRuleListProviderTest, UMA_CreationAndRemovalSuccess) {
  const std::string key = "uma_test_key";
  ASSERT_TRUE(UpdateRuleListSync(key, kValidTestRuleListJson));

  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Compile.Success." + key, true, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.ContentRuleListProvider.Compile.Time." + key, 1);

  ASSERT_TRUE(RemoveRuleListSync(key));
  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Remove.Success." + key, true, 1);
}

// Tests UMA logging for a failed compilation.
TEST_F(WKContentRuleListProviderTest, UMA_CreationFailure) {
  const std::string key = "uma_test_key";
  TestFuture<NSError*> future;
  provider_->UpdateRuleList(key, kInvalidTestRuleListJson,
                            future.GetCallback());
  ASSERT_NE(nil, future.Get());

  histogram_tester_.ExpectUniqueSample(
      "IOS.ContentRuleListProvider.Compile.Success." + key, false, 1);
  histogram_tester_.ExpectTotalCount(
      "IOS.ContentRuleListProvider.Compile.Time." + key, 0);
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
  const std::string key = "BlockLocalResources";
  std::string rules =
      base::SysNSStringToUTF8(CreateLocalBlockingJsonRuleList());
  EXPECT_TRUE(UpdateRuleListSync(key, rules));
  EXPECT_TRUE(CheckStoreForRuleListSync(key));
}

// Tests that the JSON from CreateMixedContentAutoUpgradeJsonRuleList compiles.
TEST_F(WKContentRuleListProviderTest, StaticMixedContentListCompiles) {
  const std::string key = "MixedContentUpgrade";
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
    provider_->UpdateRuleList(key, kValidTestRuleListJson,
                              futures[i].GetCallback());
  }

  // Verify that all updates completed successfully.
  for (int i = 0; i < kUpdateCount; ++i) {
    EXPECT_EQ(nil, futures[i].Get());
    EXPECT_TRUE(CheckStoreForRuleListSync(keys[i]));
  }
}

}  // namespace web
