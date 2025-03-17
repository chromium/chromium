// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"

#import <optional>

#import "base/functional/callback.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

bool IsRestrictAccountsToPatternsEnabled() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->GetList(prefs::kRestrictAccountsToPatterns)
              .empty();
}

std::optional<BOOL> IsIdentityManaged(id<SystemIdentity> identity) {
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();
  NSString* hosted_domain =
      system_identity_manager->GetCachedHostedDomainForIdentity(identity);
  if (hosted_domain != nil) {
    return hosted_domain.length > 0;
  }
  return std::nullopt;
}

void FetchManagedStatusForIdentity(
    id<SystemIdentity> identity,
    base::OnceCallback<void(BOOL)> management_status_callback) {
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();

  system_identity_manager->GetHostedDomain(
      identity,
      base::BindOnce(
          ^(base::OnceCallback<void(BOOL)> callback, NSString* hostedDomain,
            NSError* error) {
            std::move(callback).Run(!error && hostedDomain.length > 0);
          },
          std::move(management_status_callback)));
}
