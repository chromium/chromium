// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_precondition_checker.h"

#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

base::expected<void, RemoteActorPreconditionError>
CheckRemoteActorPreconditions(ProfileIOS* profile,
                              std::string_view requested_gaia_id) {
  if (ShouldPresentFirstRunExperience()) {
    return base::unexpected(RemoteActorPreconditionError::kFirstRunIncomplete);
  }

  signin::IdentityManager* identity_manager =
      profile ? IdentityManagerFactory::GetForProfile(profile) : nullptr;
  CoreAccountInfo primary_account =
      identity_manager ? identity_manager->GetPrimaryAccountInfo(
                             signin::ConsentLevel::kSignin)
                       : CoreAccountInfo();
  if (primary_account.gaia.empty()) {
    return base::unexpected(RemoteActorPreconditionError::kNotSignedIn);
  }

  if (requested_gaia_id.empty() ||
      primary_account.gaia.ToString() != requested_gaia_id) {
    return base::unexpected(RemoteActorPreconditionError::kProfileMismatch);
  }

  return base::ok();
}

RemoteActorPreconditionCallbackParams GetPreconditionFailureCallbackParams(
    RemoteActorPreconditionError error) {
  switch (error) {
    case RemoteActorPreconditionError::kFirstRunIncomplete:
    case RemoteActorPreconditionError::kNotSignedIn:
      return {.status = RemoteActorCallbackStatus::kCancelled,
              .reason = RemoteActorCallbackReason::kNotSignedIn};
    case RemoteActorPreconditionError::kProfileMismatch:
      return {.status = RemoteActorCallbackStatus::kFailed,
              .reason = RemoteActorCallbackReason::kProfileMismatch};
  }
}
