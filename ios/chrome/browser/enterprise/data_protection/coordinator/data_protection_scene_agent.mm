// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/data_protection_scene_agent.h"

#import <algorithm>

#import "base/scoped_observation.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/rules_service_observer_bridge.h"
#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_style_utils.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/screenshot_protection/screenshot_protection_api.h"

namespace {

// Represents the current screenshot protection state applied to the window.
enum class ProtectionState {
  // Screenshot protection is currently applied to the window.
  kEnabled,
  // Screenshot protection is currently removed from the window.
  kDisabled,
};

// Returns whether real time enterprise lookups are enabled via Enterprise
// policies.
bool AreEnterpriseLookupsEnabled(const ProfileIOS& profile) {
  if (profile.IsOffTheRecord()) {
    return false;
  }

  const PrefService* prefs = profile.GetPrefs();
  CHECK(prefs);

  // Check lookups policy.
  enterprise_connectors::EnterpriseRealTimeUrlCheckMode mode =
      static_cast<enterprise_connectors::EnterpriseRealTimeUrlCheckMode>(
          prefs->GetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckMode));

  return enterprise_connectors::IsEnterpriseUrlFilteringEnabled(mode);
}

}  // namespace

@interface DataProtectionSceneAgent () <BrowserObserving,
                                        DataProtectionTabHelperObserving,
                                        IncognitoStateObserver,
                                        PrefObserverDelegate,
                                        ProfileStateObserver,
                                        RulesServiceObserving,
                                        TabGridStateObserving,
                                        TabsDependencyInstalling,
                                        WebStateListObserving>
@end

@implementation DataProtectionSceneAgent {
  // Bridge to observe rules service changes.
  std::unique_ptr<data_controls::RulesServiceObserverBridge>
      _rulesObserverBridge;
  // Observation for rules service changes.
  std::unique_ptr<
      base::ScopedObservation<data_controls::RulesServiceBase,
                              data_controls::RulesServiceBase::Observer>>
      _rulesObservation;

  // Registrar for pref changes.
  PrefChangeRegistrar _prefRegistrar;
  // Bridge to observe pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // The last protection state that was applied.
  ProtectionState _currentProtectionState;

  // Bridge to observe browser destruction.
  std::unique_ptr<BrowserObserverBridge> _browserObserverBridge;

  // Observes active WebState changes in the current Browser.
  std::unique_ptr<TabsDependencyInstallerBridge> _tabDependencyInstallerBridge;

  // Bridge to observe WebStateList events in the current Browser.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _webStateListObservation;

  // Bridge to observe the protection state of the currently active WebState.
  std::unique_ptr<DataProtectionTabHelperObserverBridge>
      _tabHelperObserverBridge;

  // The view used to display the watermark over the tab grid.
  WatermarkView* _tabGridWatermarkView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _currentProtectionState = ProtectionState::kDisabled;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  CHECK(sceneState.profileState);
  [sceneState.tabGridState addObserver:self];
  [sceneState.profileState addObserver:self];
  [sceneState.incognitoState addObserver:self];

  // Observe prefs if profile was loaded.
  [self startObservingPrefs];

  [self observeCurrentBrowser];

  [self startObservingWindows];

  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self observeCurrentBrowser];
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Restore the windows state.
  if (_currentProtectionState == ProtectionState::kEnabled) {
    _currentProtectionState = ProtectionState::kDisabled;
    [self applyScreenshotProtection:NO toWindows:self.sceneState.scene.windows];
  }

  [self stopObservingWindows];
  [self removeTabGridWatermark];

  [sceneState.profileState removeObserver:self];
  [sceneState.incognitoState removeObserver:self];
  [sceneState.tabGridState removeObserver:self];
  [sceneState removeObserver:self];

  [self stopObservingPrefs];
  [self teardownBrowserObservers];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // Profile might be loaded now.
  [self startObservingPrefs];
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

#pragma mark - TabGridStateObserving

- (void)willEnterTabGrid {
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

- (void)willExitTabGrid {
  [self updateScreenshotProtection];
  [self removeTabGridWatermark];
}

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  // Monitor protection changes in the inconito browser.
  [self observeCurrentBrowser];
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  // Monitor protection changes in the regular browser.
  [self observeCurrentBrowser];
  [self updateScreenshotProtection];
  [self updateTabGridWatermarking];
}

#pragma mark - TabsDependencyInstalling

