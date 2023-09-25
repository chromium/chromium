// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

@interface IncognitoGridMediator () <PrefObserverDelegate>
@end

@implementation IncognitoGridMediator {
  // Preference service from the application context.
  PrefService* _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // YES if incognito is disabled.
  BOOL _incognitoDisabled;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                           consumer:(id<TabCollectionConsumer>)consumer {
  if (self = [super initWithConsumer:consumer]) {
      CHECK(prefService);
      _prefService = prefService;
      _prefChangeRegistrar.Init(_prefService);
      if (base::FeatureList::IsEnabled(
              supervised_user::
                  kFilterWebsitesForSupervisedUsersOnDesktopAndIOS)) {
        _prefObserverBridge.reset(new PrefObserverBridge(self));
        // Register to observe any changes on supervised_user status.
        _prefObserverBridge->ObserveChangesForPreference(
            prefs::kSupervisedUserId, &_prefChangeRegistrar);
      }
      _incognitoDisabled = [self isIncognitoModeDisabled];
    }
  return self;
}

// TODO(crbug.com/1457146): Refactor the grid commands to have the same function
// name to close all.
#pragma mark - GridCommands

- (void)closeAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllIncognitoTabs"));
  // This is a no-op if `webStateList` is already empty.
  self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();
}

- (void)saveAndCloseAllItems {
  NOTREACHED_NORETURN() << "Incognito tabs should not be saved before closing.";
}

- (void)undoCloseAllItems {
  NOTREACHED_NORETURN() << "Incognito tabs are not saved before closing.";
}

- (void)discardSavedClosedItems {
  NOTREACHED_NORETURN() << "Incognito tabs cannot be saved.";
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectIncognitoPanel"));

    [self configureToolbarsButtons];
  }
  // TODO(crbug.com/1457146): Implement.
}

#pragma mark - TabGridToolbarsButtonsDelegate

- (void)closeAllButtonTapped:(id)sender {
  [self closeAllItems];
}

#pragma mark - Parent's function

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nil;
  [super disconnect];
}

- (void)configureToolbarsButtons {
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  toolbarsConfiguration.closeAllButton = !self.webStateList->empty();
  toolbarsConfiguration.doneButton = YES;
  toolbarsConfiguration.newTabButton = IsAddNewTabAllowedByPolicy(
      self.browser->GetBrowserState()->GetPrefs(), YES);
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton = !self.webStateList->empty();
  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kSupervisedUserId) {
    BOOL isDisabled = [self isIncognitoModeDisabled];
    if (_incognitoDisabled != isDisabled) {
      _incognitoDisabled = isDisabled;
      [self.incognitoDelegate shouldDisableIncognito:_incognitoDisabled];
    }
  }
}

#pragma mark - Private

// Returns YES if incognito is disabled.
- (BOOL)isIncognitoModeDisabled {
  if (base::FeatureList::IsEnabled(
          supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS)) {
    return supervised_user::IsSubjectToParentalControls(_prefService) ||
           IsIncognitoModeDisabled(_prefService);
  }
  return IsIncognitoModeDisabled(_prefService);
}

@end
