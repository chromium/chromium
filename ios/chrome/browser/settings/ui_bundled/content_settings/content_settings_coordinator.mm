// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/content_settings_coordinator.h"

#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/content_settings/content_settings_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/content_settings/default_page_mode_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/content_settings/web_inspector_state_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface ContentSettingsCoordinator () <
    BrowserObserving,
    ContentSettingsTableViewControllerPresentationDelegate>

@end

@implementation ContentSettingsCoordinator {
  ContentSettingsTableViewController* _viewController;

  // The coordinator showing the view to choose the defaultMode.
  DefaultPageModeCoordinator* _defaultModeViewCoordinator;

  // The coordinator showing the view to enable or disable Web Inspector.
  WebInspectorStateCoordinator* _webInspectorStateViewCoordinator;

  // Bridge for browser observation, to make sure any references are cut when
  // the browser is destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserverBridge;

  // Verifies that `stop` is always called before dealloc.
  BOOL _stopped;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _browserObserverBridge =
      std::make_unique<BrowserObserverBridge>(self.browser, self);

  HostContentSettingsMap* settingsMap =
      ios::HostContentSettingsMapFactory::GetForProfile(self.profile);
  MailtoHandlerService* mailtoHandlerService =
      MailtoHandlerServiceFactory::GetForProfile(self.profile);
  PrefService* prefService = self.profile->GetPrefs();

  _viewController = [[ContentSettingsTableViewController alloc]
      initWithHostContentSettingsMap:settingsMap
                mailtoHandlerService:mailtoHandlerService
                         prefService:prefService];
  _viewController.presentationDelegate = self;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _stopped = YES;
  [_defaultModeViewCoordinator stop];
  _defaultModeViewCoordinator = nil;

  [_webInspectorStateViewCoordinator stop];
  _webInspectorStateViewCoordinator = nil;

  [_viewController disconnect];
  _viewController = nil;
}

- (void)dealloc {
  // TODO(crbug.com/427791214): If stop is always called before dealloc, then
  // do all C++ cleanup in stop.
  CHECK(_stopped, base::NotFatalUntil::M150);
}

#pragma mark - ContentSettingsTableViewControllerPresentationDelegate

- (void)contentSettingsTableViewControllerWasRemoved:
    (ContentSettingsTableViewController*)controller {
  [self.delegate contentSettingsCoordinatorViewControllerWasRemoved:self];
}

- (void)contentSettingsTableViewControllerSelectedDefaultPageMode:
    (ContentSettingsTableViewController*)controller {
  _defaultModeViewCoordinator = [[DefaultPageModeCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser];
  [_defaultModeViewCoordinator start];
}

- (void)contentSettingsTableViewControllerSelectedWebInspector:
    (ContentSettingsTableViewController*)controller {
  [_webInspectorStateViewCoordinator stop];

  _webInspectorStateViewCoordinator = [[WebInspectorStateCoordinator alloc]
      initWithBaseNavigationController:_baseNavigationController
                               browser:self.browser];
  [_webInspectorStateViewCoordinator start];
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  [_viewController disconnect];
}

@end
