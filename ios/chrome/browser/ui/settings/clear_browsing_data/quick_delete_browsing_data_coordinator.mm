// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_coordinator.h"

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"

@interface QuickDeleteBrowsingDataCoordinator () <
    QuickDeleteBrowsingDataViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation QuickDeleteBrowsingDataCoordinator {
  QuickDeleteBrowsingDataViewController* _viewController;
  UINavigationController* _navigationController;
  QuickDeleteMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  CHECK(!browserState->IsOffTheRecord());

  BrowsingDataCounterWrapperProducer* producer =
      [[BrowsingDataCounterWrapperProducer alloc]
          initWithBrowserState:browserState];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserState(browserState);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForBrowserState(browserState);

  _mediator =
      [[QuickDeleteMediator alloc] initWithPrefs:browserState->GetPrefs()
              browsingDataCounterWrapperProducer:producer
                                 identityManager:identityManager
                             browsingDataRemover:browsingDataRemover
                             discoverFeedService:discoverFeedService];

  _viewController = [[QuickDeleteBrowsingDataViewController alloc] init];
  _viewController.delegate = self;
  _viewController.mutator = _mediator;

  _mediator.consumer = _viewController;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _viewController.delegate = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  _mediator.consumer = nil;
  _mediator = nil;
}

#pragma mark - QuickDeleteBrowsingDataViewControllerDelegate

- (void)dismissBrowsingDataPage {
  [self.delegate stopBrowsingDataPage];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissBrowsingDataPage];
}

@end
