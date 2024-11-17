// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_identity_util.h"

#import "base/check.h"
#import "components/signin/public/identity_manager/account_info.h"

id<SystemIdentity> GetPrimarySystemIdentity(
    signin::ConsentLevel consent_level,
    signin::IdentityManager* identity_manager,
    ChromeAccountManagerService* account_manager) {
  CHECK(identity_manager);
  CHECK(account_manager);
  if (!identity_manager->HasPrimaryAccount(consent_level)) {
    return nil;
  }

  // TODO(crbug.com/376046766): if HasPrimaryAccount(...) return true, then
  // the CoreAccountInfo returned by GetPrimaryAccountInfo(...) should be
  // valid. This return should be a CHECK(...).
  const CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  if (account_info.gaia.empty()) {
    return nil;
  }

  return account_manager->GetIdentityWithGaiaID(account_info.gaia);
}
