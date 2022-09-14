// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/main_application_delegate.h"

#import <Foundation/Foundation.h>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using MainApplicationDelegateTest = PlatformTest;

// Tests that the application does not crash if `applicationWillTerminate:` is
// called before a previous call to `application:didFinishLaunchingWithOptions:`
// set up the ChromeBrowserProvider. This can happen if the app is force-quit
// while the splash screen is still visible.
TEST_F(MainApplicationDelegateTest, TerminateCalledWithNoBrowserProvider) {
  id application = [OCMockObject niceMockForClass:[UIApplication class]];

  // The test fixture automatically registers a ChromeBrowserProvider, but this
  // test is trying to verify behavior in the case where
  // ios::GetChromeBrowserProvider() return nullptr. Clear the previously-set
  // provider before proceeding.
  ios::ChromeBrowserProvider* stashed_chrome_browser_provider =
      ios::SetChromeBrowserProvider(nullptr);

  MainApplicationDelegate* delegate = [[MainApplicationDelegate alloc] init];
  [delegate applicationWillTerminate:application];

  // Restore ChromeBrowserProvider to its original value.
  ios::ChromeBrowserProvider* registered_provider =
      ios::SetChromeBrowserProvider(stashed_chrome_browser_provider);

  EXPECT_FALSE(registered_provider);
}
