// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsRestrictAccountsToPatternsEnabled() {
  const base::ListValue* value =
      GetApplicationContext()->GetLocalState()->GetList(
          prefs::kRestrictAccountsToPatterns);
  return !value->GetList().empty();
}

// TODO(crbug.com/1244632): Use the Authentication Service sign-in status API
// instead of this when available.
bool IsForceSignInEnabled() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  return policy_mode == BrowserSigninMode::kForced;
}

EnterpriseSignInRestrictions GetEnterpriseSignInRestrictions() {
  EnterpriseSignInRestrictions restrictions = kNoEnterpriseRestriction;
  if (IsForceSignInEnabled())
    restrictions |= kEnterpriseForceSignIn;
  if (IsRestrictAccountsToPatternsEnabled())
    restrictions |= kEnterpriseRestrictAccounts;
  return restrictions;
}
