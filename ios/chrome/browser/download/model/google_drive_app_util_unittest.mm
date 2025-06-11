// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/model/external_app_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using GoogleDriveAppUtilTest = PlatformTest;

// Tests GetGoogleDriveAppURL() function.
TEST_F(GoogleDriveAppUtilTest, GetGoogleDriveAppURL) {
  NSURL* url = GetGoogleDriveAppURL();
  ASSERT_TRUE(url);
  ASSERT_NSEQ(kGoogleDriveAppURLScheme, url.scheme);
}

// Tests IsGoogleDriveAppInstalled() function returning true.
TEST_F(GoogleDriveAppUtilTest, IsGoogleDriveAppInstalledTrue) {
  id application = OCMClassMock([UIApplication class]);
  OCMStub([application sharedApplication]).andReturn(application);

  OCMStub([application canOpenURL:GetGoogleDriveAppURL()]).andReturn(YES);
  EXPECT_TRUE(IsGoogleDriveAppInstalled());

  [application stopMocking];
}

// Tests IsGoogleDriveAppInstalled() function returning false.
TEST_F(GoogleDriveAppUtilTest, IsGoogleDriveAppInstalledFalse) {
  id application = OCMClassMock([UIApplication class]);
  OCMStub([application sharedApplication]).andReturn(application);

  OCMStub([application canOpenURL:GetGoogleDriveAppURL()]).andReturn(NO);
  EXPECT_FALSE(IsGoogleDriveAppInstalled());

  [application stopMocking];
}

// Tests GetGoogleMapsAppURL() function.
TEST_F(GoogleDriveAppUtilTest, GetGoogleMapsAppURL) {
  NSURL* url = GetGoogleMapsAppURL();
  ASSERT_TRUE(url);
  ASSERT_NSEQ(kGoogleMapsAppURLScheme, url.scheme);
}
