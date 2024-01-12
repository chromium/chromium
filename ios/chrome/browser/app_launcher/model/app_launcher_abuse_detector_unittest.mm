// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_abuse_detector.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using AppLauncherAbuseDetectorTest = PlatformTest;

// Tests cases when the same app is launched repeatedly from same source.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_SameAppSameSource) {
  const GURL kSourceUrl1("http://www.google.com");

  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+154")
                            fromSourcePageURL:kSourceUrl1]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+1354")
                              fromSourcePageURL:kSourceUrl1];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+12354")
                            fromSourcePageURL:kSourceUrl1]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+154")
                              fromSourcePageURL:kSourceUrl1];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:GURL("facetime://+13454")
                            fromSourcePageURL:kSourceUrl1]);

  [abuseDetector didRequestLaunchExternalAppURL:GURL("facetime://+14")
                              fromSourcePageURL:kSourceUrl1];
  // App was launched more than the max allowed times, the policy should change
  // to Prompt.
  EXPECT_EQ(ExternalAppLaunchPolicyPrompt,
            [abuseDetector launchPolicyForURL:GURL("facetime://+14")
                            fromSourcePageURL:kSourceUrl1]);
}

// Tests cases when same app is launched repeatedly from different sources.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_SameAppDifferentSources) {
  const GURL kSourceUrl1("http://www.google.com");
  const GURL kSourceUrl2("http://www.goog.com");
  const GURL kSourceUrl3("http://www.goog.ab");
  const GURL kSourceUrl4("http://www.foo.com");
  const GURL kAppUrl1("facetime://+1354");

  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl1
                              fromSourcePageURL:kSourceUrl1];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);

  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl1
                              fromSourcePageURL:kSourceUrl2];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl2]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl1
                              fromSourcePageURL:kSourceUrl3];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl3]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl1
                              fromSourcePageURL:kSourceUrl4];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl4]);
}

// Tests cases when different apps are launched from different sources.
TEST_F(AppLauncherAbuseDetectorTest,
       TestRepeatedAppLaunches_DifferentAppsDifferentSources) {
  const GURL kSourceUrl1("http://www.google.com");
  const GURL kSourceUrl2("http://www.goog.com");
  const GURL kSourceUrl3("http://www.goog.ab");
  const GURL kSourceUrl4("http://www.foo.com");
  const GURL kAppUrl1("facetime://+1354");
  const GURL kAppUrl2("facetime-audio://+1234");
  const GURL kAppUrl3("abc://abc");
  const GURL kAppUrl4("chrome://www.google.com");

  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl1
                              fromSourcePageURL:kSourceUrl1];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);

  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl2
                              fromSourcePageURL:kSourceUrl2];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl2
                            fromSourcePageURL:kSourceUrl2]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl3
                              fromSourcePageURL:kSourceUrl3];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl3
                            fromSourcePageURL:kSourceUrl3]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl4
                              fromSourcePageURL:kSourceUrl4];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl4
                            fromSourcePageURL:kSourceUrl4]);
}

// Tests blocking App launch only when the app have been allowed through the
// abuse detector before.
TEST_F(AppLauncherAbuseDetectorTest, TestBlockLaunchingApp) {
  const GURL kSourceUrl1("http://www.google.com");
  const GURL kSourceUrl2("http://www.goog.com");
  const GURL kAppUrl1("facetime://+1354");
  const GURL kAppUrl2("facetime-audio://+1234");

  AppLauncherAbuseDetector* abuseDetector =
      [[AppLauncherAbuseDetector alloc] init];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);
  // Don't block for apps that have not been registered.
  [abuseDetector blockLaunchingAppURL:kAppUrl1 fromSourcePageURL:kSourceUrl1];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl1
                            fromSourcePageURL:kSourceUrl1]);

  // Block for apps that have been registered
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl2
                            fromSourcePageURL:kSourceUrl2]);
  [abuseDetector didRequestLaunchExternalAppURL:kAppUrl2
                              fromSourcePageURL:kSourceUrl2];
  EXPECT_EQ(ExternalAppLaunchPolicyAllow,
            [abuseDetector launchPolicyForURL:kAppUrl2
                            fromSourcePageURL:kSourceUrl2]);
  [abuseDetector blockLaunchingAppURL:kAppUrl2 fromSourcePageURL:kSourceUrl2];
  EXPECT_EQ(ExternalAppLaunchPolicyBlock,
            [abuseDetector launchPolicyForURL:kAppUrl2
                            fromSourcePageURL:kSourceUrl2]);
}
