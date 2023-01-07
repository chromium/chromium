// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"

#import <string>
#import <utility>

#import "base/bind.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/threading/thread_task_runner_handle.h"
#import "base/time/default_clock.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/image_fetcher/core/image_decoder.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/ntp_snippets/category_rankers/category_ranker.h"
#import "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#import "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#import "components/ntp_snippets/content_suggestions_service.h"
#import "components/ntp_snippets/features.h"
#import "components/ntp_snippets/ntp_snippets_constants.h"
#import "components/ntp_snippets/remote/persistent_scheduler.h"
#import "components/ntp_snippets/remote/remote_suggestions_database.h"
#import "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#import "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#import "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#import "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"
#import "components/ntp_snippets/user_classifier.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/version_info/version_info.h"
#import "google_apis/google_api_keys.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/json_parser/in_process_json_parser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"
#import "net/url_request/url_request_context_getter.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using history::HistoryService;
using image_fetcher::CreateIOSImageDecoder;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcherImpl;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusServiceImpl;
using ntp_snippets::UserClassifier;

namespace ntp_snippets {

std::unique_ptr<KeyedService>
CreateChromeContentSuggestionsServiceWithProviders(
    web::BrowserState* browser_state) {
  auto service =
      ntp_snippets::CreateChromeContentSuggestionsService(browser_state);
  ContentSuggestionsService* suggestions_service =
      static_cast<ContentSuggestionsService*>(service.get());

  if (base::FeatureList::IsEnabled(ntp_snippets::kArticleSuggestionsFeature)) {
    ntp_snippets::RegisterRemoteSuggestionsProvider(suggestions_service,
                                                    browser_state);
  }

  return service;
}

std::unique_ptr<KeyedService> CreateChromeContentSuggestionsService(
    web::BrowserState* browser_state) {
  using State = ContentSuggestionsService::State;
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  PrefService* prefs = chrome_browser_state->GetPrefs();

  auto user_classifier = std::make_unique<UserClassifier>(
      prefs, base::DefaultClock::GetInstance());

  // TODO(crbug.com/676249): Implement a persistent scheduler for iOS.
  auto scheduler = std::make_unique<RemoteSuggestionsSchedulerImpl>(
      /*persistent_scheduler=*/nullptr, user_classifier.get(), prefs,
      GetApplicationContext()->GetLocalState(),
      base::DefaultClock::GetInstance());

  // Create the ContentSuggestionsService.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state);
  HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          chrome_browser_state);
  std::unique_ptr<ntp_snippets::CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          prefs, base::DefaultClock::GetInstance());
  return std::make_unique<ContentSuggestionsService>(
      State::ENABLED, identity_manager, history_service, large_icon_service,
      prefs, std::move(category_ranker), std::move(user_classifier),
      std::move(scheduler));
}

void RegisterRemoteSuggestionsProvider(ContentSuggestionsService* service,
                                       web::BrowserState* browser_state) {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  PrefService* prefs = chrome_browser_state->GetPrefs();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      browser_state->GetRequestContext();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      browser_state->GetSharedURLLoaderFactory();

  base::FilePath database_dir(
      browser_state->GetStatePath().Append(ntp_snippets::kDatabaseFolder));

  std::string api_key;
  // This API needs allow-listed API keys. Get the key only if it is not a
  // dummy key.
  if (google_apis::HasAPIKeyConfigured()) {
    bool is_stable_channel = GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }
  auto suggestions_fetcher = std::make_unique<RemoteSuggestionsFetcherImpl>(
      identity_manager, url_loader_factory, prefs, nullptr,
      base::BindRepeating(&InProcessJsonParser::Parse), GetFetchEndpoint(),
      api_key, service->user_classifier());

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      chrome_browser_state->GetProtoDatabaseProvider();

  // This pref is also used for logging. If it is changed, change it in the
  // other places.
  std::vector<std::string> prefs_vector = {prefs::kArticlesForYouEnabled};
  prefs_vector.push_back(prefs::kNTPContentSuggestionsEnabled);

  auto provider = std::make_unique<RemoteSuggestionsProviderImpl>(
      service, prefs, GetApplicationContext()->GetApplicationLocale(),
      service->category_ranker(), service->remote_suggestions_scheduler(),
      std::move(suggestions_fetcher),
      std::make_unique<ImageFetcherImpl>(
          CreateIOSImageDecoder(), browser_state->GetSharedURLLoaderFactory()),
      std::make_unique<RemoteSuggestionsDatabase>(db_provider, database_dir),
      std::make_unique<RemoteSuggestionsStatusServiceImpl>(
          identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync),
          prefs, prefs_vector),
      std::make_unique<base::OneShotTimer>());

  service->remote_suggestions_scheduler()->SetProvider(provider.get());
  service->set_remote_suggestions_provider(provider.get());
  service->RegisterProvider(std::move(provider));
}

}  // namespace ntp_snippets
