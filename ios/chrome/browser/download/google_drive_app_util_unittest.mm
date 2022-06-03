// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/external_app_util.h"

#import <UIKit/UIKit.h>

#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
