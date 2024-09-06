// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator.h"

#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

@interface TabGroupIndicatorCoordinator () <
    TabGroupIndicatorCoordinatorDelegate>
@end

@implementation TabGroupIndicatorCoordinator {
  TabGroupIndicatorMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(IsTabGroupIndicatorEnabled());
  CHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  _view = [[TabGroupIndicatorView alloc] init];
  _view.incognito = self.browser->GetBrowserState()->IsOffTheRecord();

  tab_groups::TabGroupSyncService* tabGroupSyncService =
      tab_groups::TabGroupSyncServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _mediator = [[TabGroupIndicatorMediator alloc]
          initWithProfile:self.browser->GetProfile()
      tabGroupSyncService:tabGroupSyncService
                 consumer:_view
             webStateList:self.browser->GetWebStateList()];
  _mediator.delegate = self;
  _view.mutator = _mediator;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - TabGroupIndicatorCoordinatorDelegate

- (void)showTabGroupIndicatorEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup {
  // TODO(crbug.com/361499394): Implement this.
}

- (void)showTabGroupIndicatorConfirmationForAction:
    (TabGroupActionType)actionType {
  // TODO(crbug.com/361499394): Implement this.
}

- (void)showTabGroupIndicatorSnackbarAfterClosingGroup {
  // TODO(crbug.com/361499394): Implement this.
}

@end