- (void)newWebStateActivated:(web::WebState*)newActiveWebState
           oldActiveWebState:(web::WebState*)oldActiveWebState {
  [self observeActiveWebState];

  [self updateScreenshotProtection];
}

- (void)webStateInserted:(web::WebState*)webState {
  // No-op.
}

- (void)webStateRemoved:(web::WebState*)webState {
  // No-op.
}

- (void)webStateDeleted:(web::WebState*)webState {
  // No-op.
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // Do nothing if it's a batch operation since it will be handled in
  // `webStateListBatchOperationEnded`.
  if (webStateList->IsBatchInProgress()) {
    return;
  }
  [self updateTabGridWatermarking];
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  [self updateTabGridWatermarking];
}

#pragma mark - DataProtectionTabHelperObserving

- (void)screenshotProtectionDidChangeForWebState:(web::WebState*)webState
                                     isProtected:(BOOL)isProtected {
  [self updateScreenshotProtection];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  [self updateScreenshotProtection];
}

#pragma mark - RulesServiceObserving

- (void)onRulesUpdated {
  // Rules changes only trigger protection updates on the tab grid. The tab
  // helper observation drives updates while not in the tab grid.
  if (self.sceneState.tabGridState.tabGridVisible) {
    [self updateScreenshotProtection];
  }
}

#pragma mark - BrowserObserving

- (void)browserDestroyed:(Browser*)browser {
  [self teardownBrowserObservers];
}

#pragma mark - Private

- (Browser*)currentBrowser {
  CHECK(self.sceneState.UIEnabled);
  id<BrowserProviderInterface> browserProviderInterface =
      self.sceneState.browserProviderInterface;

  id<BrowserProvider> browserProvider =
      self.sceneState.incognitoState.incognitoContentVisible
          ? browserProviderInterface.incognitoBrowserProvider
          : browserProviderInterface.mainBrowserProvider;

  return browserProvider.browser;
}

// Whether the scene can be protected.
- (BOOL)isSceneStateReadyForProtection {
  // Only protect the scene when in the foreground, the profile has been loaded
  // and the UI is enabled.
  return self.sceneState.activationLevel >=
             SceneActivationLevelForegroundInactive &&
         self.sceneState.profileState.initStage >=
             ProfileInitStage::kProfileLoaded &&
         self.sceneState.UIEnabled;
}

// Stops tracking pref and data control rules.
- (void)stopObservingPrefs {
  _rulesObservation.reset();
  _rulesObserverBridge.reset();
  _prefObserverBridge.reset();
  _prefRegistrar.RemoveAll();
}

// Starts watching for changes in the protection of the current Browser's active
// WebState. No-op if the scene's UI is not enabled yet. Safe to call multiple
// times.
- (void)observeCurrentBrowser {
  if (!self.sceneState.UIEnabled) {
    return;
  }
  // Make sure we always monitor the active WebState.
  [self observeCurrentBrowserActiveWebStateChanges];
  [self observeActiveWebState];
}

// Returns the current Browser's active WebState, if any.
- (web::WebState*)activeWebState {
  return [self currentBrowser]->GetWebStateList()->GetActiveWebState();
}

// Monitor for changes in the current Browser's active WebState.
- (void)observeCurrentBrowserActiveWebStateChanges {
  CHECK(self.sceneState.UIEnabled);
  [self teardownBrowserObservers];

  Browser* browser = [self currentBrowser];
  _browserObserverBridge =
      std::make_unique<BrowserObserverBridge>(browser, self);

  _tabDependencyInstallerBridge =
      std::make_unique<TabsDependencyInstallerBridge>();
  _tabDependencyInstallerBridge->StartObserving(self, browser);

  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _webStateListObservation = std::make_unique<
      base::ScopedObservation<WebStateList, WebStateListObserver>>(
      _webStateListObserverBridge.get());
  _webStateListObservation->Observe(browser->GetWebStateList());
}

// Cleans up Browser and WebState layer observers.
- (void)teardownBrowserObservers {
  _tabHelperObserverBridge.reset();
  _webStateListObservation.reset();
  _webStateListObserverBridge.reset();
  _browserObserverBridge.reset();
  if (_tabDependencyInstallerBridge) {
    _tabDependencyInstallerBridge->StopObserving();
    _tabDependencyInstallerBridge.reset();
  }
}

