// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/ui_bundled/web_extension_error_scene_agent.h"

#import <memory>

#import "components/prefs/pref_service.h"
#import "components/universal_optout/features.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"
#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"
#import "ios/chrome/browser/web_extension/model/extension_service_observer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface WebExtensionErrorSceneAgent () <ExtensionServiceObserving>
@end

@implementation WebExtensionErrorSceneAgent {
  std::unique_ptr<ExtensionServiceObserverBridge>
      _extensionServiceObserverBridge;
  BOOL _hasPendingError;
}

- (void)dealloc {
  [self tearDownObservation];
}

// Removes observers and tears down the observer bridge.
- (void)tearDownObservation {
  _extensionServiceObserverBridge.reset();
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self startObservingProfileIfAvailable];
  [self maybeShowErrorSnackbar];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    [self maybeShowErrorSnackbar];
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self startObservingProfileIfAvailable];
  [self maybeShowErrorSnackbar];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self tearDownObservation];
}

- (void)sceneWillDisconnect:(SceneState*)sceneState {
  [self tearDownObservation];
}

#pragma mark - ExtensionServiceObserving

- (void)extensionService:(ExtensionService*)service
    didChangeGPCLoadError:(bool)hasGPCLoadError {
  if (hasGPCLoadError) {
    _hasPendingError = YES;
    [self maybeShowErrorSnackbar];
  }
}

#pragma mark - Private

// Starts observing ExtensionService if the Profile is loaded and the bridge
// is not already created.
- (void)startObservingProfileIfAvailable {
  if (_extensionServiceObserverBridge) {
    return;
  }
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return;
  }
  ExtensionService* service = ExtensionServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  _extensionServiceObserverBridge =
      std::make_unique<ExtensionServiceObserverBridge>(service, self);
  if (service->HasGPCLoadError()) {
    _hasPendingError = YES;
  }
}

// Presents the web extension error snackbar if the scene is active and UI is
// enabled, and the extension service has encountered an error.
- (void)maybeShowErrorSnackbar {
  if (universal_optout::features::IsUniversalOptOutExtensionSilentEnabled()) {
    return;
  }
  if (!_hasPendingError) {
    return;
  }
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return;
  }
  PrefService* prefService = profile->GetPrefs();
  if (prefService->GetBoolean(prefs::kUniversalOptOutErrorToastShown)) {
    _hasPendingError = NO;
    return;
  }
  if (self.sceneState.activationLevel != SceneActivationLevelForegroundActive) {
    return;
  }
  if (!self.sceneState.UIEnabled) {
    return;
  }

  Browser* browser =
      self.sceneState.browserProviderInterface.currentBrowserProvider.browser;
  if (!browser) {
    return;
  }

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  if (!snackbarHandler || !settingsHandler) {
    return;
  }

  _hasPendingError = NO;
  prefService->SetBoolean(prefs::kUniversalOptOutErrorToastShown, true);

  NSString* message =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_MESSAGE);
  NSString* buttonText =
      l10n_util::GetNSString(IDS_IOS_UNIVERSAL_OPT_OUT_SNACKBAR_ACTION);

  __weak id<SettingsCommands> weakSettingsHandler = settingsHandler;

  [snackbarHandler
      showSnackbarWithMessage:message
                   buttonText:buttonText
                messageAction:^{
                  [weakSettingsHandler showUniversalOptOutSettings];
                }
             completionAction:nil];
}

@end
