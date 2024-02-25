// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/external_app_util.h"

#import <UIKit/UIKit.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using GoogleDriveAppUtilTest = PlatformTest;

// Tests GetGoogleDriveAppUrl() function.
TEST_F(GoogleDriveAppUtilTest, GetGoogleDriveAppUrl) {
  NSURL* url = GetGoogleDriveAppUrl();
  ASSERT_TRUE(url);
  ASSERT_NSEQ(kGoogleDriveAppURLScheme, url.scheme);
}

// Tests IsGoogleDriveAppInstalled() function returning true.
TEST_F(GoogleDriveAppUtilTest, IsGoogleDriveAppInstalledTrue) {
  id application = OCMClassMock([UIApplication class]);
  OCMStub([application sharedApplication]).andReturn(application);

  OCMStub([application canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);
  EXPECT_TRUE(IsGoogleDriveAppInstalled());

  [application stopMocking];
}

// Tests IsGoogleDriveAppInstalled() function returning false.
TEST_F(GoogleDriveAppUtilTest, IsGoogleDriveAppInstalledFalse) {
  id application = OCMClassMock([UIApplication class]);
  OCMStub([application sharedApplication]).andReturn(application);

  OCMStub([application canOpenURL:GetGoogleDriveAppUrl()]).andReturn(NO);
  EXPECT_FALSE(IsGoogleDriveAppInstalled());

  [application stopMocking];
}
