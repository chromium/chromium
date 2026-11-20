// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "base/check_op.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator+subclassing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@implementation ChromeCoordinator {
  base::WeakPtr<Browser> _browser;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  CHECK(browser);
  if ((self = [super init])) {
    _baseViewController = viewController;
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                     regularBrowserOfScene:(SceneState*)sceneState {
  Browser* browser = GetRegularBrowserForScene(sceneState);
  if ((self = [super init])) {
    _baseViewController = viewController;
    _browser = browser->AsWeakPtr();
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                            regularBrowser:(Browser*)browser {
  CHECK(browser);
  CHECK_EQ(browser->type(), Browser::Type::kRegular);
  if ((self = [super init])) {
    _baseViewController = viewController;
    _browser = browser->AsWeakPtr();
  }
  return self;
}

#pragma mark - Accessors

- (Browser*)browser {
  // `_browser` is only nullptr if the Browser was destroyed before this
  // coordinator was stopped and deallocated. Coordinators should not execute
  // code or access `browser` after `-stop`.
  CHECK(_browser.get(), base::NotFatalUntil::M155);
  return _browser.get();
}

- (ProfileIOS*)profile {
  ProfileIOS* profile = self.browser->GetProfile();
  // `profile` should always be non-nil for a live Browser. Coordinators should
  // not execute code or access `profile` after `-stop`.
  CHECK(profile, base::NotFatalUntil::M155);
  return profile;
}

- (BOOL)isOffTheRecord {
  CHECK(self.profile);
  return self.profile->IsOffTheRecord();
}

- (SceneState*)sceneState {
  if (!self.browser) {
    return nil;
  }
  return self.browser->GetSceneState();
}

#pragma mark - Public

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end
