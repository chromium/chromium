// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"

#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/browser_sync/common_controller_builder.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/metrics/demographics/user_demographics.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "components/plus_addresses/webdata/plus_address_webdata_service.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/data_type_controller.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "components/version_info/version_info.h"
#import "components/version_info/version_string.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_data_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_client.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace ios_web_view {
namespace {

syncer::DataTypeSet GetDisabledTypes() {
  syncer::DataTypeSet disabled_types = syncer::UserTypes();
  disabled_types.Remove(syncer::AUTOFILL);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_DATA);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_METADATA);
  disabled_types.Remove(syncer::AUTOFILL_PROFILE);
  disabled_types.Remove(syncer::PASSWORDS);
  return disabled_types;
}

}  // namespace

syncer::DataTypeController::TypeVector CreateControllers(
    WebViewBrowserState* browser_state,
    syncer::SyncService* sync_service) {
  scoped_refptr<autofill::AutofillWebDataService>
      autofill_profile_web_data_service = WebViewWebDataServiceWrapperFactory::
          GetAutofillWebDataForBrowserState(browser_state,
                                            ServiceAccessType::IMPLICIT_ACCESS);

  browser_sync::CommonControllerBuilder controller_builder;

  controller_builder.SetAutofillWebDataService(
      web::GetUIThreadTaskRunner({}),
      autofill_profile_web_data_service,
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS));
  controller_builder.SetDeviceInfoSyncService(
      WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(browser_state));
  controller_builder.SetIdentityManager(
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state));
  controller_builder.SetDataTypeStoreService(
      WebViewDataTypeStoreServiceFactory::GetForBrowserState(browser_state));
  controller_builder.SetPasswordStore(
      WebViewProfilePasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS),
      WebViewAccountPasswordStoreFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS));
  controller_builder.SetPrefService(browser_state->GetPrefs());

  // Unused.
  controller_builder.SetBookmarkModel(nullptr);
  controller_builder.SetBookmarkSyncService(nullptr, nullptr);
  controller_builder.SetConsentAuditor(nullptr);
  controller_builder.SetDataSharingService(nullptr);
  controller_builder.SetDualReadingListModel(nullptr);
  controller_builder.SetFaviconService(nullptr);
  controller_builder.SetGoogleGroupsManager(nullptr);
  controller_builder.SetHistoryService(nullptr);
  controller_builder.SetPasskeyModel(nullptr);
  controller_builder.SetPasswordReceiverService(nullptr);
  controller_builder.SetPasswordSenderService(nullptr);
  controller_builder.SetPlusAddressServices(nullptr, nullptr);
  controller_builder.SetPowerBookmarkService(nullptr);
  controller_builder.SetPrefServiceSyncable(nullptr);
  // TODO(crbug.com/330201909) implement for iOS.
  controller_builder.SetProductSpecificationsService(nullptr);
  controller_builder.SetSendTabToSelfSyncService(nullptr);
  controller_builder.SetSessionSyncService(nullptr);
  controller_builder.SetSharingMessageBridge(nullptr);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controller_builder.SetSupervisedUserSettingsService(nullptr);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controller_builder.SetTabGroupSyncService(nullptr);
  controller_builder.SetTemplateURLService(nullptr);
  controller_builder.SetUserEventService(nullptr);

  return controller_builder.Build(GetDisabledTypes(), sync_service,
                                  version_info::Channel::STABLE);
}

// static
WebViewSyncServiceFactory* WebViewSyncServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewSyncServiceFactory> instance;
  return instance.get();
}

// static
syncer::SyncService* WebViewSyncServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewSyncServiceFactory::WebViewSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The SyncServiceImpl depends on various KeyedServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order. Note that some of the dependencies are listed here but
  // actually plumbed in WebViewSyncClient, which this factory constructs.
  DependsOn(WebViewDataTypeStoreServiceFactory::GetInstance());
  DependsOn(WebViewDeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
  DependsOn(WebViewAccountPasswordStoreFactory::GetInstance());
  DependsOn(WebViewProfilePasswordStoreFactory::GetInstance());
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
  DependsOn(WebViewSyncInvalidationsServiceFactory::GetInstance());
}

WebViewSyncServiceFactory::~WebViewSyncServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.sync_client = std::make_unique<WebViewSyncClient>(
      browser_state->GetPrefs(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      WebViewDataTypeStoreServiceFactory::GetForBrowserState(browser_state),
      WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(browser_state),
      WebViewSyncInvalidationsServiceFactory::GetForBrowserState(
          browser_state));
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker();
  init_params.channel = version_info::Channel::STABLE;

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize(
      CreateControllers(browser_state, sync_service.get()));
  return sync_service;
}

}  // namespace ios_web_view
