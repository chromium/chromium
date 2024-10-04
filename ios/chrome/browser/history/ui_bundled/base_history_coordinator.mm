// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "components/history/core/browser/browsing_history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_coordinator+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_view_controller.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_mediator.h"
#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver.h"
#import "ios/chrome/browser/history/ui_bundled/ios_browsing_history_driver_delegate_bridge.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"

namespace {

history::WebHistoryService* WebHistoryServiceGetter(
    base::WeakPtr<ProfileIOS> weak_profile) {
  DCHECK(weak_profile.get())
      << "Getter should not be called after ProfileIOS destruction.";
  return ios::WebHistoryServiceFactory::GetForProfile(weak_profile.get());
}

}  // anonymous namespace

@interface BaseHistoryCoordinator () {
  // Provides delegate bridge instance for `_browsingHistoryDriver`.
  std::unique_ptr<IOSBrowsingHistoryDriverDelegateBridge>
      _browsingHistoryDriverDelegate;
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> _browsingHistoryDriver;
  // Abstraction to communicate with HistoryService and WebHistoryService.
  std::unique_ptr<history::BrowsingHistoryService> _browsingHistoryService;
  // Observe BrowserObserver to prevent any access to Browser before its
  // destroyed.
  std::unique_ptr<BrowserObserverBridge> _browserObserver;
  // Mediator being managed by this Coordinator.
  HistoryMediator* _mediator;
  // Coordinator in charge of handling sharing use cases.
  SharingCoordinator* _sharingCoordinator;
}
@end

@implementation BaseHistoryCoordinator

- (void)start {
  // Initialize and configure BaseHistoryViewController.
  self.viewController.browser = self.browser;
  self.viewController.loadStrategy = self.loadStrategy;

  self.viewController.menuProvider = self;

  DCHECK(!_browserObserver);
  _browserObserver =
      std::make_unique<BrowserObserverBridge>(self.browser, self);

  // Initialize and set HistoryMediator.
  _mediator =
      [[HistoryMediator alloc] initWithProfile:self.browser->GetProfile()];
  self.viewController.imageDataSource = _mediator;

  // Initialize and configure HistoryServices.
  _browsingHistoryDriverDelegate =
      std::make_unique<IOSBrowsingHistoryDriverDelegateBridge>(
          self.viewController);
  _browsingHistoryDriver = std::make_unique<IOSBrowsingHistoryDriver>(
      base::BindRepeating(&WebHistoryServiceGetter,
                          self.browser->GetProfile()->AsWeakPtr()),
      _browsingHistoryDriverDelegate.get());
  _browsingHistoryService = std::make_unique<history::BrowsingHistoryService>(
      _browsingHistoryDriver.get(),
      ios::HistoryServiceFactory::GetForProfile(
          self.browser->GetProfile(), ServiceAccessType::EXPLICIT_ACCESS),
      SyncServiceFactory::GetForProfile(self.browser->GetProfile()));
  self.viewController.historyService = _browsingHistoryService.get();

  self.viewController.presentationDelegate = self.presentationDelegate;
}

- (void)stop {
  // `stop` is called as part of the UI teardown, which means that the browser
  // objects may be deleted before the dismiss animation is complete.
  // Disconnect the viewController before dismissing it to avoid
  // it accessing stalled objects.
  [self.viewController detachFromBrowser];
  [self dismissWithCompletion:nil];

  // Clear C++ objects as they may reference objects that will become
  // unavailable.
  _browsingHistoryDriver = nullptr;
  _browsingHistoryService = nullptr;
  _browsingHistoryDriverDelegate = nullptr;
}

- (void)dealloc {
  self.viewController.historyService = nullptr;
}

- (BaseHistoryViewController*)viewController {
  NOTREACHED() << "This should be implemented in subclasses.";
}

- (MenuScenarioHistogram)scenario {
  NOTREACHED() << "This should be implemented in subclasses.";
}

- (void)dismissWithCompletion:(ProceduralBlock)completionHandler {
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;

  if (_browserObserver) {
    _browserObserver.reset();
  }

  [self.viewController.contextMenuCoordinator stop];

  _browsingHistoryDriver = nullptr;
  _browsingHistoryService = nullptr;
  _browsingHistoryDriverDelegate = nullptr;
}

#pragma mark - HistoryTableViewControllerDelegate

- (void)dismissViewController:(BaseHistoryViewController*)controller
               withCompletion:(ProceduralBlock)completionHandler {
  [self.delegate closeHistoryWithCompletion:completionHandler];
}

- (void)dismissViewController:(BaseHistoryViewController*)controller {
  [self.delegate closeHistory];
}

#pragma mark - HistoryMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (HistoryEntryItem*)item
                                                      withView:(UIView*)view {
  __weak id<HistoryEntryItemDelegate> historyItemDelegate = self.viewController;
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }

    BaseHistoryCoordinator* strongSelf = weakSelf;

    // Record that this context menu was shown to the user.
    RecordMenuShown(self.scenario);

    BrowserActionFactory* actionFactory =
        [[BrowserActionFactory alloc] initWithBrowser:strongSelf.browser
                                             scenario:self.scenario];

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    [menuElements
        addObject:[actionFactory
                      actionToOpenInNewTabWithURL:item.URL
                                       completion:^{
                                         [weakSelf onOpenedURLInNewTab];
                                       }]];

    UIAction* incognitoAction = [actionFactory
        actionToOpenInNewIncognitoTabWithURL:item.URL
                                  completion:^{
                                    [weakSelf onOpenedURLInNewIncognitoTab];
                                  }];
    if (IsIncognitoModeDisabled(self.browser->GetProfile()->GetPrefs())) {
      // Disable the "Open in Incognito" option if the incognito mode is
      // disabled.
      incognitoAction.attributes = UIMenuElementAttributesDisabled;
    }
    [menuElements addObject:incognitoAction];

    if (base::ios::IsMultipleScenesSupported()) {
      [menuElements
          addObject:
              [actionFactory
                  actionToOpenInNewWindowWithURL:item.URL
                                  activityOrigin:WindowActivityHistoryOrigin]];
    }

    CrURL* URL = [[CrURL alloc] initWithGURL:item.URL];
    [menuElements addObject:[actionFactory actionToCopyURL:URL]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf shareURL:item.URL title:item.text fromView:view];
                  }]];

    [menuElements addObject:[actionFactory actionToDeleteWithBlock:^{
                    [historyItemDelegate historyEntryItemDidRequestDelete:item];
                  }]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  DCHECK_EQ(browser, self.browser);
  self.viewController.browser = nil;
}

// Stops the coordinator and requests the presentation delegate to transition to
// the active regular tab.
- (void)onOpenedURLInNewTab {
  __weak __typeof(self) weakSelf = self;
  [self.delegate closeHistoryWithCompletion:^{
    [weakSelf.presentationDelegate showActiveRegularTabFromHistory];
  }];
}

// Stops the coordinator and requests the presentation delegate to transition to
// the active incognito tab.
- (void)onOpenedURLInNewIncognitoTab {
  __weak __typeof(self) weakSelf = self;
  [self.delegate closeHistoryWithCompletion:^{
    [weakSelf.presentationDelegate showActiveIncognitoTabFromHistory];
  }];
}

#pragma mark - Private

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::HistoryEntry];
  _sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self.viewController
                                                     browser:self.browser
                                                      params:params
                                                  originView:view];
  [_sharingCoordinator start];
}

@end
