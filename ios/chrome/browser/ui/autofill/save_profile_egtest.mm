// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using chrome_test_util::TapWebViewElementWithId;

// URLs of the test pages.
const char kProfileForm[] =
    "http://ios/testing/data/http_server_files/autofill_smoke_test.html";

}  // namepsace

@interface SaveProfileEGTest : ChromeTestCase {
  autofill::PersonalDataManager* personal_data_manager_;
}

@end

@implementation SaveProfileEGTest

- (void)setUp {
  [super setUp];

  personal_data_manager_ =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
}

- (void)tearDown {
  // Clear existing profile.
  for (const auto* profile : personal_data_manager_->GetProfiles()) {
    personal_data_manager_->RemoveByGUID(profile->guid());
  }

  [super tearDown];
}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitForm {
  GREYAssert(TapWebViewElementWithId("fill_profile_president"),
             @"Failed to tap \"fill_profile_president\"");
  GREYAssert(TapWebViewElementWithId("submit_profile"),
             @"Failed to tap \"submit_profile\"");
}

#pragma mark - Tests

// Ensures that the profile is saved to Chrome after submitting the form.
- (void)testUserData_LocalSave {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, personal_data_manager_->GetProfiles().size(),
                  @"There should be no saved profile.");

  [self fillAndSubmitForm];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, personal_data_manager_->GetProfiles().size(),
                  @"Profile should have been saved.");
}

@end
