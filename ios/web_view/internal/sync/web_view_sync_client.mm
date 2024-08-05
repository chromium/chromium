// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#import <algorithm>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/service/sync_engine_factory.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_data_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

namespace ios_web_view {

WebViewSyncClient::WebViewSyncClient(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::DataTypeStoreService* data_type_store_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_invalidations_service_(sync_invalidations_service) {
  engine_factory_ = std::make_unique<browser_sync::SyncEngineFactoryImpl>(
      this, device_info_sync_service->GetDeviceInfoTracker(),
      data_type_store_service->GetSyncDataPath());

  // TODO(crbug.com/40264840): introduce ios webview version of
  // TrustedVaultServiceFactory.
  trusted_vault_client_ = std::make_unique<WebViewTrustedVaultClient>();
}

WebViewSyncClient::~WebViewSyncClient() {}

PrefService* WebViewSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return pref_service_;
}

signin::IdentityManager* WebViewSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return identity_manager_;
}

base::FilePath WebViewSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::SyncInvalidationsService*
WebViewSyncClient::GetSyncInvalidationsService() {
  return sync_invalidations_service_;
}

trusted_vault::TrustedVaultClient* WebViewSyncClient::GetTrustedVaultClient() {
  return trusted_vault_client_.get();
}

scoped_refptr<syncer::ExtensionsActivity>
WebViewSyncClient::GetExtensionsActivity() {
  return nullptr;
}

syncer::SyncEngineFactory* WebViewSyncClient::GetSyncEngineFactory() {
  return engine_factory_.get();
}

bool WebViewSyncClient::IsCustomPassphraseAllowed() {
  return true;
}

bool WebViewSyncClient::IsPasswordSyncAllowed() {
  return true;
}

void WebViewSyncClient::SetPasswordSyncAllowedChangeCb(
    const base::RepeatingClosure& cb) {
  // IsPasswordSyncAllowed() doesn't change on //ios/web_view/.
}

void WebViewSyncClient::RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
    const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup& group) {
  // This code might be reached but synthetic field trials are not supported on
  // iOS webview.
}
}  // namespace ios_web_view
