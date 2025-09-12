// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/notimplemented.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/supervised_user/model/family_link_user_capabilities_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_observing.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_page_mutator.h"

@interface TabGridMediator () <FamilyLinkUserCapabilitiesObserving,
                               TabGridModeObserving>
@end

@implementation TabGridMediator {
  // Current selected grid.
  id<TabGridPageMutator> _currentPageMutator;
  // Preference service from the application context.
  raw_ptr<PrefService> _prefService;
  // Feature engagement tracker.
  raw_ptr<feature_engagement::Tracker, DanglingUntriaged> _engagementTracker;
  // Identity manager providing AccountInfo capabilities.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer to track changes to Family Link user state.
  std::unique_ptr<supervised_user::FamilyLinkUserCapabilitiesObserverBridge>
      _familyLinkUserCapabilitiesObserver;
  // Holder for the current mode of the TabGrid.
  TabGridModeHolder* _modeHolder;
}

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                            prefService:(PrefService*)prefService
               featureEngagementTracker:(feature_engagement::Tracker*)tracker
                             modeHolder:(TabGridModeHolder*)modeHolder {
  self = [super init];
  if (self) {
    CHECK(identityManager);
    CHECK(prefService);
    CHECK(tracker);
    CHECK(modeHolder);
    _engagementTracker = tracker;
    _modeHolder = modeHolder;
    [_modeHolder addObserver:self];
    _identityManager = identityManager;
    _familyLinkUserCapabilitiesObserver = std::make_unique<
        supervised_user::FamilyLinkUserCapabilitiesObserverBridge>(
        _identityManager, self);
    _prefService = prefService;
  }
  return self;
}

- (void)disconnect {
  _prefService = nil;
  _familyLinkUserCapabilitiesObserver.reset();
  _identityManager = nil;
  _consumer = nil;

  [_modeHolder removeObserver:self];
  _modeHolder = nil;
}

#pragma mark - Public

- (void)setActivePage:(TabGridPage)page {
  [self notifyPageMutatorAboutPage:page];
  [_currentPageMutator setPageAsActive];
}

- (void)setConsumer:(id<TabGridConsumer>)consumer {
  _consumer = consumer;
  BOOL isSubjectToParentalControls =
      supervised_user::IsPrimaryAccountSubjectToParentalControls(
          _identityManager) == signin::Tribool::kTrue;
  [_consumer updateParentalControlStatus:isSubjectToParentalControls];
}

#pragma mark - PrefObserverDelegate

#pragma mark - FamilyLinkUserCapabilitiesObserving

- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState {
  BOOL isSubjectToParentalControl =
      (capabilityUpdateState ==
       supervised_user::CapabilityUpdateState::kSetToTrue);
  [_consumer updateParentalControlStatus:isSubjectToParentalControl];
  [_consumer updateTabGridForIncognitoModeDisabled:IsIncognitoModeDisabled(
                                                       _prefService)];
}

#pragma mark - Private

// Sets the current selected mutator according to the page.
- (void)updateCurrentPageMutatorForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      _currentPageMutator = self.incognitoPageMutator;
      break;
    case TabGridPageRegularTabs:
      _currentPageMutator = self.regularPageMutator;
      break;
    case TabGridPage::TabGridPageTabGroups:
      _currentPageMutator = self.tabGroupsPageMutator;
      break;
  }
}

// Notifies mutators if it is the current selected one or not.
- (void)notifyPageMutatorAboutPage:(TabGridPage)page {
  [_currentPageMutator currentlySelectedGrid:NO];
  if (_modeHolder.mode == TabGridMode::kSearch) {
    // It shouldn't be possible to switch panel in search mode, but it is
    // doable with the right timing. Cancel search if it happens.
    _modeHolder.mode = TabGridMode::kNormal;
  }
  [self updateCurrentPageMutatorForPage:page];
  [_currentPageMutator currentlySelectedGrid:YES];
}

#pragma mark - TabGridModeObserving

- (void)tabGridModeDidChange:(TabGridModeHolder*)modeHolder {
  [self.consumer setMode:modeHolder.mode];
}

#pragma mark - TabGridMutator

- (void)pageChanged:(TabGridPage)currentPage
        interaction:(TabSwitcherPageChangeInteraction)interaction {
  UMA_HISTOGRAM_ENUMERATION(kUMATabSwitcherPageChangeInteractionHistogram,
                            interaction);

  [self notifyPageMutatorAboutPage:currentPage];
  if (currentPage == TabGridPageIncognitoTabs) {
    switch (interaction) {
      case TabSwitcherPageChangeInteraction::kScrollDrag:
      case TabSwitcherPageChangeInteraction::kAccessibilitySwipe:
        _engagementTracker->NotifyEvent(
            feature_engagement::events::kIOSSwipeRightForIncognitoUsed);
        break;
      case TabSwitcherPageChangeInteraction::kControlTap:
      case TabSwitcherPageChangeInteraction::kControlDrag:
        _engagementTracker->NotifyEvent(
            feature_engagement::events::kIOSIncognitoPageControlTapped);
        break;
      case TabSwitcherPageChangeInteraction::kNone:
      case TabSwitcherPageChangeInteraction::kItemDrag:
        break;
    }
  }
  // TODO(crbug.com/40921760): Implement the incognito grid or content visible
  // notification.
}

- (void)dragAndDropSessionStarted {
  [self.toolbarsMutator setButtonsEnabled:NO];
}

- (void)dragAndDropSessionEnded {
  [self.toolbarsMutator setButtonsEnabled:YES];
}

- (void)quitSearchMode {
  _modeHolder.mode = TabGridMode::kNormal;
}

@end
