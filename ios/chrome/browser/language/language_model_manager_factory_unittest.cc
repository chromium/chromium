// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/language_model_manager_factory.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using testing::Eq;
using testing::IsNull;
using testing::Not;

using LanguageModelManagerFactoryTest = PlatformTest;

// Check that Incognito language modeling is inherited from the user's profile.
TEST_F(LanguageModelManagerFactoryTest, SharedWithIncognito) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<TestChromeBrowserState> state(
      TestChromeBrowserState::Builder().Build());
  const language::LanguageModelManager* const manager =
      LanguageModelManagerFactory::GetForBrowserState(state.get());
  EXPECT_THAT(manager, Not(IsNull()));

  ChromeBrowserState* const incognito =
      state->GetOffTheRecordChromeBrowserState();
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(LanguageModelManagerFactory::GetForBrowserState(incognito),
              Eq(manager));
}
