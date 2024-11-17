// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_coordinator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_view_controller_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_transition_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_view_controller.h"
#import "ios/web/public/web_state_id.h"

@interface CreateTabGroupCoordinator () <
    CreateOrEditTabGroupViewControllerDelegate,
    CreateTabGroupMediatorDelegate>
@end

@implementation CreateTabGroupCoordinator {
  // Mediator for tab groups creation.
  CreateTabGroupMediator* _mediator;
  // View controller for tab groups creation.
  CreateTabGroupViewController* _viewController;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Tab group to edit.
  raw_ptr<const TabGroup> _tabGroup;
  // Transition delegate for the animation to show/hide.
  CreateTabGroupTransitionDelegate* _transitionDelegate;
}

#pragma mark - Public

- (instancetype)
    initTabGroupCreationWithBaseViewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                                  selectedTabs:
                                      (const std::set<web::WebStateID>&)
                                          identifiers {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _identifiers = identifiers;
    _animatedDismissal = YES;
  }
  return self;
}

- (instancetype)
    initTabGroupEditionWithBaseViewController:(UIViewController*)viewController
                                      browser:(Browser*)browser
                                     tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to edit a tab group outside the Tab Groups "
         "experiment.";
  CHECK(tabGroup) << "You need to pass a tab group in order to edit it.";
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
  }
  return self;
}

#pragma mark - CreateOrEditTabGroupViewControllerDelegate

- (void)createOrEditTabGroupViewControllerDidDismiss:
            (CreateTabGroupViewController*)viewController
                                            animated:(BOOL)animated {
  [self.delegate createOrEditTabGroupCoordinatorDidDismiss:self
                                                  animated:animated];
}

#pragma mark - CreateTabGroupMediatorDelegate

- (void)createTabGroupMediatorEditedGroupWasExternallyMutated:
    (CreateTabGroupMediator*)mediator {
  [self.delegate createOrEditTabGroupCoordinatorDidDismiss:self animated:YES];
}

#pragma mark - ChromeCoordinator

- (void)start {
  Browser* browser = self.browser;
  ProfileIOS* profile = browser->GetProfile();
  BOOL editMode = _tabGroup != nullptr;
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  BOOL tabSynced =
      syncService && syncService->GetUserSettings()->GetSelectedTypes().Has(
                         syncer::UserSelectableType::kTabs);
  _viewController =
      [[CreateTabGroupViewController alloc] initWithEditMode:editMode
                                                   tabSynced:tabSynced];

  if (_tabGroup) {
    _mediator = [[CreateTabGroupMediator alloc]
        initTabGroupEditionWithConsumer:_viewController
                               tabGroup:_tabGroup
                           webStateList:browser->GetWebStateList()];
    _mediator.delegate = self;
  } else {
    _mediator = [[CreateTabGroupMediator alloc]
        initTabGroupCreationWithConsumer:_viewController
                            selectedTabs:_identifiers
                                 browser:browser];
  }
  _viewController.mutator = _mediator;
  _viewController.delegate = self;

  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _transitionDelegate = [[CreateTabGroupTransitionDelegate alloc] init];
  _viewController.transitioningDelegate = _transitionDelegate;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  [_viewController.presentingViewController
      dismissViewControllerAnimated:self.animatedDismissal
                         completion:nil];
  _viewController = nil;
}

@end
