// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
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
    _childCoordinators = [MutableCoordinatorArray array];
    _browser = browser->AsWeakPtr();
  }
  return self;
}

#pragma mark - Accessors

- (ChromeCoordinator*)activeChildCoordinator {
  // By default the active child is the one most recently added to the child
  // array, but subclasses can override this.
  return self.childCoordinators.lastObject;
}

- (Browser*)browser {
  // Browser can only be nil after -stop. Coordinators should typically not
  // execute any code after this point, and definitely should not refer to
  // browser.
  CHECK(_browser.get(), base::NotFatalUntil::M147);
  return _browser.get();
}

- (ProfileIOS*)profile {
  ProfileIOS* profile = self.browser->GetProfile();
  // Profile can only be nil after -stop. Coordinators should typically not
  // execute any code after this point, and definitely should not refer to
  // profile.
  CHECK(profile, base::NotFatalUntil::M147);
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