// Determines the current screenshot protection state and applies it to the
// scene's window. If the target state is identical to the current active state,
// this method acts as a no-op. The method is also a no-op if the scene is not
// ready yet for protection (e.g. UI is not enabled).
- (void)updateScreenshotProtection {
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  UIWindowScene* windowScene = self.sceneState.scene;
  if (!windowScene) {
    return;
  }

  BOOL shouldProtect = self.sceneState.tabGridState.tabGridVisible
                           ? [self isTabGridProtectionEnabled]
                           : [self isWebStateProtectionEnabled];

  ProtectionState targetState =
      shouldProtect ? ProtectionState::kEnabled : ProtectionState::kDisabled;

  // No-op if the protection state did not change.
  if (_currentProtectionState == targetState) {
    return;
  }

  _currentProtectionState = targetState;

  [self applyScreenshotProtection:shouldProtect toWindows:windowScene.windows];
}

// Observes changes in the Scene's windows.
- (void)startObservingWindows {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(windowDidBecomeVisible:)
             name:UIWindowDidBecomeVisibleNotification
           object:nil];
}

// Removes observation on the Scene's windows.
- (void)stopObservingWindows {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIWindowDidBecomeVisibleNotification
              object:nil];
}

// Applies screenshot protection to new windows.
- (void)windowDidBecomeVisible:(NSNotification*)notification {
  UIWindow* newWindow = notification.object;

  if (newWindow.windowScene != self.sceneState.scene) {
    return;
  }
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  if (_currentProtectionState == ProtectionState::kDisabled) {
    return;
  }

  [self applyScreenshotProtection:YES toWindow:newWindow];
}

// Tracks pref and rule service changes for updating screenshot protection.
// No-op if the scene is not ready for protection. Also no-op if observers were
// already set up.
- (void)startObservingPrefs {
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  // No-op if observation was previously set up.
  if (!_prefRegistrar.IsEmpty()) {
    return;
  }

  ProfileIOS* profile = self.sceneState.profileState.profile;
  CHECK(profile);

  data_controls::IOSRulesService* rulesService =
      data_controls::IOSRulesServiceFactory::GetForProfile(profile);
  CHECK(rulesService);

  CHECK(!_rulesObserverBridge);
  CHECK(!_rulesObservation);
  _rulesObserverBridge =
      std::make_unique<data_controls::RulesServiceObserverBridge>(self);
  _rulesObservation = std::make_unique<
      base::ScopedObservation<data_controls::RulesServiceBase,
                              data_controls::RulesServiceBase::Observer>>(
      _rulesObserverBridge.get());
  _rulesObservation->Observe(rulesService);

  CHECK(!_prefObserverBridge);
  _prefRegistrar.Init(profile->GetPrefs());
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

  _prefObserverBridge->ObserveChangesForPreference(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, &_prefRegistrar);
}

// Returns whether the overall Tab Grid should be shielded based on enterprise
// policies configured on the current active profile.
- (BOOL)isTabGridProtectionEnabled {
  CHECK([self isSceneStateReadyForProtection]);

  ProfileIOS* profile = [self currentProfile];

  data_controls::IOSRulesService* rulesService =
      data_controls::IOSRulesServiceFactory::GetForProfile(profile);
  CHECK(rulesService);
  // Protect the tab grid if there is at least one screenshot data control rule.
  // We could go through each tab checking if its URL matches a blocking rule.
  // However, we can't do the same for real time lookups as most tabs will be
  // unrealized. We chose to keep the logic simple and just check for the
  // presence of a blocking rule or lookups enabled.
  return rulesService->HasBlockingScreenshotRule() ||
         AreEnterpriseLookupsEnabled(*profile);
}

// Returns whether screenshot protection is enabled for the current Browser's
// active WebState.
- (BOOL)isWebStateProtectionEnabled {
  CHECK([self isSceneStateReadyForProtection]);

  Browser* currentBrowser = [self currentBrowser];
  auto* activeWebState = currentBrowser->GetWebStateList()->GetActiveWebState();

  if (!activeWebState) {
    return NO;
  }
  DataProtectionTabHelper* tabHelper =
      DataProtectionTabHelper::FromWebState(activeWebState);
  CHECK(tabHelper);

  return tabHelper->IsScreenshotProtectionEnabled();
}

// Returns the profile corresponding to the currently active UI (Main or
// Incognito).
- (ProfileIOS*)currentProfile {
  CHECK([self isSceneStateReadyForProtection]);

  ProfileIOS* profile = self.sceneState.profileState.profile;
  CHECK(profile);

  return self.sceneState.incognitoState.incognitoContentVisible
             ? profile->GetOffTheRecordProfile()
             : profile;
}

