// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/notimplemented.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities_observer_bridge.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_observing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

@interface TabGridMediator () <PrefObserverDelegate,
                               SupervisedUserCapabilitiesObserving,
                               TabGridModeObserving>
@end

@implementation TabGridMediator {
  // Current selected grid.
  id<TabGridPageMutator> _currentPageMutator;
  // Preference service from the application context.
  raw_ptr<PrefService> _prefService;
  // Feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _engagementTracker;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Identity manager providing AccountInfo capabilities.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer to track changes to supervision-related capabilities.
  std::unique_ptr<supervised_user::SupervisedUserCapabilitiesObserverBridge>
      _supervisedUserCapabilitiesObserver;
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

    if (base::FeatureList::IsEnabled(
            supervised_user::
                kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
      _identityManager = identityManager;
      _supervisedUserCapabilitiesObserver = std::make_unique<
          supervised_user::SupervisedUserCapabilitiesObserverBridge>(
          _identityManager, self);
    } else {
      _prefService = prefService;
      _prefChangeRegistrar.Init(_prefService);
      _prefObserverBridge.reset(new PrefObserverBridge(self));

      // Register to observe any changes on supervised_user status.
      _prefObserverBridge->ObserveChangesForPreference(prefs::kSupervisedUserId,
                                                       &_prefChangeRegistrar);
    }
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nil;
  _supervisedUserCapabilitiesObserver.reset();
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
  BOOL isSubjectToParentalControls;
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    isSubjectToParentalControls =
        supervised_user::IsPrimaryAccountSubjectToParentalControls(
            _identityManager) == signin::Tribool::kTrue;
  } else {
    isSubjectToParentalControls =
        supervised_user::IsSubjectToParentalControls(*_prefService);
  }
  [_consumer updateParentalControlStatus:isSubjectToParentalControls];
}

#pragma mark - PrefObserverDelegate

// TODO(b/295307282): Migrate to IncognitoGridMediator once the incognito grid
// coordinator and view controller is ready.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (!base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS) &&
      preferenceName == prefs::kSupervisedUserId) {
    [_consumer updateParentalControlStatus:
        supervised_user::IsSubjectToParentalControls(*_prefService)];
    [_consumer updateTabGridForIncognitoModeDisabled:IsIncognitoModeDisabled(
                                                         _prefService)];
  }
}

#pragma mark - SupervisedUserCapabilitiesObserving

- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState {
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    BOOL isSubjectToParentalControl =
        (capabilityUpdateState ==
         supervised_user::CapabilityUpdateState::kSetToTrue);
    [_consumer updateParentalControlStatus:isSubjectToParentalControl];
    [_consumer updateTabGridForIncognitoModeDisabled:IsIncognitoModeDisabled(
                                                         _prefService)];
  }
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
    case TabGridPageRemoteTabs:
      _currentPageMutator = self.remotePageMutator;
      break;
    case TabGridPage::TabGridPageTabGroups:
      _currentPageMutator = self.tabGroupsPageMutator;
      break;
  }
}

// Notifies mutators if it is the current selected one or not.
- (void)notifyPageMutatorAboutPage:(TabGridPage)page {
  [_currentPageMutator currentlySelectedGrid:NO];
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
