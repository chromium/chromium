// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/multitasking_test_scene_delegate.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/chrome_overlay_window.h"

namespace {

// These command line switches enable slide over or split view test mode for
// multitasking tests. Only one of them should be enabled at any time.
NSString* const kEnableSlideOverTestMode = @"--enable-slide-over-test-mode";
NSString* const kEnableSplitViewTestMode = @"--enable-split-view-test-mode";

// Screen size of various iPad models in terms of logical points. All models
// have regular size except for 12.9 inch iPad Pro, which is slightly larger.
const CGSize kRegularIPadPortraitSize = CGSizeMake(768.0, 1024.0);
const CGSize kLargeIPadPortraitSize = CGSizeMake(1024.0, 1366.0);

// Width of the application window size while in portrait slide over mode or
// landscape half-screen split view mode. These values are obtained by running
// application in actual portrait slide over mode and landscape half-screen
// split view mode.
const CGFloat kWidthPortraitSlideOverOnRegularIPad = 320.0;
const CGFloat kWidthPortraitSlideOverOnLargeIPad = 375.0;
const CGFloat kWidthLandscapeSplitViewOnRegularIPad = 507.0;
const CGFloat kWidthLandscapeSplitViewOnLargeIPad = 678.0;

}  // namespace

@implementation MultitaskingTestSceneDelegate

- (UIWindow*)window {
  UIWindow* window = [super window];
  // Adjust window size for multitasking tests.
  CGSize newWindowSize = [self windowSize];
  window.frame = CGRectMake(0, 0, newWindowSize.width, newWindowSize.height);
  return window;
}

#pragma mark - helpers

// Returns true if test is running on 12.9 inch iPad Pro. Otherwise, it's
// running on regular iPad.
- (BOOL)isRunningOnLargeIPadPro {
  CGSize size = [[UIScreen mainScreen] bounds].size;
  return MAX(size.height, size.width) ==
         MAX(kLargeIPadPortraitSize.width, kLargeIPadPortraitSize.height);
}

- (BOOL)IsRunningInSlideOverTestMode {
  return [[[NSProcessInfo processInfo] arguments]
      containsObject:kEnableSlideOverTestMode];
}

- (BOOL)IsRunningInSplitViewTestMode {
  return [[[NSProcessInfo processInfo] arguments]
      containsObject:kEnableSplitViewTestMode];
}

// Returns the size that will be used to configure the application window for
// multitasking tests. Both width and height are determined by the target name
// and on which iPad model it is running.
- (CGSize)windowSize {
  CGSize size;
  if ([self IsRunningInSlideOverTestMode]) {
    if ([self isRunningOnLargeIPadPro]) {
      size.width = kWidthPortraitSlideOverOnLargeIPad;
      size.height = kLargeIPadPortraitSize.height;
    } else {
      size.width = kWidthPortraitSlideOverOnRegularIPad;
      size.height = kRegularIPadPortraitSize.height;
    }
  } else if ([self IsRunningInSplitViewTestMode]) {
    if ([self isRunningOnLargeIPadPro]) {
      size.width = kWidthLandscapeSplitViewOnLargeIPad;
      size.height = kLargeIPadPortraitSize.width;
    } else {
      size.width = kWidthLandscapeSplitViewOnRegularIPad;
      size.height = kRegularIPadPortraitSize.width;
    }
  } else {
    NOTREACHED_IN_MIGRATION() << "Unsupported multitasking test mode. Only "
                                 "--enable-slide-over-test-mode and "
                                 "--enable-split-view-test-mode are supported.";
  }
  return size;
}

@end
