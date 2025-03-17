// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Called by ChangeProfileSignoutContinuation once the sign-out is complete.
void SignoutDone(base::WeakPtr<Browser> weak_browser,
                 bool force_snackbar_over_toolbar,
                 MDCSnackbarMessage* snackbar_message) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  if (!snackbar_message) {
    return;
  }

  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  if (force_snackbar_over_toolbar) {
    [snackbar_commands_handler
        showSnackbarMessageOverBrowserToolbar:snackbar_message];
  } else {
    [snackbar_commands_handler showSnackbarMessage:snackbar_message
                                      bottomOffset:0];
  }
}

// Implementation of the continuation that sign-out the profile.
void ChangeProfileSignoutContinuation(
    signin_metrics::ProfileSignout signout_source_metric,
    BOOL force_snackbar_over_toolbar,
    BOOL should_record_metrics,
    MDCSnackbarMessage* snackbar_message,
    base::OnceClosure signout_completion,
    SceneState* scene_state,
    base::OnceClosure closure) {
  // The regular browser should be used to complete the signout, even if in
  // incognito mode.
  Browser* browser =
      scene_state.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);

  // Create the closure corresponding to the action to perform once the signout
  // action completes, chaining `signout_completion` and `closure`.
  base::OnceClosure completion =
      base::BindOnce(&SignoutDone, browser->AsWeakPtr(),
                     force_snackbar_over_toolbar, snackbar_message)
          .Then(std::move(signout_completion))
          .Then(std::move(closure));

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());
  authentication_service->SignOut(signout_source_metric,
                                  base::CallbackToBlock(std::move(completion)));

  if (should_record_metrics) {
    // TODO(crbug.com/40066949): Remove buckets related to sync-the-feature, and
    // maybe rename histogram.
    signin_metrics::RecordSignoutForceClearDataChoice(
        /*force_clear_data=*/false);
    signin_metrics::RecordSignoutUserAction(/*force_clear_data=*/false);
  }
}

// Implementation of the continuation that shows the force sign out prompt after
// sign out due to `BrowserSignin` policy being set to disabled.
void ChangeProfileForceSignoutContinuation(SceneState* scene_state,
                                           base::OnceClosure closure) {
  BOOL scene_is_active =
      scene_state.activationLevel >= SceneActivationLevelForegroundActive;
  if (scene_is_active) {
    // Try to show the signout prompt in all cases: if there is a sign
    // in in progress, the UI will prevent the prompt from showing.
    Browser* browser =
        scene_state.browserProviderInterface.currentBrowserProvider.browser;
    CHECK(browser);
    // TODO(crbug.com/364574533):Dismiss in-progress signin here and show the
    // prompt in the callback of its completion. This requires a new
    // ApplicationCommands handler method to call
    // SceneController::interruptSigninCoordinatorAnimated or directly show the
    // force sign out prompt.
    id<PolicyChangeCommands> policy_change_handler = HandlerForProtocol(
        browser->GetCommandDispatcher(), PolicyChangeCommands);
    [policy_change_handler showForceSignedOutPrompt];
  } else {
    scene_state.profileState.shouldShowForceSignOutPrompt = YES;
  }

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileSignoutContinuation(
    signin_metrics::ProfileSignout signout_source_metric,
    BOOL force_snackbar_over_toolbar,
    BOOL should_record_metrics,
    MDCSnackbarMessage* snackbar_message,
    ProceduralBlock signout_completion) {
  return base::BindOnce(&ChangeProfileSignoutContinuation,
                        signout_source_metric, force_snackbar_over_toolbar,
                        should_record_metrics, snackbar_message,
                        signout_completion ? base::BindOnce(signout_completion)
                                           : base::DoNothing());
}

ChangeProfileContinuation CreateChangeProfileForceSignoutContinuation() {
  return base::BindOnce(&ChangeProfileForceSignoutContinuation);
}
