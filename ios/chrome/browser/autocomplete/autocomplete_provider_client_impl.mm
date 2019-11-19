// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/autocomplete_provider_client_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/language/core/browser/pref_names.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "ios/chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AutocompleteProviderClientImpl::AutocompleteProviderClientImpl(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      url_consent_helper_(unified_consent::UrlKeyedDataCollectionConsentHelper::
                              NewPersonalizedDataCollectionConsentHelper(
                                  ProfileSyncServiceFactory::GetForBrowserState(
                                      browser_state_))) {}

AutocompleteProviderClientImpl::~AutocompleteProviderClientImpl() {}

scoped_refptr<network::SharedURLLoaderFactory>
AutocompleteProviderClientImpl::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

PrefService* AutocompleteProviderClientImpl::GetPrefs() {
  return browser_state_->GetPrefs();
}

const AutocompleteSchemeClassifier&
AutocompleteProviderClientImpl::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
AutocompleteProviderClientImpl::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(browser_state_);
}

history::HistoryService* AutocompleteProviderClientImpl::GetHistoryService() {
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<history::TopSites> AutocompleteProviderClientImpl::GetTopSites() {
  return ios::TopSitesFactory::GetForBrowserState(browser_state_);
}

bookmarks::BookmarkModel* AutocompleteProviderClientImpl::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
}

history::URLDatabase* AutocompleteProviderClientImpl::GetInMemoryDatabase() {
  // This method is called in unit test contexts where the HistoryService isn't
  // loaded. In that case, return null.
  history::HistoryService* history_service = GetHistoryService();
  return history_service ? history_service->InMemoryDatabase() : nullptr;
}

InMemoryURLIndex* AutocompleteProviderClientImpl::GetInMemoryURLIndex() {
  return ios::InMemoryURLIndexFactory::GetForBrowserState(browser_state_);
}

TemplateURLService* AutocompleteProviderClientImpl::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const TemplateURLService*
AutocompleteProviderClientImpl::GetTemplateURLService() const {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

RemoteSuggestionsService*
AutocompleteProviderClientImpl::GetRemoteSuggestionsService(
    bool create_if_necessary) const {
  return nullptr;
}

DocumentSuggestionsService*
AutocompleteProviderClientImpl::GetDocumentSuggestionsService(
    bool create_if_necessary) const {
  return nullptr;
}

OmniboxPedalProvider* AutocompleteProviderClientImpl::GetPedalProvider() const {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackend() {
  return ios::ShortcutsBackendFactory::GetForBrowserState(browser_state_);
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackendIfExists() {
  return ios::ShortcutsBackendFactory::GetForBrowserStateIfExists(
      browser_state_);
}

std::unique_ptr<KeywordExtensionsDelegate>
AutocompleteProviderClientImpl::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
  return nullptr;
}

std::string AutocompleteProviderClientImpl::GetAcceptLanguages() const {
  return browser_state_->GetPrefs()->GetString(
      language::prefs::kAcceptLanguages);
}

std::string
AutocompleteProviderClientImpl::GetEmbedderRepresentationOfAboutScheme() const {
  return kChromeUIScheme;
}

std::vector<base::string16> AutocompleteProviderClientImpl::GetBuiltinURLs() {
  std::vector<std::string> chrome_builtins(
      kChromeHostURLs, kChromeHostURLs + kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<base::string16> builtins;
  for (auto& url : chrome_builtins) {
    builtins.push_back(base::ASCIIToUTF16(url));
  }
  return builtins;
}

std::vector<base::string16>
AutocompleteProviderClientImpl::GetBuiltinsToProvideAsUserTypes() {
  return {base::ASCIIToUTF16(kChromeUIChromeURLsURL),
          base::ASCIIToUTF16(kChromeUIVersionURL)};
}

component_updater::ComponentUpdateService*
AutocompleteProviderClientImpl::GetComponentUpdateService() {
  return GetApplicationContext()->GetComponentUpdateService();
}

bool AutocompleteProviderClientImpl::IsOffTheRecord() const {
  return browser_state_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::SearchSuggestEnabled() const {
  return browser_state_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool AutocompleteProviderClientImpl::IsPersonalizedUrlDataCollectionActive()
    const {
  return url_consent_helper_->IsEnabled();
}

bool AutocompleteProviderClientImpl::IsAuthenticated() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state_);
  return identity_manager != nullptr && identity_manager->HasPrimaryAccount();
}

bool AutocompleteProviderClientImpl::IsSyncActive() const {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  return sync && sync->IsSyncFeatureActive();
}

void AutocompleteProviderClientImpl::Classify(
    const base::string16& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  AutocompleteClassifier* classifier = GetAutocompleteClassifier();
  classifier->Classify(text, prefer_keyword, allow_exact_keyword_match,
                       page_classification, match, alternate_nav_url);
}

void AutocompleteProviderClientImpl::DeleteMatchingURLsForKeywordFromHistory(
    history::KeywordID keyword_id,
    const base::string16& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void AutocompleteProviderClientImpl::PrefetchImage(const GURL& url) {}

bool AutocompleteProviderClientImpl::IsTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) {
  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state_);
  WebStateList* web_state_list = tab_model.webStateList;
  return web_state_list && web_state_list->GetIndexOfInactiveWebStateWithURL(
                               url) != WebStateList::kInvalidIndex;
}
