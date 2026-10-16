// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"

#import <memory>
#import <utility>

#import "base/test/run_until.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace actor {
namespace {

constexpr int kPolicyEnabled = 0;
constexpr int kPolicyDisabled = 1;

class ActorEnterprisePolicyCheckerIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kGeminiActOnWebSettings, kPolicyEnabled);
    pref_service_.registry()->RegisterListPref(
        prefs::kGeminiActOnWebAllowedForURLs);
    pref_service_.registry()->RegisterListPref(
        prefs::kGeminiActOnWebBlockedForURLs);

    checker_ =
        std::make_unique<ActorEnterprisePolicyCheckerIOS>(&pref_service_);
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ActorEnterprisePolicyCheckerIOS> checker_;
};

// Tests that `CanActOnWeb()` returns true by default when the policy is
// enabled.
TEST_F(ActorEnterprisePolicyCheckerIOSTest, CanActOnWeb_DefaultEnabled) {
  EXPECT_TRUE(checker_->CanActOnWeb());
}

// Tests that `CanActOnWeb()` returns false when the default policy is disabled
// and the allowlist is empty.
TEST_F(ActorEnterprisePolicyCheckerIOSTest,
       CanActOnWeb_DisabledWithEmptyAllowlist) {
  pref_service_.SetInteger(prefs::kGeminiActOnWebSettings, kPolicyDisabled);
  EXPECT_FALSE(checker_->CanActOnWeb());
}

// Tests that `CanActOnWeb()` returns true when the default policy is disabled
// but the allowlist is non-empty.
TEST_F(ActorEnterprisePolicyCheckerIOSTest,
       CanActOnWeb_DisabledWithNonEmptyAllowlist) {
  pref_service_.SetInteger(prefs::kGeminiActOnWebSettings, kPolicyDisabled);
  pref_service_.SetList(prefs::kGeminiActOnWebAllowedForURLs,
                        base::ListValue().Append("allowed.example.com"));
  EXPECT_TRUE(base::test::RunUntil([&]() { return checker_->CanActOnWeb(); }));
}

// Tests that subscribers to `AddCanActOnWebChangedCallback()` are notified
// when `CanActOnWeb()` flips between true and false.
TEST_F(ActorEnterprisePolicyCheckerIOSTest,
       CanActOnWebChangedCallback_NotifiesOnChange) {
  base::test::TestFuture<bool> future;
  base::CallbackListSubscription subscription =
      checker_->AddCanActOnWebChangedCallback(future.GetRepeatingCallback());

  pref_service_.SetInteger(prefs::kGeminiActOnWebSettings, kPolicyDisabled);
  EXPECT_FALSE(future.Take());

  pref_service_.SetInteger(prefs::kGeminiActOnWebSettings, kPolicyEnabled);
  EXPECT_TRUE(future.Take());
}

// Tests that `Evaluate()` returns `kNotBlocked` by default for an unlisted URL.
TEST_F(ActorEnterprisePolicyCheckerIOSTest, Evaluate_DefaultNotBlocked) {
  EXPECT_EQ(checker_->Evaluate(GURL("https://example.com/page")),
            EnterprisePolicyUrlBlockReason::kNotBlocked);
}

// Tests that `Evaluate()` respects the URL blocklist and allowlist, and gives
// the allowlist precedence when a URL matches both.
TEST_F(ActorEnterprisePolicyCheckerIOSTest, Evaluate_BlocklistAndAllowlist) {
  pref_service_.SetList(prefs::kGeminiActOnWebBlockedForURLs,
                        base::ListValue().Append("blocked.example.com"));
  pref_service_.SetList(
      prefs::kGeminiActOnWebAllowedForURLs,
      base::ListValue().Append("allowed.blocked.example.com"));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return checker_->Evaluate(GURL("https://blocked.example.com/path")) ==
           EnterprisePolicyUrlBlockReason::kExplicitlyBlocked;
  }));
  EXPECT_EQ(
      checker_->Evaluate(GURL("https://allowed.blocked.example.com/path")),
      EnterprisePolicyUrlBlockReason::kExplicitlyAllowed);
  EXPECT_EQ(checker_->Evaluate(GURL("https://other.example.com/path")),
            EnterprisePolicyUrlBlockReason::kNotBlocked);
}

// Tests that when `GeminiActOnWebSettings` is disabled, `Evaluate()` blocks
// unlisted URLs while still allowing URLs on the allowlist.
TEST_F(ActorEnterprisePolicyCheckerIOSTest,
       Evaluate_DisableByDefaultAllowsOnlyAllowListedUrls) {
  pref_service_.SetInteger(prefs::kGeminiActOnWebSettings, kPolicyDisabled);
  pref_service_.SetList(prefs::kGeminiActOnWebAllowedForURLs,
                        base::ListValue().Append("allowed.example.com"));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return checker_->Evaluate(GURL("https://allowed.example.com/path")) ==
           EnterprisePolicyUrlBlockReason::kExplicitlyAllowed;
  }));

  EXPECT_EQ(checker_->Evaluate(GURL("https://unlisted.example.com/path")),
            EnterprisePolicyUrlBlockReason::kExplicitlyBlocked);
}

}  // namespace
}  // namespace actor