// Applies or removes screenshot protection on `windows`.
- (void)applyScreenshotProtection:(BOOL)isProtected
                        toWindows:(NSArray<UIWindow*>*)windows {
  for (UIWindow* window in windows) {
    [self applyScreenshotProtection:isProtected toWindow:window];
  }
}

// Applies or removes screenshot protection on the given window.
- (void)applyScreenshotProtection:(BOOL)isProtected toWindow:(UIWindow*)window {
  // Only apply a protection when the scene is ready or disable it when the UI
  // is being tear down.
  CHECK((!isProtected && !self.sceneState.UIEnabled) ||
        [self isSceneStateReadyForProtection]);
  // Applied protection must match internal state.
  CHECK(isProtected || _currentProtectionState == ProtectionState::kDisabled);

  ios::provider::ScreenshotProtectionOptions options;
  options.obfuscate_screenshots = isProtected;
  options.obfuscate_screen_recordings = isProtected;
  ios::provider::SetScreenshotProtection(window, options);
}

// Returns the watermark text to be displayed over the Tab Grid if any tab in
// the current window's active Browser requires watermarking.
- (NSString*)watermarkTextForTabGrid {
  ProfileIOS* profile = [self currentProfile];
  if (!profile || !AreEnterpriseLookupsEnabled(*profile)) {
    return nil;
  }

  Browser* browser = [self currentBrowser];
  if (!browser || !browser->GetWebStateList()) {
    return nil;
  }

  const base::DictValue& watermarkedTabs = profile->GetPrefs()->GetDict(
      enterprise_data_protection::kDataProtectionWatermarkedTabs);
  if (watermarkedTabs.empty()) {
    return nil;
  }

  WebStateList* webStateList = browser->GetWebStateList();
  // Always watermark the tab grid if the total tabs more than the
  // `kMaxTabsToCheckForWatermark`, and there are recorded entries in the
  // `kDataProtectionWatermarkedTabs` prefs map.
  if (webStateList->count() > kMaxTabsToCheckForWatermark) {
    auto it = watermarkedTabs.begin();
    return base::SysUTF8ToNSString(it->second.GetString());
  }

  for (int i = 0; i < webStateList->count(); ++i) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    std::string webStateID =
        base::NumberToString(webState->GetUniqueIdentifier().identifier());
    const std::string* text = watermarkedTabs.FindString(webStateID);
    if (text && !text->empty()) {
      return base::SysUTF8ToNSString(*text);
    }
  }

  return nil;
}

// Updates the watermark view for the Tab Grid window based on the current
// scene state and the watermarking status of open tabs in this window.
- (void)updateTabGridWatermarking {
  // If the scene is not ready (in the background or still initializing), do
  // nothing.
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  if (!IsEnableEnterpriseWatermarkingIOS() ||
      !self.sceneState.tabGridState.tabGridVisible) {
    [self removeTabGridWatermark];
    return;
  }

  NSString* watermarkText = [self watermarkTextForTabGrid];
  if (!watermarkText.length) {
    [self removeTabGridWatermark];
    return;
  }

  UIWindow* window = self.sceneState.window;
  if (!window) {
    return;
  }

  if (!_tabGridWatermarkView) {
    _tabGridWatermarkView = [[WatermarkView alloc] initWithFrame:CGRectZero];
    _tabGridWatermarkView.translatesAutoresizingMaskIntoConstraints = NO;
  }

  ProfileIOS* profile = [self currentProfile];
  WatermarkStyle style = GetWatermarkStyleFromPrefs(profile->GetPrefs());
  [_tabGridWatermarkView updateWithText:watermarkText style:style];

  if (_tabGridWatermarkView.superview != window) {
    [window addSubview:_tabGridWatermarkView];
    AddSameConstraints(_tabGridWatermarkView, window);
  }
}

// Removes the watermark view from the window.
- (void)removeTabGridWatermark {
  [_tabGridWatermarkView removeFromSuperview];
  _tabGridWatermarkView = nil;
}

// Observes screenshot protection changes for the active WebState.
- (void)observeActiveWebState {
  CHECK(self.sceneState.UIEnabled);
  // Cleanup the previous observer.
  _tabHelperObserverBridge.reset();

  web::WebState* webState = [self activeWebState];
  if (!webState) {
    return;
  }

  DataProtectionTabHelper* tabHelper =
      DataProtectionTabHelper::FromWebState(webState);
  CHECK(tabHelper);
  _tabHelperObserverBridge =
      std::make_unique<DataProtectionTabHelperObserverBridge>(self, tabHelper);
}

@end
