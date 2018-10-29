// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/ios/ios_image_decoder_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/breaking_news/breaking_news_listener.h"
#include "components/ntp_snippets/category_rankers/category_ranker.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/logger.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/persistent_scheduler.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler_impl.h"
#include "components/ntp_snippets/remote/remote_suggestions_status_service_impl.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using history::HistoryService;
using image_fetcher::CreateIOSImageDecoder;
using image_fetcher::ImageFetcherImpl;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::GetFetchEndpoint;
using ntp_snippets::Logger;
using ntp_snippets::PersistentScheduler;
using ntp_snippets::RemoteSuggestionsDatabase;
using ntp_snippets::RemoteSuggestionsFetcherImpl;
using ntp_snippets::RemoteSuggestionsProviderImpl;
using ntp_snippets::RemoteSuggestionsSchedulerImpl;
using ntp_snippets::RemoteSuggestionsStatusServiceImpl;
using ntp_snippets::UserClassifier;

namespace {

void ParseJson(const std::string& json,
               const ntp_snippets::SuccessCallback& success_callback,
               const ntp_snippets::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(success_callback, base::Passed(&value)));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback, json_reader.GetErrorMessage()));
  }
}

}  // namespace

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
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  PrefService* prefs = chrome_browser_state->GetPrefs();

  auto user_classifier = std::make_unique<UserClassifier>(
      prefs, base::DefaultClock::GetInstance());

  auto debug_logger = std::make_unique<Logger>();

  // TODO(crbug.com/676249): Implement a persistent scheduler for iOS.
  auto scheduler = std::make_unique<RemoteSuggestionsSchedulerImpl>(
      /*persistent_scheduler=*/nullptr, user_classifier.get(), prefs,
      GetApplicationContext()->GetLocalState(),
      base::DefaultClock::GetInstance(), debug_logger.get());

  // Create the ContentSuggestionsService.
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state);
  HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  favicon::LargeIconService* large_icon_service =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          chrome_browser_state);
  std::unique_ptr<ntp_snippets::CategoryRanker> category_ranker =
      ntp_snippets::BuildSelectedCategoryRanker(
          prefs, base::DefaultClock::GetInstance(),
          /*is_chrome_home_enabled=*/false);
  return std::make_unique<ContentSuggestionsService>(
      State::ENABLED, identity_manager, history_service, large_icon_service,
      prefs, std::move(category_ranker), std::move(user_classifier),
      std::move(scheduler), std::move(debug_logger));
}

void RegisterRemoteSuggestionsProvider(ContentSuggestionsService* service,
                                       web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  PrefService* prefs = chrome_browser_state->GetPrefs();
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state);
  scoped_refptr<net::URLRequestContextGetter> request_context =
      browser_state->GetRequestContext();
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      browser_state->GetSharedURLLoaderFactory();

  base::FilePath database_dir(
      browser_state->GetStatePath().Append(ntp_snippets::kDatabaseFolder));

  std::string api_key;
  // This API needs whitelisted API keys. Get the key only if it is not a
  // dummy key.
  if (google_apis::HasAPIKeyConfigured()) {
    bool is_stable_channel = GetChannel() == version_info::Channel::STABLE;
    api_key = is_stable_channel ? google_apis::GetAPIKey()
                                : google_apis::GetNonStableAPIKey();
  }
  auto suggestions_fetcher = std::make_unique<RemoteSuggestionsFetcherImpl>(
      identity_manager, url_loader_factory, prefs, nullptr,
      base::BindRepeating(&ParseJson), GetFetchEndpoint(), api_key,
      service->user_classifier());

  // This pref is also used for logging. If it is changed, change it in the
  // other places.
  std::string pref_name = prefs::kArticlesForYouEnabled;
  auto provider = std::make_unique<RemoteSuggestionsProviderImpl>(
      service, prefs, GetApplicationContext()->GetApplicationLocale(),
      service->category_ranker(), service->remote_suggestions_scheduler(),
      std::move(suggestions_fetcher),
      std::make_unique<ImageFetcherImpl>(
          CreateIOSImageDecoder(), browser_state->GetSharedURLLoaderFactory()),
      std::make_unique<RemoteSuggestionsDatabase>(database_dir),
      std::make_unique<RemoteSuggestionsStatusServiceImpl>(
          identity_manager->HasPrimaryAccount(), prefs, pref_name),
      /*prefetched_pages_tracker=*/nullptr,
      /*breaking_news_raw_data_provider*/ nullptr, service->debug_logger(),
      std::make_unique<base::OneShotTimer>());

  service->remote_suggestions_scheduler()->SetProvider(provider.get());
  service->set_remote_suggestions_provider(provider.get());
  service->RegisterProvider(std::move(provider));
}

}  // namespace ntp_snippets
