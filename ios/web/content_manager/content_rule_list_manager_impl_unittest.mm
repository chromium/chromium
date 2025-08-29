// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content_manager/content_rule_list_manager_impl.h"

#import <WebKit/WebKit.h>

#import <optional>
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/web/public/content_manager/content_rule_list_manager.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::TestFuture;

namespace web {
namespace {

// Bring the type alias into the current scope for convenience.
using RuleListKey = ContentRuleListManager::RuleListKey;

// A valid JSON string for a content rule list.
constexpr char kValidTestRuleListJson[] = R"([
  {
    "trigger": {"url-filter": ".*"},
    "action": {"type": "block"}
  }
])";

// An invalid JSON string for a content rule list.
constexpr char kInvalidTestRuleListJson[] = "this is not valid json";

// A test key for a content rule list.
const RuleListKey kTestRuleListKey = "test_key";

// Helper to check for a rule list's presence in the store and wait.
// Returns a gtest AssertionResult for use with EXPECT_TRUE.
[[nodiscard]] testing::AssertionResult CheckStoreForRuleListSync(
    const RuleListKey& rule_list_key,
    bool expect_found = true) {
  TestFuture<WKContentRuleList*, NSError*> future;
  void (^completion_block)(WKContentRuleList*, NSError*) =
      base::CallbackToBlock(future.GetCallback());
  [WKContentRuleListStore.defaultStore
      lookUpContentRuleListForIdentifier:base::SysUTF8ToNSString(rule_list_key)
                       completionHandler:completion_block];
  auto [rule_list, error] = future.Get();

  const bool was_found = (rule_list != nil);

  // First, handle any unexpected WebKit errors. This is always a failure.
  // WKErrorContentRuleListStoreLookUpFailed are ignored because that is the
  // expected error when a rule list is not found.
  if (error && (![error.domain isEqualToString:WKErrorDomain] ||
                error.code != WKErrorContentRuleListStoreLookUpFailed)) {
    return testing::AssertionFailure()
           << "Unexpected error looking up content rule list with key '"
           << rule_list_key << "': " << base::SysNSStringToUTF8(error.description);
  }

  // Now, check if the outcome matches the expectation.
  if (was_found == expect_found) {
    return testing::AssertionSuccess();
  }

  // The outcome did not match the expectation, return a specific failure.
  if (expect_found) {
    return testing::AssertionFailure()
           << "Expected to find content rule list with key '"
           << rule_list_key << "', but it was not found.";
  } else {
    return testing::AssertionFailure()
           << "Expected content rule list with key '" << rule_list_key
           << "' to be absent, but it was found.";
  }
}

}  // namespace

class ContentRuleListManagerImplTest : public PlatformTest {
 protected:
  void TearDown() override {
    // Ensure any test lists are cleared from the store to prevent leakage.
    // Use a test-specific key to avoid interfering with other tests.
    TestFuture<NSError*> future;
    [WKContentRuleListStore.defaultStore
        removeContentRuleListForIdentifier:base::SysUTF8ToNSString(
                                               kTestRuleListKey)
                         completionHandler:base::CallbackToBlock(
                                               future.GetCallback())];
    std::ignore = future.Wait();
    PlatformTest::TearDown();
  }

  // Returns the ContentRuleListManager associated with `browser_state_`.
  ContentRuleListManager& GetManager() {
    return ContentRuleListManager::FromBrowserState(&browser_state_);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  FakeBrowserState browser_state_;
};

// Tests that the manager can be created and retrieved correctly.
TEST_F(ContentRuleListManagerImplTest, GetInstance) {
  ContentRuleListManager& manager =
      ContentRuleListManager::FromBrowserState(&browser_state_);
  // Verify that for a given browser state, the same manager is returned.
  EXPECT_EQ(&manager,
            &ContentRuleListManager::FromBrowserState(&browser_state_));

  FakeBrowserState other_browser_state;
  EXPECT_NE(&manager,
            &ContentRuleListManager::FromBrowserState(&other_browser_state));
}

// Tests registering a valid rule list for the first time.
TEST_F(ContentRuleListManagerImplTest, RegisterValidRules) {
  // The rule list should not be in the store yet.
  ASSERT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/false));

  TestFuture<NSError*> future;
  GetManager().UpdateRuleList(kTestRuleListKey, kValidTestRuleListJson,
                              future.GetCallback());

  EXPECT_EQ(nil, future.Get());
  EXPECT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/true));
}

// Tests attempting to register an invalid rule list.
TEST_F(ContentRuleListManagerImplTest, RegisterInvalidRules) {
  ASSERT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/false));

  TestFuture<NSError*> future;
  GetManager().UpdateRuleList(kTestRuleListKey, kInvalidTestRuleListJson,
                              future.GetCallback());

  NSError* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_NSEQ(WKErrorDomain, result.domain);
  EXPECT_EQ(WKErrorContentRuleListStoreCompileFailed, result.code);
  EXPECT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/false));
}

// Tests removing a rule list that has been previously added.
TEST_F(ContentRuleListManagerImplTest, RemoveExistingRules) {
  TestFuture<NSError*> add_future;
  GetManager().UpdateRuleList(kTestRuleListKey, kValidTestRuleListJson,
                              add_future.GetCallback());
  ASSERT_EQ(nil, add_future.Get());
  ASSERT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/true));

  TestFuture<NSError*> remove_future;
  GetManager().RemoveRuleList(kTestRuleListKey, remove_future.GetCallback());

  EXPECT_EQ(nil, remove_future.Get());
  EXPECT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/false));
}

// Tests removing a rule list when none exists.
TEST_F(ContentRuleListManagerImplTest, RemoveNonExistentRules) {
  ASSERT_TRUE(CheckStoreForRuleListSync(kTestRuleListKey,
                                        /*expect_found=*/false));

  TestFuture<NSError*> future;
  GetManager().RemoveRuleList(kTestRuleListKey, future.GetCallback());
  EXPECT_EQ(nil, future.Get());
}

}  // namespace web