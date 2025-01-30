// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_feature_manager.h"

#import "base/notreached.h"
#import "components/password_manager/core/browser/features/password_manager_features_util.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"

namespace ios_web_view {
WebViewPasswordFeatureManager::WebViewPasswordFeatureManager(
    PrefService* pref_service,
    const syncer::SyncService* sync_service)
    : pref_service_(pref_service), sync_service_(sync_service) {}

bool WebViewPasswordFeatureManager::IsGenerationEnabled() const {
  return true;
}

bool WebViewPasswordFeatureManager::IsAccountStorageEnabled() const {
  // Although ios/web_view will only write to the account store, this should
  // still be controlled on a per user basis to ensure that the logged out user
  // remains with account storage disabled.
  return password_manager::features_util::IsAccountStorageEnabled(
      pref_service_, sync_service_);
}

password_manager::features_util::PasswordAccountStorageUsageLevel
WebViewPasswordFeatureManager::ComputePasswordAccountStorageUsageLevel() const {
  // ios/web_view doesn't support either the profile password store or sync, so
  // the account-scoped storage is the only option.
  return password_manager::features_util::PasswordAccountStorageUsageLevel::
      kUsingAccountStorage;
}

bool WebViewPasswordFeatureManager::
    IsBiometricAuthenticationBeforeFillingEnabled() const {
  // This feature is related only to MacOS and Windows, this function
  // shouldn't be called on iOS.
  NOTREACHED();
}

}  // namespace ios_web_view
