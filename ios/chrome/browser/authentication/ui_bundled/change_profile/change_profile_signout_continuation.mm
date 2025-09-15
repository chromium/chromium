// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
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

// Completion for ChangeProfileSignoutContinuation(...) that presents the
// snackback message (if non-null), invoke the signout completion and then
// the closure.
void ChangeProfileSignoutCompletion(
    base::WeakPtr<Browser> weak_browser,
    SnackbarMessage* snackbar_message,
    bool force_snackbar_over_toolbar,
    SignoutCompletionCallback signout_completion,
    base::OnceClosure closure) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  if (snackbar_message) {
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

  std::move(signout_completion).Run(browser->GetSceneState());
  std::move(closure).Run();
}

// Implementation of the continuation that sign-out the profile.
void ChangeProfileSignoutContinuation(
    signin_metrics::ProfileSignout signout_source_metric,
    BOOL force_snackbar_over_toolbar,
    BOOL should_record_metrics,
    SnackbarMessage* snackbar_message,
    SignoutCompletionCallback signout_completion,
    SceneState* scene_state,
    base::OnceClosure closure) {
  // The regular browser should be used to complete the signout, even if in
  // incognito mode.
  Browser* browser =
      scene_state.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);

  // Create the final completion that will be invoked when the signout
  // operation completes.
  base::OnceClosure completion =
      base::BindOnce(&ChangeProfileSignoutCompletion, browser->AsWeakPtr(),
                     snackbar_message, force_snackbar_over_toolbar,
                     std::move(signout_completion), std::move(closure));

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());
  authentication_service->SignOut(signout_source_metric,
                                  base::CallbackToBlock(std::move(completion)));

  // TODO(crbug.com/406274746): Consider removing `should_record_metrics`.
  if (should_record_metrics) {
    base::RecordAction(base::UserMetricsAction("Signin_Signout"));
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
    SnackbarMessage* snackbar_message,
    SignoutCompletionCallback signout_completion) {
  CHECK(!signout_completion.is_null());
  return base::BindOnce(&ChangeProfileSignoutContinuation,
                        signout_source_metric, force_snackbar_over_toolbar,
                        should_record_metrics, snackbar_message,
                        std::move(signout_completion));
}

ChangeProfileContinuation CreateChangeProfileForceSignoutContinuation() {
  return base::BindOnce(&ChangeProfileForceSignoutContinuation);
}
