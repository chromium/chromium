// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/search_engine_choice_profile_agent.h"

#import <memory>

#import "base/check.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/search_engine_choice_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Enum storing the result of deciding whether the Search Engine Choice
// Screen should be skipped or not.
enum class SkipScreenDecision {
  kUnknown,
  kPresent,
  kSkip,
};

// Returns the SearchEngineChoiceCommands handler for a SceneState.
id<SearchEngineChoiceCommands> GetSearchEngineChoiceHandler(
    SceneState* scene_state) {
  if (Browser* browser =
          scene_state.browserProviderInterface.currentBrowserProvider.browser) {
    return HandlerForProtocol(browser->GetCommandDispatcher(),
                              SearchEngineChoiceCommands);
  }

  return nil;
}

}  // namespace

@implementation SearchEngineChoiceProfileAgent {
  // UI blocker used by the search engine selection screen.
  std::unique_ptr<ScopedUIBlocker> _searchEngineChoiceUIBlocker;
  // Scene state ID where the search engine choice dialog is displayed.
  std::string _searchEngineChoiceSceneStateID;
  // Store whether the Search Engine Choice Screen should be skipped or not.
  SkipScreenDecision _skipScreenDecision;
  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.profileState.initStage != ProfileInitStage::kChoiceScreen) {
    return;
  }

  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      // Nothing to do as the SceneState is not ready.
      break;

    case SceneActivationLevelForegroundActive:
      [self maybeShowChoiceScreen:sceneState];
      break;

    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      [self sceneStateDisconnected:sceneState];
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kChoiceScreen) {
    return;
  }
  if ([self shouldShowChoiceScreen]) {
    AppState* appState = profileState.appState;
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kChoiceScreen) {
    // Try to present the Choice Screen on the first active SceneState.
    if (SceneState* sceneState = profileState.foregroundActiveScene) {
      [self maybeShowChoiceScreen:sceneState];
    }
    return;
  }

  if (fromInitStage == ProfileInitStage::kChoiceScreen) {
    _scopedForceOrientation.reset();
    [profileState removeAgent:self];
  }
}

#pragma mark - Private

// Returns whether the app was started via an external intent (i.e. any
// connected scene was given an external intent).
- (BOOL)startupHadExternalIntent {
  for (SceneState* sceneState in self.profileState.connectedScenes) {
    if (sceneState.startupHadExternalIntent) {
      return YES;
    }
  }

  return NO;
}

// Returns whether the choice screen should be presented or not. The return
// value is cached to ensure stability.
- (BOOL)shouldShowChoiceScreen {
  DCHECK(self.profileState.profile);
  ProfileIOS* profile = self.profileState.profile;

  if (_skipScreenDecision == SkipScreenDecision::kUnknown) {
    if (ShouldDisplaySearchEngineChoiceScreen(
            *profile, /*is_first_run_entrypoint=*/false,
            [self startupHadExternalIntent])) {
      _skipScreenDecision = SkipScreenDecision::kPresent;
    } else {
      _skipScreenDecision = SkipScreenDecision::kSkip;
    }
  }

  return _skipScreenDecision == SkipScreenDecision::kPresent;
}

// Tries to present the choice screen on `sceneState`. If the screen is not
// presented for any reason, then advance the application init state.
- (void)maybeShowChoiceScreen:(SceneState*)sceneState {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kChoiceScreen);
  DCHECK_EQ(sceneState.activationLevel, SceneActivationLevelForegroundActive);

  // If the Choice Screen is already presented on another SceneState, then
  // there is nothing to do.
  if (!_searchEngineChoiceSceneStateID.empty()) {
    DCHECK(_searchEngineChoiceUIBlocker);
    return;
  }

  DCHECK(!_searchEngineChoiceUIBlocker);
  DCHECK(_searchEngineChoiceSceneStateID.empty());

  if (![self shouldShowChoiceScreen]) {
    // If there is no need to present the screen, then transition to the next
    // application stage (otherwise the transition will happen once the user
    // has selected a default search engine and completed the workflow). In
    // that case, the method won't be called again.
    [self.profileState queueTransitionToNextInitStage];
    return;
  }

  id<SearchEngineChoiceCommands> handler =
      GetSearchEngineChoiceHandler(sceneState);
  if (!handler) {
    [self.profileState queueTransitionToNextInitStage];
    return;
  }

  // Present the screen.
  _searchEngineChoiceSceneStateID = sceneState.sceneSessionID;
  _searchEngineChoiceUIBlocker = ScopedUIBlocker::ProfileScoped(sceneState);

  __weak __typeof(self) weakSelf = self;
  __weak SceneState* weakSceneState = sceneState;
  [handler showSearchEngineChoiceScreenWithCompletion:^{
    [weakSelf choiceScreenClosedForSceneState:weakSceneState];
  }];
}

