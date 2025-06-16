// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_authentication_continuation.h"

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Callback for SystemIdentityManager::IterateOverIdentities().
SystemIdentityManager::IteratorResult IdentitiesOnDevice(
    NSMutableArray<id<SystemIdentity>>* identities,
    id<SystemIdentity> identity) {
  [identities addObject:identity];
  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

void ChangeProfileSignoutCompletion(base::WeakPtr<Browser> weak_browser,
                                    NSSet<UIOpenURLContext*>* contexts,
                                    base::OnceClosure closure) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }
  browser->GetSceneState().URLContextsToOpen = contexts;
  std::move(closure).Run();
}

// Sign out and open URL contexts.
void SignoutAndOpenContexts(Browser* browser,
                            NSSet<UIOpenURLContext*>* contexts,
                            AuthenticationService* authentication_service,
                            base::OnceClosure closure) {
  base::OnceClosure completion =
      base::BindOnce(&ChangeProfileSignoutCompletion, browser->AsWeakPtr(),
                     contexts, std::move(closure));

  authentication_service->SignOut(
      signin_metrics::ProfileSignout::kSignoutFromWidgets,
      base::CallbackToBlock(std::move(completion)));
}

// Sign in profile to open widget context.
void SigninForContext(WidgetContext* context,
                      NSSet<UIOpenURLContext*>* contexts,
                      AuthenticationService* authentication_service,
                      SceneState* scene_state,
                      base::OnceClosure closure) {
  // Iterate over all identities on device because the newGaia could
  // be in a different profile.
  id<SystemIdentity> newIdentity;
  NSMutableArray<id<SystemIdentity>>* identities =
      [[NSMutableArray alloc] init];
  GetApplicationContext()->GetSystemIdentityManager()->IterateOverIdentities(
      base::BindRepeating(&IdentitiesOnDevice, identities));
  for (id<SystemIdentity> identity in identities) {
    if ([identity.gaiaID isEqualToString:context.gaiaID]) {
      newIdentity = identity;
    }
  }
  // Don't perform sign-in if the new identity is not found.
  if (!newIdentity) {
    std::move(closure).Run();
    return;
  }

  authentication_service->SignIn(newIdentity,
                                 signin_metrics::AccessPoint::kWidget);
  scene_state.URLContextsToOpen = contexts;
  std::move(closure).Run();
}

// Implementation of the continuation that starts the sign-in or sign-out flow.
void ChangeProfileAuthenticationContinuation(WidgetContext* context,
                                             NSSet<UIOpenURLContext*>* contexts,
                                             SceneState* scene_state,
                                             base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());

  if (context.type == AccountSwitchType::kSignOut) {
    // Perform sign-out only if there is a signed-in account in the profile.
    if (authentication_service->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      SignoutAndOpenContexts(browser, contexts, authentication_service,
                             std::move(closure));
    } else {
      scene_state.URLContextsToOpen = contexts;
      std::move(closure).Run();
    }
  } else {
    if (!authentication_service->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      SigninForContext(context, contexts, authentication_service, scene_state,
                       std::move(closure));
    } else if (authentication_service
                   ->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
                   .gaiaID != context.gaiaID) {
      base::OnceClosure completion = base::BindOnce(
          &SigninForContext, context, contexts, authentication_service,
          scene_state, std::move(closure));
      authentication_service->SignOut(
          signin_metrics::ProfileSignout::kSignoutFromWidgets,
          base::CallbackToBlock(std::move(completion)));
    } else {
      scene_state.URLContextsToOpen = contexts;
      std::move(closure).Run();
    }
  }
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileAuthenticationContinuation(
    WidgetContext* context,
    NSSet<UIOpenURLContext*>* contexts) {
  return base::BindOnce(&ChangeProfileAuthenticationContinuation, context,
                        contexts);
}
