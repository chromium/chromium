// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"

#import "ios/chrome/browser/signin/model/authentication_service.h"

bool CanFetchUserPolicy(AuthenticationService* authService,
                        PrefService* prefService) {
  // Return true if the primary identity is managed.
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}
