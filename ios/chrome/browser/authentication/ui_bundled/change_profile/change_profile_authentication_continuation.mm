// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_authentication_continuation.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "google_apis/gaia/gaia_id.h"
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

// Completion callback for sign-out during profile change.
// If `contexts` is not nil, sets `URLContextsToOpen` on the scene state of
// `weak_browser` to `contexts` so the URLs are opened once sign-out completes.
void ChangeProfileSignoutCompletion(base::WeakPtr<Browser> weak_browser,
                                    NSSet<UIOpenURLContext*>* contexts,
                                    base::OnceClosure closure) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  if (contexts) {
    browser->GetSceneState().URLContextsToOpen = contexts;
  }
  std::move(closure).Run();
}

// Signs out and opens `contexts` if `contexts` is not nil.
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

// Signs in to the profile to open `context`.
// If `contexts` is not nil, `context.context` must belong to `contexts`, and
// `scene_state.URLContextsToOpen` will be set to `contexts` upon completion.
void SigninForContext(URLContext* context,
                      NSSet<UIOpenURLContext*>* contexts,
                      AuthenticationService* authentication_service,
                      SceneState* scene_state,
                      base::OnceClosure closure) {
  // Sign-in can become disabled due to this method being executed
  // asynchronously. Don't perform sign-in if sign-in is disabled.
  if (!authentication_service->SigninEnabled()) {
    std::move(closure).Run();
    return;
  }
  CHECK(!contexts || [contexts containsObject:context.context],
        base::NotFatalUntil::M157);

  // Iterate over all identities on device because the newGaia could
  // be in a different profile.
  id<SystemIdentity> new_identity = nil;
  NSMutableArray<id<SystemIdentity>>* identities =
      [[NSMutableArray alloc] init];
  GetApplicationContext()->GetSystemIdentityManager()->IterateOverIdentities(
      base::BindRepeating(&IdentitiesOnDevice, identities));
  for (id<SystemIdentity> identity in identities) {
    if (identity.gaiaId == context.gaiaID) {
      new_identity = identity;
    }
  }
  // Don't perform sign-in if the new identity is not found.
  if (!new_identity) {
    std::move(closure).Run();
    return;
  }

  authentication_service->SignIn(new_identity,
                                 signin_metrics::AccessPoint::kWidget);
  if (contexts) {
    scene_state.URLContextsToOpen = contexts;
  }
  std::move(closure).Run();
}

// Implementation of the continuation that starts the sign-in or sign-out flow.
// If `contexts` is not nil, `context.context` must belong to `contexts`, and
// `scene_state.URLContextsToOpen` will be set to `contexts` upon completion.
void ChangeProfileAuthenticationContinuation(URLContext* context,
                                             NSSet<UIOpenURLContext*>* contexts,
                                             SceneState* scene_state,
                                             base::OnceClosure closure) {
  CHECK(!contexts || [contexts containsObject:context.context],
        base::NotFatalUntil::M157);

  Browser* browser =
      scene_state.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);

  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());

  if (context.type == AccountSwitchType::kSignOut) {
    // Perform sign-out only if there is a signed-in account in the profile.
    if (authentication_service->HasPrimaryIdentity()) {
      SignoutAndOpenContexts(browser, contexts, authentication_service,
                             std::move(closure));
    } else {
      if (contexts) {
        scene_state.URLContextsToOpen = contexts;
      }
      std::move(closure).Run();
    }
  } else {
    if (!authentication_service->HasPrimaryIdentity()) {
      SigninForContext(context, contexts, authentication_service, scene_state,
                       std::move(closure));
    } else if (context.gaiaID !=
                   authentication_service->GetPrimaryIdentity().gaiaId &&
               !authentication_service->HasPrimaryIdentityManaged()) {
      base::OnceClosure completion = base::BindOnce(
          &SigninForContext, context, contexts, authentication_service,
          scene_state, std::move(closure));
      authentication_service->SignOut(
          signin_metrics::ProfileSignout::kSignoutFromWidgets,
          base::CallbackToBlock(std::move(completion)));
    } else {
      if (contexts) {
        scene_state.URLContextsToOpen = contexts;
      }
      std::move(closure).Run();
    }
  }
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileAuthenticationContinuation(
    URLContext* context,
    NSSet<UIOpenURLContext*>* contexts) {
  CHECK(!contexts || [contexts containsObject:context.context],
        base::NotFatalUntil::M157);
  return base::BindOnce(&ChangeProfileAuthenticationContinuation, context,
                        contexts);
}
