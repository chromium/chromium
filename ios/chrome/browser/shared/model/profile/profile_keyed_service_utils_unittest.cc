// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_utils.h"

#import "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class ProfileKeyedServiceUtilsTest : public PlatformTest {
 public:
  ProfileKeyedServiceUtilsTest() {
    test_profile_ = TestProfileIOS::Builder().Build();
    test_profile_->CreateOffTheRecordBrowserStateWithTestingFactories();
  }

  ProfileIOS* GetRegularProfile() { return test_profile_.get(); }

  ProfileIOS* GetOffTheRecordProfile() {
    return test_profile_->GetOffTheRecordProfile();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> test_profile_;
};

// Tests that GetContextToUseForKeyedServiceFactory(...) returns the correct
// profile according to the context and the ProfileSelection.
TEST_F(ProfileKeyedServiceUtilsTest, GetContextToUseForKeyedServiceFactory) {
  struct TestCase {
    raw_ptr<web::BrowserState> context;
    raw_ptr<web::BrowserState> expects;
    ProfileSelection profile_selection;
  };

  const TestCase kTestCases[] = {
      // If the context is nullptr, the returned value must be nullptr.
      {
          .context = nullptr,
          .expects = nullptr,
          .profile_selection = ProfileSelection::kNoInstanceInIncognito,
      },
      {
          .context = nullptr,
          .expects = nullptr,
          .profile_selection = ProfileSelection::kRedirectedInIncognito,
      },
      {
          .context = nullptr,
          .expects = nullptr,
          .profile_selection = ProfileSelection::kOwnInstanceInIncognito,
      },

      // If the context is a regular profile, the returned value must be the
      // profile itself.
      {
          .context = GetRegularProfile(),
          .expects = GetRegularProfile(),
          .profile_selection = ProfileSelection::kNoInstanceInIncognito,
      },
      {
          .context = GetRegularProfile(),
          .expects = GetRegularProfile(),
          .profile_selection = ProfileSelection::kRedirectedInIncognito,
      },
      {
          .context = GetRegularProfile(),
          .expects = GetRegularProfile(),
          .profile_selection = ProfileSelection::kOwnInstanceInIncognito,
      },

      // If the context is an incognito profile, the returned value depends on
      // the ProfileSelection.
      {
          .context = GetOffTheRecordProfile(),
          .expects = nullptr,
          .profile_selection = ProfileSelection::kNoInstanceInIncognito,
      },
      {
          .context = GetOffTheRecordProfile(),
          .expects = GetRegularProfile(),
          .profile_selection = ProfileSelection::kRedirectedInIncognito,
      },
      {
          .context = GetOffTheRecordProfile(),
          .expects = GetOffTheRecordProfile(),
          .profile_selection = ProfileSelection::kOwnInstanceInIncognito,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    web::BrowserState* const result = GetContextToUseForKeyedServiceFactory(
        test_case.context, test_case.profile_selection);

    EXPECT_EQ(result, test_case.expects);
  }
}
