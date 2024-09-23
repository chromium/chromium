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

bool WebViewPasswordFeatureManager::IsOptedInForAccountStorage() const {
  // Although ios/web_view will only write to the account store, this should
  // still be controlled on a per user basis to ensure that the logged out user
  // remains opted out.
  return password_manager::features_util::IsOptedInForAccountStorage(
      pref_service_, sync_service_);
}

bool WebViewPasswordFeatureManager::ShouldShowAccountStorageOptIn() const {
  return false;
}

bool WebViewPasswordFeatureManager::ShouldShowAccountStorageReSignin(
    const GURL& current_page_url) const {
  return false;
}

bool WebViewPasswordFeatureManager::ShouldShowAccountStorageBubbleUi() const {
  return false;
}

password_manager::PasswordForm::Store
WebViewPasswordFeatureManager::GetDefaultPasswordStore() const {
  // ios/web_view should never write to the profile password store.
  return password_manager::PasswordForm::Store::kAccountStore;
}

bool WebViewPasswordFeatureManager::IsDefaultPasswordStoreSet() const {
  return false;
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
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace ios_web_view
