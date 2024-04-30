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
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator_provider_wrangler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

@interface TabGridMediator () <PrefObserverDelegate>
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
  // Current page.
  // TODO(crbug.com/41487637): Remove once the mediator and the view controller
  // are fully sync.
  TabGridPage _currentPage;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
           featureEngagementTracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    CHECK(prefService);
    CHECK(tracker);
    _prefService = prefService;
    _engagementTracker = tracker;
    _prefChangeRegistrar.Init(_prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    // Register to observe any changes on supervised_user status.
    _prefObserverBridge->ObserveChangesForPreference(prefs::kSupervisedUserId,
                                                     &_prefChangeRegistrar);
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nil;
  _consumer = nil;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  [self notifyPageMutatorAboutPage:page];
}

- (void)setModeOnCurrentPage:(TabGridMode)mode {
  [_currentPageMutator switchToMode:mode];
}

- (void)setConsumer:(id<TabGridConsumer>)consumer {
  _consumer = consumer;
  [_consumer updateParentalControlStatus:
      supervised_user::IsSubjectToParentalControls(*_prefService)];
}

#pragma mark - PrefObserverDelegate

// TODO(b/295307282): Migrate to IncognitoGridMediator once the incognito grid
// coordinator and view controller is ready.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kSupervisedUserId) {
    [_consumer updateParentalControlStatus:
        supervised_user::IsSubjectToParentalControls(*_prefService)];
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
      // TODO(crbug.com/329626033): Handle displaying Tab Groups.
      NOTIMPLEMENTED();
      break;
  }
}

// Notifies mutators if it is the current selected one or not.
- (void)notifyPageMutatorAboutPage:(TabGridPage)page {
  _currentPage = page;
  [_currentPageMutator currentlySelectedGrid:NO];
  [self updateCurrentPageMutatorForPage:page];
  [_currentPageMutator currentlySelectedGrid:YES];
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

#pragma mark - TabGridMediatorProviderWrangler

- (TabGridPage)currentPage {
  return _currentPage;
}

@end
