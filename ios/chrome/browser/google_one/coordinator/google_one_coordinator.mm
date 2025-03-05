// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/coordinator/google_one_coordinator.h"

#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation GoogleOneCoordinator {
  GoogleOneEntryPoint _entryPoint;
  id<GoogleOneController> _controller;
  id<SystemIdentity> _identity;
  // UI blocker used while the there is only one buying flow at the time on
  // any window.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
  // Whether the internal controller has been stopped.
  BOOL _controllerStopped;
  // Whether this coordinator has been stopped, either from the controller or
  // by an external source.
  BOOL _stopped;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  identity:(id<SystemIdentity>)identity {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
    CHECK(identity);
    _identity = identity;
  }
  return self;
}

- (void)start {
  [super start];
  GoogleOneConfiguration* configuration = [[GoogleOneConfiguration alloc] init];
  configuration.entryPoint = _entryPoint;
  configuration.identity = _identity;
  __weak __typeof(self) weakSelf = self;
  configuration.openURLCallback = ^(NSURL* url) {
    [weakSelf openURL:url];
  };
  // There can be only one purchase flow in the application.
  _UIBlocker = std::make_unique<ScopedUIBlocker>(self.browser->GetSceneState(),
                                                 UIBlockerExtent::kApplication);
  _controller = ios::provider::CreateGoogleOneController(configuration);
  [_controller launchWithViewController:self.baseViewController
                             completion:^(NSError* error) {
                               [weakSelf flowDidCompleteWithError:error];
                             }];
}

- (void)stop {
  if (_stopped) {
    return;
  }
  _stopped = YES;
  if (!_controllerStopped) {
    [_controller stop];
  }
  _UIBlocker.reset();
  [super stop];
}

#pragma mark - Private

- (void)openURL:(NSURL*)url {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:net::GURLWithNSURL(url)
                   inIncognito:browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

- (void)flowDidCompleteWithError:(NSError*)error {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }
  _controllerStopped = YES;
  // TODO(crbug.com/388443644): handle error.
  if (!_stopped) {
    [HandlerForProtocol(browser->GetCommandDispatcher(), GoogleOneCommands)
        hideGoogleOne];
  }
}

@end
