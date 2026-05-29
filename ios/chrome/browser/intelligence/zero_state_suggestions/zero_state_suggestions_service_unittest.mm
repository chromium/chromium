// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ai {

class ZeroStateSuggestionsServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    fake_web_state_ = std::make_unique<web::FakeWebState>();
    fake_web_state_->SetBrowserState(profile_.get());
    fake_web_state_->SetVisibleURL(GURL("https://example.com"));

    service_ =
        std::make_unique<ZeroStateSuggestionsService>(fake_web_state_.get());
  }

  void SetCachedSuggestions(const std::vector<std::string>& suggestions) {
    service_->SetCanApply(true);
    service_->suggestions_ = suggestions;
    service_->suggestions_url_ = fake_web_state_->GetVisibleURL();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  std::unique_ptr<ZeroStateSuggestionsService> service_;
};

// Tests that FetchZeroStateSuggestions returns nil if can_apply is false.
TEST_F(ZeroStateSuggestionsServiceTest,
       FetchSuggestionsReturnsNilWhenCanApplyIsFalse) {
  service_->SetCanApply(false);

  base::test::TestFuture<NSArray<NSString*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_NSEQ(nil, future.Get());
}

// Tests that populated cache correctly returns suggestions.
TEST_F(ZeroStateSuggestionsServiceTest, TestFetchCachedSuggestions) {
  std::vector<std::string> suggestions = {"suggestion1", "suggestion2"};
  SetCachedSuggestions(suggestions);

  EXPECT_TRUE(service_->CanApply());

  base::test::TestFuture<NSArray<NSString*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  NSArray<NSString*>* result = future.Get();
  ASSERT_NE(nil, result);
  EXPECT_EQ(2u, result.count);
  EXPECT_NSEQ(@"suggestion1", result[0]);
  EXPECT_NSEQ(@"suggestion2", result[1]);
}

// Tests that ClearCachedSuggestions clears the cached suggestions and
// can_apply.
TEST_F(ZeroStateSuggestionsServiceTest, TestClearCachedSuggestions) {
  std::vector<std::string> suggestions = {"suggestion1"};
  SetCachedSuggestions(suggestions);
  EXPECT_TRUE(service_->CanApply());

  service_->ClearCachedSuggestions();
  EXPECT_FALSE(service_->CanApply());

  base::test::TestFuture<NSArray<NSString*>*> future;
  service_->FetchZeroStateSuggestions(future.GetCallback());

  EXPECT_NSEQ(nil, future.Get());
}

}  // namespace ai
