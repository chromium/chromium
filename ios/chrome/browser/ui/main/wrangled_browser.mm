// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/wrangled_browser.h"

#import "base/check.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_coordinator.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface WrangledBrowser ()
@property(nonatomic, weak, readonly) BrowserCoordinator* coordinator;
@end

@implementation WrangledBrowser

#pragma mark - Public

@synthesize inactiveBrowser = _inactiveBrowser;

- (instancetype)initWithCoordinator:(BrowserCoordinator*)coordinator {
  if ((self = [super init])) {
    DCHECK(coordinator.browser);
    _coordinator = coordinator;
  }
  return self;
}

- (UIViewController*)viewController {
  return self.coordinator.viewController;
}

- (BrowserViewController*)bvc {
  return self.coordinator.viewController;
}

- (id<SyncPresenter>)syncPresenter {
  return self.coordinator;
}

- (Browser*)browser {
  return self.coordinator.browser;
}

- (ProfileIOS*)profile {
  return self.browser->GetProfile();
}

- (BOOL)incognito {
  return self.profile->IsOffTheRecord();
}

- (BOOL)playingTTS {
  return self.coordinator.playingTTS;
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [self.coordinator clearPresentedStateWithCompletion:completion
                                       dismissOmnibox:dismissOmnibox];
}

@end
