// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

// URLs of the test pages.
const char kProfileForm[] = "/autofill_smoke_test.html";

}  // namepsace

@interface SaveProfileEGTest : ChromeTestCase

@end

@implementation SaveProfileEGTest

- (void)tearDown {
  // Clear existing profile.
  [AutofillAppInterface clearProfilesStore];

  [super tearDown];
}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitForm {
  [ChromeEarlGrey tapWebStateElementWithID:@"fill_profile_president"];
  [ChromeEarlGrey tapWebStateElementWithID:@"submit_profile"];
}

#pragma mark - Tests

// Ensures that the profile is saved to Chrome after submitting the form.
- (void)testUserData_LocalSave {
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kProfileForm)];

  // Ensure there are no saved profiles.
  GREYAssertEqual(0U, [AutofillAppInterface profilesCount],
                  @"There should be no saved profile.");

  // Shortcut explicit save prompts and automatically accept.
  [AutofillAppInterface setAutoAcceptAddressImports:YES];
  [self fillAndSubmitForm];

  // Ensure profile is saved locally.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  [AutofillAppInterface setAutoAcceptAddressImports:NO];
}

@end
