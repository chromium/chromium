// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/language/model/language_model_manager_factory.h"

#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
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

  std::unique_ptr<TestProfileIOS> state(TestProfileIOS::Builder().Build());
  const language::LanguageModelManager* const manager =
      LanguageModelManagerFactory::GetForProfile(state.get());
  EXPECT_THAT(manager, Not(IsNull()));

  ProfileIOS* const incognito = state->GetOffTheRecordProfile();
  ASSERT_THAT(incognito, Not(IsNull()));
  EXPECT_THAT(LanguageModelManagerFactory::GetForProfile(incognito),
              Eq(manager));
}