// Tries to dismiss the choice screen if presented by `sceneState` as the
// SceneState will be disconnected or detached soon. If that `sceneState`
// was presenting the Search Engine Choice Screen, move the presentation
// to the next active SceneState, if any.
- (void)sceneStateDisconnected:(SceneState*)sceneState {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kChoiceScreen);
  if (_searchEngineChoiceSceneStateID != sceneState.sceneSessionID) {
    // Nothing to do if the Search Engine Choice Screen is not presented
    // by `sceneState`.
    return;
  }

  [self stopPresentingChoiceScreenForSceneState:sceneState];
  if (SceneState* nextSceneState = self.profileState.foregroundActiveScene) {
    [self maybeShowChoiceScreen:nextSceneState];
  }
}

// Stops presenting the choice screen. Called after it has been dismissed
// by the user or when programmatically dismissing when a SceneState is
// detached or disconnected while the screen is presented.
- (void)stopPresentingChoiceScreenForSceneState:(SceneState*)sceneState {
  DCHECK(!_searchEngineChoiceSceneStateID.empty());
  DCHECK_EQ(_searchEngineChoiceSceneStateID, sceneState.sceneSessionID);
  _searchEngineChoiceSceneStateID.clear();
  _searchEngineChoiceUIBlocker.reset();

  id<SearchEngineChoiceCommands> handler =
      GetSearchEngineChoiceHandler(sceneState);
  [handler stopSearchEngineChoiceScreen];
}

- (void)choiceScreenClosedForSceneState:(SceneState*)sceneState {
  [self stopPresentingChoiceScreenForSceneState:sceneState];
  [self maybeShowChoiceConfirmationSnackbarForSceneState:sceneState];

  // Advance to the next stage when the screen is dismissed by the user.
  if (self.profileState.initStage == ProfileInitStage::kChoiceScreen) {
    [self.profileState queueTransitionToNextInitStage];
  }
}

// Displays a snackbar confirming the search engine choice if requested by the
// regional capabilities service and `kSearchEngineChoiceScreenSnackbar` is
// enabled.
- (void)maybeShowChoiceConfirmationSnackbarForSceneState:
    (SceneState*)sceneState {
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);
  ProfileIOS* profile = browser->GetProfile();
  regional_capabilities::RegionalCapabilitiesService*
      regionalCapabilitiesService =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  CHECK(regionalCapabilitiesService);
  if (!regionalCapabilitiesService->ShouldShowChoiceConfirmationSnackbar()) {
    return;
  }

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  CHECK(templateURLService);
  const TemplateURL* defaultSearchProvider =
      templateURLService->GetDefaultSearchProvider();
  if (!defaultSearchProvider) {
    return;
  }
  NSString* messageText = l10n_util::GetNSStringF(
      IDS_SEARCH_ENGINE_CHOICE_SETTINGS_CONFIRMATION_TOAST_LABEL,
      defaultSearchProvider->short_name());

  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE);
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  __weak id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  action.handler = ^{
    [settingsHandler showDefaultSearchEngineSettings];
  };

  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageText];
  message.action = action;

  id<SnackbarCommands> snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarHandler showSnackbarMessage:message];
}

@end
