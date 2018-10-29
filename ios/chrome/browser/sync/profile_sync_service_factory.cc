// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/profile_sync_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_util.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_deprecated_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#include "ios/chrome/browser/signin/about_signin_internals_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

using browser_sync::ProfileSyncService;

namespace {

void UpdateNetworkTimeOnUIThread(base::Time network_time,
                                 base::TimeDelta resolution,
                                 base::TimeDelta latency,
                                 base::TimeTicks post_time) {
  GetApplicationContext()->GetNetworkTimeTracker()->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkTime(const base::Time& network_time,
                       const base::TimeDelta& resolution,
                       const base::TimeDelta& latency) {
  base::PostTaskWithTraits(
      FROM_HERE, {web::WebThread::UI},
      base::Bind(&UpdateNetworkTimeOnUIThread, network_time, resolution,
                 latency, base::TimeTicks::Now()));
}

}  // namespace

// static
ProfileSyncServiceFactory* ProfileSyncServiceFactory::GetInstance() {
  return base::Singleton<ProfileSyncServiceFactory>::get();
}

// static
ProfileSyncService* ProfileSyncServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  if (!ProfileSyncService::IsSyncAllowedByFlag())
    return nullptr;

  return static_cast<ProfileSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ProfileSyncService* ProfileSyncServiceFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  if (!ProfileSyncService::IsSyncAllowedByFlag())
    return nullptr;

  return static_cast<ProfileSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

ProfileSyncServiceFactory::ProfileSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ProfileSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The ProfileSyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(ios::AboutSigninInternalsFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::BookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromePasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeProfileInvalidationProviderFactory::GetInstance());
  DependsOn(
      IOSChromeDeprecatedProfileInvalidationProviderFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

ProfileSyncServiceFactory::~ProfileSyncServiceFactory() {}

std::unique_ptr<KeyedService>
ProfileSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  // Always create the GCMProfileService instance such that we can listen to
  // the profile notifications and purge the GCM store when the profile is
  // being signed out.
  IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state);

  // TODO(crbug.com/171406): Change AboutSigninInternalsFactory to load on
  // startup once bug has been fixed.
  ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);

  ProfileSyncService::InitParams init_params;
  init_params.identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  init_params.signin_scoped_device_id_callback = base::BindRepeating(
      &signin::GetSigninScopedDeviceId, browser_state->GetPrefs());
  init_params.start_behavior = ProfileSyncService::MANUAL_START;
  init_params.sync_client =
      std::make_unique<IOSChromeSyncClient>(browser_state);
  init_params.network_time_update_callback = base::Bind(&UpdateNetworkTime);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      GetApplicationContext()->GetNetworkConnectionTracker();
  init_params.debug_identifier = browser_state->GetDebugName();
  init_params.channel = ::GetChannel();
  init_params.user_events_separate_pref_group =
      unified_consent::IsUnifiedConsentFeatureEnabled();

  bool use_fcm_invalidations =
      base::FeatureList::IsEnabled(invalidation::switches::kFCMInvalidations);
  if (use_fcm_invalidations) {
    auto* fcm_invalidation_provider =
        IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
            browser_state);
    if (fcm_invalidation_provider) {
      init_params.invalidations_identity_providers.push_back(
          fcm_invalidation_provider->GetIdentityProvider());
    }
  }
  // This code should stay here until all invalidation client are
  // migrated from deprecated invalidation  infructructure.
  // Since invalidations will work only if ProfileSyncService calls
  // SetActiveAccountId for all identity providers.
  auto* deprecated_invalidation_provider =
      IOSChromeDeprecatedProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state);
  if (deprecated_invalidation_provider) {
    init_params.invalidations_identity_providers.push_back(
        deprecated_invalidation_provider->GetIdentityProvider());
  }

  auto pss = std::make_unique<ProfileSyncService>(std::move(init_params));
  pss->Initialize();
  return pss;
}
