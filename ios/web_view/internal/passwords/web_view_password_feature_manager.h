// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_feature_manager.h"

namespace syncer {
class SyncService;
}  // namespace syncer

class PrefService;

namespace ios_web_view {
// An //ios/web_view implementation of password_manager::PasswordFeatureManager.
class WebViewPasswordFeatureManager
    : public password_manager::PasswordFeatureManager {
 public:
  WebViewPasswordFeatureManager(PrefService* pref_service,
                                const syncer::SyncService* sync_service);

  WebViewPasswordFeatureManager(const WebViewPasswordFeatureManager&) = delete;
  WebViewPasswordFeatureManager& operator=(
      const WebViewPasswordFeatureManager&) = delete;

  ~WebViewPasswordFeatureManager() override = default;

  bool IsGenerationEnabled() const override;
  bool IsOptedInForAccountStorage() const override;
  bool ShouldShowAccountStorageOptIn() const override;
  bool ShouldShowAccountStorageReSignin(
      const GURL& current_page_url) const override;

  bool ShouldShowAccountStorageBubbleUi() const override;

  password_manager::PasswordForm::Store GetDefaultPasswordStore()
      const override;
  bool IsDefaultPasswordStoreSet() const override;

  password_manager::features_util::PasswordAccountStorageUsageLevel
  ComputePasswordAccountStorageUsageLevel() const override;

  bool IsBiometricAuthenticationBeforeFillingEnabled() const override;

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<const syncer::SyncService> sync_service_;
};
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_
