// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/supervised_user_capabilities.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities_observer_bridge.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/web/public/web_state_id.h"

// TODO(crbug.com/40273478): Needed for `TabPresentationDelegate`, should be
// refactored.
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@interface IncognitoGridMediator () <IncognitoReauthObserver,
                                     PrefObserverDelegate,
                                     SupervisedUserCapabilitiesObserving>
@end

@implementation IncognitoGridMediator {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // YES if incognito is disabled.
  BOOL _incognitoDisabled;
  // YES if parental supervision is enabled.
  BOOL _isSubjectToParentalControl;
  // Whether this grid is currently selected.
  BOOL _selected;
  // Identity manager providing AccountInfo capabilities to determine
  // supervision status. This identity manager is not available for
  // the incognito browser state and need to be passed in.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer to track changes to supervision-related capabilities.
  std::unique_ptr<supervised_user::SupervisedUserCapabilitiesObserverBridge>
      _supervisedUserCapabilitiesObserver;
}

// TODO(crbug.com/40273478): Refactor the grid commands to have the same
// function name to close all.
#pragma mark - GridCommands

- (void)closeItemWithID:(web::WebStateID)itemID {
  // Record when an incognito tab is closed.
  base::RecordAction(base::UserMetricsAction("MobileTabGridCloseIncognitoTab"));
  [super closeItemWithID:itemID];
}

- (void)closeAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllIncognitoTabs"));
  // This is a no-op if `webStateList` is already empty.
  CloseAllWebStates(*self.webStateList, WebStateList::CLOSE_USER_ACTION);
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

- (void)setPinState:(BOOL)pinState forItemWithID:(web::WebStateID)itemID {
  NOTREACHED_NORETURN() << "Should not be called in incognito.";
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  _selected = selected;
  if (selected) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectIncognitoPanel"));

    [self configureToolbarsButtons];
  }
}

#pragma mark - TabGridToolbarsGridDelegate

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

  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self.gridConsumer prepareForDismissal];
  // Present the tab only if it have been added.
  if ([self addNewItem]) {
    [self displayActiveTab];
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCreateIncognitoTab"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridFailedCreateIncognitoTab"));
  }
}

#pragma mark - Parent's function

- (void)disconnect {
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _supervisedUserCapabilitiesObserver.reset();
  _identityManager = nil;
  [_reauthSceneAgent removeObserver:self];
  [super disconnect];
}

- (void)configureToolbarsButtons {
  if (!_selected) {
    return;
  }
  // Start to configure the delegate, so configured buttons will depend on the
  // correct delegate.
  [self.toolbarsMutator setToolbarsButtonsDelegate:self];

  BOOL authenticationRequired = self.reauthSceneAgent.authenticationRequired;
  if (_incognitoDisabled || authenticationRequired) {
    [self.toolbarsMutator
        setToolbarConfiguration:
            [TabGridToolbarsConfiguration
                disabledConfigurationForPage:TabGridPageIncognitoTabs]];
    return;
  }

  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc]
          initWithPage:TabGridPageIncognitoTabs];
  toolbarsConfiguration.mode = self.currentMode;

  if (self.currentMode == TabGridModeSelection) {
    [self configureButtonsInSelectionMode:toolbarsConfiguration];
  } else {
    toolbarsConfiguration.closeAllButton = !self.webStateList->empty();
    toolbarsConfiguration.doneButton = !self.webStateList->empty();
    toolbarsConfiguration.newTabButton = YES;
    toolbarsConfiguration.searchButton = YES;
    toolbarsConfiguration.selectTabsButton = !self.webStateList->empty();
  }

  [self.toolbarsMutator setToolbarConfiguration:toolbarsConfiguration];
}

- (void)displayActiveTab {
  [self.gridConsumer setActivePageFromPage:TabGridPageIncognitoTabs];
  [self.tabPresentationDelegate showActiveTabInPage:TabGridPageIncognitoTabs
                                       focusOmnibox:NO];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (!base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS) &&
      preferenceName == prefs::kSupervisedUserId) {
    BOOL isDisabled = [self isIncognitoModeDisabled];
    if (_incognitoDisabled != isDisabled) {
      _incognitoDisabled = isDisabled;
      [self.incognitoDelegate shouldDisableIncognito:_incognitoDisabled];
    }

    [self configureToolbarsButtons];
  }
}

#pragma mark - Properties

- (void)setBrowser:(Browser*)browser {
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();

  [super setBrowser:browser];

  if (browser) {
    PrefService* prefService = browser->GetBrowserState()->GetPrefs();
    DCHECK(prefService);

    if (!base::FeatureList::IsEnabled(
            supervised_user::
                kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
      _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
      _prefChangeRegistrar->Init(prefService);

      // Register to observe any changes on supervised_user status.
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kSupervisedUserId, _prefChangeRegistrar.get());
    }

    _incognitoDisabled = [self isIncognitoModeDisabled];
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
  if (isRequired) {
    [self.tabGroupsHandler hideTabGroup];
  }
  if (_selected) {
    if (isRequired) {
      self.currentMode = TabGridModeNormal;
    }
    [self configureToolbarsButtons];
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
    if (_isSubjectToParentalControl != isSubjectToParentalControl) {
      _isSubjectToParentalControl = isSubjectToParentalControl;
      _incognitoDisabled = [self isIncognitoModeDisabled];
      [self.incognitoDelegate shouldDisableIncognito:_incognitoDisabled];
    }

    [self configureToolbarsButtons];
  }
}

#pragma mark - Public

- (void)initializeSupervisedUserCapabilitiesObserver:
    (signin::IdentityManager*)identityManager {
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    DCHECK(identityManager);
    _identityManager = identityManager;
    _supervisedUserCapabilitiesObserver = std::make_unique<
        supervised_user::SupervisedUserCapabilitiesObserverBridge>(
        _identityManager, self);
    _incognitoDisabled = [self isIncognitoModeDisabled];
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

  // Incognito mode is disabled for supervised users.
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionPrefsWithAccountCapabilitiesOnIOS)) {
    return _identityManager &&
           supervised_user::IsPrimaryAccountSubjectToParentalControls(
               _identityManager) == signin::Tribool::kTrue;
  } else {
    return supervised_user::IsSubjectToParentalControls(*prefService);
  }
}

@end
