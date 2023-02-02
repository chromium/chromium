// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_coordinator.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "components/history/core/browser/browsing_history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_observer_bridge.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/history/history_clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/history/history_mediator.h"
#import "ios/chrome/browser/ui/history/history_menu_provider.h"
#import "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/history/history_ui_delegate.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver_delegate_bridge.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

history::WebHistoryService* WebHistoryServiceGetter(
    base::WeakPtr<ChromeBrowserState> weak_browser_state) {
  DCHECK(weak_browser_state.get())
      << "Getter should not be called after ChromeBrowserState destruction.";
  return ios::WebHistoryServiceFactory::GetForBrowserState(
      weak_browser_state.get());
}

}  // anonymous namespace

@interface HistoryCoordinator () <BrowserObserving,
                                  HistoryMenuProvider,
                                  HistoryUIDelegate> {
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
}
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableViewNavigationController* historyNavigationController;

@property(nonatomic, strong)
    HistoryTableViewController* historyTableViewController;

// Mediator being managed by this Coordinator.
@property(nonatomic, strong) HistoryMediator* mediator;

// The coordinator that will present Clear Browsing Data.
@property(nonatomic, strong)
    HistoryClearBrowsingDataCoordinator* historyClearBrowsingDataCoordinator;

// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
@end

@implementation HistoryCoordinator

- (void)start {
  // Initialize and configure HistoryTableViewController.
  self.historyTableViewController = [[HistoryTableViewController alloc] init];
  self.historyTableViewController.browser = self.browser;
  self.historyTableViewController.loadStrategy = self.loadStrategy;
  self.historyTableViewController.searchTerms = self.searchTerms;

  self.historyTableViewController.menuProvider = self;

  DCHECK(!_browserObserver);
  _browserObserver =
      std::make_unique<BrowserObserverBridge>(self.browser, self);

  // Initialize and set HistoryMediator
  self.mediator = [[HistoryMediator alloc]
      initWithBrowserState:self.browser->GetBrowserState()];
  self.historyTableViewController.imageDataSource = self.mediator;

  // Initialize and configure HistoryServices.
  _browsingHistoryDriverDelegate =
      std::make_unique<IOSBrowsingHistoryDriverDelegateBridge>(
          self.historyTableViewController);
  _browsingHistoryDriver = std::make_unique<IOSBrowsingHistoryDriver>(
      base::BindRepeating(&WebHistoryServiceGetter,
                          self.browser->GetBrowserState()->AsWeakPtr()),
      _browsingHistoryDriverDelegate.get());
  _browsingHistoryService = std::make_unique<history::BrowsingHistoryService>(
      _browsingHistoryDriver.get(),
      ios::HistoryServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS),
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState()));
  self.historyTableViewController.historyService =
      _browsingHistoryService.get();

  // Configure and present HistoryNavigationController.
  self.historyNavigationController = [[TableViewNavigationController alloc]
      initWithTable:self.historyTableViewController];
  self.historyNavigationController.toolbarHidden = NO;
  self.historyTableViewController.delegate = self;
  self.historyTableViewController.presentationDelegate =
      self.presentationDelegate;

  [self.historyNavigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.historyNavigationController.presentationController.delegate =
      self.historyTableViewController;

  [self.baseViewController
      presentViewController:self.historyNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

- (void)dealloc {
  self.historyTableViewController.historyService = nullptr;
}

// This method should always execute the `completionHandler`.
- (void)stopWithCompletion:(ProceduralBlock)completionHandler {
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  if (_browserObserver) {
    _browserObserver.reset();
  }

  if (self.historyNavigationController) {
    if (self.historyClearBrowsingDataCoordinator) {
      [self.historyClearBrowsingDataCoordinator stopWithCompletion:^{
        [self dismissHistoryNavigationWithCompletion:completionHandler];
      }];
    } else {
      [self dismissHistoryNavigationWithCompletion:completionHandler];
    }
  } else if (completionHandler) {
    completionHandler();
  }
}

- (void)dismissHistoryNavigationWithCompletion:(ProceduralBlock)completion {
  // Make sure to stop `self.historyTableViewController.contextMenuCoordinator`
  // before dismissing, or `self.historyNavigationController` will dismiss that
  // instead of itself.
  [self.historyTableViewController.contextMenuCoordinator stop];
  [self.historyNavigationController dismissViewControllerAnimated:YES
                                                       completion:completion];
  self.historyNavigationController = nil;
  self.historyClearBrowsingDataCoordinator = nil;
  self.historyTableViewController.historyService = nullptr;
  _browsingHistoryDriver = nullptr;
  _browsingHistoryService = nullptr;
  _browsingHistoryDriverDelegate = nullptr;
}

#pragma mark - HistoryUIDelegate

- (void)dismissHistoryWithCompletion:(ProceduralBlock)completionHandler {
  [self stopWithCompletion:completionHandler];
}

- (void)displayPrivacySettings {
  self.historyClearBrowsingDataCoordinator =
      [[HistoryClearBrowsingDataCoordinator alloc]
          initWithBaseViewController:self.historyNavigationController
                             browser:self.browser];
  self.historyClearBrowsingDataCoordinator.delegate = self;
  self.historyClearBrowsingDataCoordinator.presentationDelegate =
      self.presentationDelegate;
  self.historyClearBrowsingDataCoordinator.loadStrategy = self.loadStrategy;
  [self.historyClearBrowsingDataCoordinator start];
}

#pragma mark - HistoryMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (HistoryEntryItem*)item
                                                      withView:(UIView*)view {
  __weak id<HistoryEntryItemDelegate> historyItemDelegate =
      self.historyTableViewController;
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }

    HistoryCoordinator* strongSelf = weakSelf;

    // Record that this context menu was shown to the user.
    RecordMenuShown(MenuScenarioHistogram::kHistoryEntry);

    BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
        initWithBrowser:strongSelf.browser
               scenario:MenuScenarioHistogram::kHistoryEntry];

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
    if (IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs())) {
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

    [menuElements addObject:[actionFactory actionToCopyURL:item.URL]];

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
  self.historyTableViewController.browser = nil;
}

#pragma mark - Private

// Stops the coordinator and requests the presentation delegate to transition to
// the active regular tab.
- (void)onOpenedURLInNewTab {
  __weak __typeof(self) weakSelf = self;
  [self stopWithCompletion:^{
    [weakSelf.presentationDelegate showActiveRegularTabFromHistory];
  }];
}

// Stops the coordinator and requests the presentation delegate to transition to
// the active incognito tab.
- (void)onOpenedURLInNewIncognitoTab {
  __weak __typeof(self) weakSelf = self;
  [self stopWithCompletion:^{
    [weakSelf.presentationDelegate showActiveIncognitoTabFromHistory];
  }];
}

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::HistoryEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.historyTableViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

@end
