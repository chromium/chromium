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
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

// TODO(crbug.com/1457146): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

namespace {

// Returns whether the feature kFilterWebsitesForSupervisedUsersOnDesktopAndIOS
// is enabled or not.
bool ShouldFilterWebSitesForSupervisedUsers() {
  return base::FeatureList::IsEnabled(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
}

}  // namespace

@interface IncognitoGridMediator () <IncognitoReauthObserver,
                                     PrefObserverDelegate>
@end

@implementation IncognitoGridMediator {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // YES if incognito is disabled.
  BOOL _incognitoDisabled;
  // Whether this grid is currently selected.
  BOOL _selected;
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
  _selected = selected;
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

- (void)newTabButtonTapped:(id)sender {
  // Ignore the tap if the current page is disabled for some reason, by policy
  // for instance. This is to avoid situations where the tap action from an
  // enabled page can make it to a disabled page by releasing the
  // button press after switching to the disabled page (b/273416844 is an
  // example).
  if (_incognitoDisabled) {
    return;
  }

  [self.gridConsumer setPageIdleStatus:NO];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self.gridConsumer prepareForDismissal];
  [self addNewItem];
  [self.gridConsumer setActivePageFromPage:TabGridPageIncognitoTabs];
  [self.tabPresentationDelegate showActiveTabInPage:TabGridPageIncognitoTabs
                                       focusOmnibox:NO];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCreateIncognitoTab"));
}

#pragma mark - Parent's function

- (void)disconnect {
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  [_reauthSceneAgent removeObserver:self];
  [super disconnect];
}

- (void)configureToolbarsButtons {
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  BOOL authenticationRequired = self.reauthSceneAgent.authenticationRequired;

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc] init];
  toolbarsConfiguration.closeAllButton =
      (!authenticationRequired && !self.webStateList->empty());
  toolbarsConfiguration.doneButton = !authenticationRequired;
  toolbarsConfiguration.newTabButton =
      (!authenticationRequired &&
       IsAddNewTabAllowedByPolicy(self.browser->GetBrowserState()->GetPrefs(),
                                  YES));
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.selectTabsButton =
      (!authenticationRequired && !self.webStateList->empty());
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

#pragma mark - Properties

- (void)setBrowser:(Browser*)browser {
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();

  [super setBrowser:browser];

  if (browser) {
    if (ShouldFilterWebSitesForSupervisedUsers()) {
      PrefService* prefService = browser->GetBrowserState()->GetPrefs();
      DCHECK(prefService);

      _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
      _prefChangeRegistrar->Init(prefService);

      // Register to observe any changes on supervised_user status.
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kSupervisedUserId, _prefChangeRegistrar.get());
    }

    // Pretend the preference changed to force setting _incognitoDisabled.
    [self onPreferenceChanged:prefs::kSupervisedUserId];
  }
}

- (PrefService*)prefService {
  Browser* browser = self.browser;
  DCHECK(browser);

  return browser->GetBrowserState()->GetPrefs();
}

- (void)setReauthSceneAgent:(IncognitoReauthSceneAgent*)reauthSceneAgent {
  if (_reauthSceneAgent == reauthSceneAgent) {
    return;
  }
  [_reauthSceneAgent removeObserver:self];
  _reauthSceneAgent = reauthSceneAgent;
  [_reauthSceneAgent addObserver:self];
}

#pragma mark - IncognitoReauthObserver

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateAuthenticationRequirement:(BOOL)isRequired {
  if (_selected) {
    [self configureToolbarsButtons];
  }
}

#pragma mark - Private

// Returns YES if incognito is disabled.
- (BOOL)isIncognitoModeDisabled {
  DCHECK(self.browser);
  PrefService* prefService = self.browser->GetBrowserState()->GetPrefs();

  if (IsIncognitoModeDisabled(prefService)) {
    return YES;
  }

  return ShouldFilterWebSitesForSupervisedUsers() &&
         supervised_user::IsSubjectToParentalControls(prefService);
}

@end
