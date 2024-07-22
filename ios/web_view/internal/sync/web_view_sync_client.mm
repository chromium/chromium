// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#import <algorithm>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "components/plus_addresses/webdata/plus_address_webdata_service.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/model/model_type_store_service.h"
#import "components/sync/service/model_type_controller.h"
#import "components/sync/service/sync_api_component_factory.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/version_info/version_info.h"
#import "components/version_info/version_string.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

namespace ios_web_view {

namespace {
syncer::ModelTypeSet GetDisabledTypes() {
  syncer::ModelTypeSet disabled_types = syncer::UserTypes();
  disabled_types.Remove(syncer::AUTOFILL);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_DATA);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_METADATA);
  disabled_types.Remove(syncer::AUTOFILL_PROFILE);
  disabled_types.Remove(syncer::PASSWORDS);
  return disabled_types;
}
}  // namespace

// static
std::unique_ptr<WebViewSyncClient> WebViewSyncClient::Create(
    WebViewBrowserState* browser_state) {
  return std::make_unique<WebViewSyncClient>(
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewProfilePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS)
          .get(),
      browser_state->GetPrefs(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      WebViewModelTypeStoreServiceFactory::GetForBrowserState(browser_state),
      WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(browser_state),
      WebViewSyncInvalidationsServiceFactory::GetForBrowserState(
          browser_state));
}

WebViewSyncClient::WebViewSyncClient(
    autofill::AutofillWebDataService* profile_web_data_service,
    autofill::AutofillWebDataService* account_web_data_service,
    password_manager::PasswordStoreInterface* profile_password_store,
    password_manager::PasswordStoreInterface* account_password_store,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::ModelTypeStoreService* model_type_store_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    syncer::SyncInvalidationsService* sync_invalidations_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_invalidations_service_(sync_invalidations_service) {
  component_factory_ =
      std::make_unique<browser_sync::SyncApiComponentFactoryImpl>(
          this, device_info_sync_service->GetDeviceInfoTracker(),
          model_type_store_service->GetSyncDataPath());

  // TODO(crbug.com/40264840): introduce ios webview version of
  // TrustedVaultServiceFactory.
  trusted_vault_client_ = std::make_unique<WebViewTrustedVaultClient>();

  controller_builder_.SetAutofillWebDataService(
      web::GetUIThreadTaskRunner({}),
      profile_web_data_service->GetDBTaskRunner(), profile_web_data_service,
      account_web_data_service);
  controller_builder_.SetBookmarkModel(nullptr);
  controller_builder_.SetBookmarkSyncService(nullptr, nullptr);
  controller_builder_.SetConsentAuditor(nullptr);
  controller_builder_.SetDataSharingService(nullptr);
  controller_builder_.SetDeviceInfoSyncService(device_info_sync_service);
  controller_builder_.SetDualReadingListModel(nullptr);
  controller_builder_.SetFaviconService(nullptr);
  controller_builder_.SetGoogleGroupsManager(nullptr);
  controller_builder_.SetHistoryService(nullptr);
  controller_builder_.SetIdentityManager(identity_manager);
  controller_builder_.SetModelTypeStoreService(model_type_store_service);
  controller_builder_.SetPasskeyModel(nullptr);
  controller_builder_.SetPasswordReceiverService(nullptr);
  controller_builder_.SetPasswordSenderService(nullptr);
  controller_builder_.SetPasswordStore(profile_password_store,
                                       account_password_store);
  controller_builder_.SetPlusAddressServices(nullptr, nullptr);
  controller_builder_.SetPowerBookmarkService(nullptr);
  controller_builder_.SetPrefService(pref_service_);
  controller_builder_.SetPrefServiceSyncable(nullptr);
  // TODO(crbug.com/330201909) implement for iOS.
  controller_builder_.SetProductSpecificationsService(nullptr);
  controller_builder_.SetSendTabToSelfSyncService(nullptr);
  controller_builder_.SetSessionSyncService(nullptr);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controller_builder_.SetSupervisedUserSettingsService(nullptr);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controller_builder_.SetUserEventService(nullptr);
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

syncer::ModelTypeController::TypeVector
WebViewSyncClient::CreateModelTypeControllers(
    syncer::SyncService* sync_service) {
  return controller_builder_.Build(GetDisabledTypes(), sync_service,
                                   version_info::Channel::STABLE);
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

syncer::SyncApiComponentFactory*
WebViewSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
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
