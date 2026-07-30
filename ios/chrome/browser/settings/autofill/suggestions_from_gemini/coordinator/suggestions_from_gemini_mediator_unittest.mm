// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "components/prefs/testing_pref_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class SuggestionsFromGeminiMediatorTest : public PlatformTest {
 protected:
  SuggestionsFromGeminiMediatorTest() {
    mediator_ = [[SuggestionsFromGeminiMediator alloc]
        initWithPrefService:&pref_service_];
  }

  ~SuggestionsFromGeminiMediatorTest() override { [mediator_ disconnect]; }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  SuggestionsFromGeminiMediator* mediator_;
};

// Tests that the mediator is correctly initialized.
TEST_F(SuggestionsFromGeminiMediatorTest, TestInitialization) {
  EXPECT_NE(nil, mediator_);
}

}  // namespace
