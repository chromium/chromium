// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/browser_state_keyed_service_factories.h"

#import "ios/web_view/internal/affiliations/web_view_affiliation_service_factory.h"
#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/language/web_view_accept_languages_service_factory.h"
#import "ios/web_view/internal/language/web_view_language_model_manager_factory.h"
#import "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_bulk_leak_check_service_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_requirements_service_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_reuse_manager_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#import "ios/web_view/internal/safe_browsing/web_view_safe_browsing_client_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#import "ios/web_view/internal/sync/web_view_data_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_instance_id_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#import "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#import "ios/web_view/internal/webui/web_view_web_ui_ios_controller_factory.h"

namespace ios_web_view {

void EnsureBrowserStateKeyedServiceFactoriesBuilt() {
  autofill::WebViewAutofillLogRouterFactory::GetInstance();
  WebViewAcceptLanguagesServiceFactory::GetInstance();
  WebViewAccountPasswordStoreFactory::GetInstance();
  WebViewAffiliationServiceFactory::GetInstance();
  WebViewAutocompleteHistoryManagerFactory::GetInstance();
  WebViewBulkLeakCheckServiceFactory::GetInstance();
  WebViewDataTypeStoreServiceFactory::GetInstance();
  WebViewDeviceInfoSyncServiceFactory::GetInstance();
  WebViewGCMProfileServiceFactory::GetInstance();
  WebViewIdentityManagerFactory::GetInstance();
  WebViewInstanceIDProfileServiceFactory::GetInstance();
  WebViewLanguageModelManagerFactory::GetInstance();
  WebViewPasswordManagerLogRouterFactory::GetInstance();
  WebViewPasswordRequirementsServiceFactory::GetInstance();
  WebViewPasswordReuseManagerFactory::GetInstance();
  WebViewPersonalDataManagerFactory::GetInstance();
  WebViewProfileInvalidationProviderFactory::GetInstance();
  WebViewProfilePasswordStoreFactory::GetInstance();
  WebViewSafeBrowsingClientFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
  WebViewStrikeDatabaseFactory::GetInstance();
  WebViewSyncInvalidationsServiceFactory::GetInstance();
  WebViewSyncServiceFactory::GetInstance();
  WebViewTranslateRankerFactory::GetInstance();
  WebViewUrlLanguageHistogramFactory::GetInstance();
  WebViewWebDataServiceWrapperFactory::GetInstance();
  WebViewWebUIIOSControllerFactory::GetInstance();
}

}  // namespace ios_web_view
