// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "components/history/core/browser/history_service.h"
#import "components/history/core/browser/top_sites.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/language/core/browser/pref_names.h"
#import "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_scoring_model_service.h"
#import "components/omnibox/browser/omnibox_triggered_feature_service.h"
#import "components/omnibox/browser/provider_state_service.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scoring_model_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/in_memory_url_index_factory.h"
#import "ios/chrome/browser/autocomplete/model/omnibox_pedal_implementation.h"
#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/autocomplete/model/tab_matcher_impl.h"
#import "ios/chrome/browser/autocomplete/model/zero_suggest_cache_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Killswitch, can be removed around December 2023. If enabled,
// IsAuthenticated() will only return true for Sync-consented accounts.
BASE_FEATURE(kIosAutocompleteProviderRequireSync,
             "IosAutocompleteProviderRequireSync",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

AutocompleteProviderClientImpl::AutocompleteProviderClientImpl(
    ProfileIOS* profile)
    : profile_(profile),
      url_consent_helper_(
          base::FeatureList::IsEnabled(
              omnibox::kPrefBasedDataCollectionConsentHelper)
              ? unified_consent::UrlKeyedDataCollectionConsentHelper::
                    NewAnonymizedDataCollectionConsentHelper(
                        profile_->GetPrefs())
              : unified_consent::UrlKeyedDataCollectionConsentHelper::
                    NewPersonalizedDataCollectionConsentHelper(
                        SyncServiceFactory::GetForProfile(profile_))),
      omnibox_triggered_feature_service_(
          std::make_unique<OmniboxTriggeredFeatureService>()),
      tab_matcher_(profile_) {
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(
      *this, GetPedalImplementations(IsOffTheRecord(), false));
}

AutocompleteProviderClientImpl::~AutocompleteProviderClientImpl() {}

scoped_refptr<network::SharedURLLoaderFactory>
AutocompleteProviderClientImpl::GetURLLoaderFactory() {
  return profile_->GetSharedURLLoaderFactory();
}

PrefService* AutocompleteProviderClientImpl::GetPrefs() const {
  return profile_->GetPrefs();
}

PrefService* AutocompleteProviderClientImpl::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

std::string AutocompleteProviderClientImpl::GetApplicationLocale() const {
  return GetApplicationContext()->GetApplicationLocale();
}

const AutocompleteSchemeClassifier&
AutocompleteProviderClientImpl::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
AutocompleteProviderClientImpl::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForProfile(profile_);
}

history::HistoryService* AutocompleteProviderClientImpl::GetHistoryService() {
  return ios::HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<history::TopSites> AutocompleteProviderClientImpl::GetTopSites() {
  return ios::TopSitesFactory::GetForProfile(profile_);
}

bookmarks::BookmarkModel* AutocompleteProviderClientImpl::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

history::URLDatabase* AutocompleteProviderClientImpl::GetInMemoryDatabase() {
  // This method is called in unit test contexts where the HistoryService isn't
  // loaded. In that case, return null.
  history::HistoryService* history_service = GetHistoryService();
  return history_service ? history_service->InMemoryDatabase() : nullptr;
}

InMemoryURLIndex* AutocompleteProviderClientImpl::GetInMemoryURLIndex() {
  return ios::InMemoryURLIndexFactory::GetForProfile(profile_);
}

TemplateURLService* AutocompleteProviderClientImpl::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

const TemplateURLService*
AutocompleteProviderClientImpl::GetTemplateURLService() const {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

RemoteSuggestionsService*
AutocompleteProviderClientImpl::GetRemoteSuggestionsService(
    bool create_if_necessary) const {
  return RemoteSuggestionsServiceFactory::GetForProfile(profile_,
                                                        create_if_necessary);
}

ZeroSuggestCacheService*
AutocompleteProviderClientImpl::GetZeroSuggestCacheService() {
  return ios::ZeroSuggestCacheServiceFactory::GetForProfile(profile_);
}

const ZeroSuggestCacheService*
AutocompleteProviderClientImpl::GetZeroSuggestCacheService() const {
  return ios::ZeroSuggestCacheServiceFactory::GetForProfile(profile_);
}

OmniboxPedalProvider* AutocompleteProviderClientImpl::GetPedalProvider() const {
  return pedal_provider_.get();
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackend() {
  return ios::ShortcutsBackendFactory::GetForProfile(profile_);
}

scoped_refptr<ShortcutsBackend>
AutocompleteProviderClientImpl::GetShortcutsBackendIfExists() {
  return ios::ShortcutsBackendFactory::GetForProfileIfExists(profile_);
}

std::unique_ptr<KeywordExtensionsDelegate>
AutocompleteProviderClientImpl::GetKeywordExtensionsDelegate(
    KeywordProvider* keyword_provider) {
  return nullptr;
}

OmniboxTriggeredFeatureService*
AutocompleteProviderClientImpl::GetOmniboxTriggeredFeatureService() const {
  return omnibox_triggered_feature_service_.get();
}

AutocompleteScoringModelService*
AutocompleteProviderClientImpl::GetAutocompleteScoringModelService() const {
  return ios::AutocompleteScoringModelServiceFactory::GetForProfile(profile_);
}

OnDeviceTailModelService*
AutocompleteProviderClientImpl::GetOnDeviceTailModelService() const {
  // TODO(crbug.com/40241602): implement the service factory for iOS.
  return nullptr;
}

ProviderStateService* AutocompleteProviderClientImpl::GetProviderStateService()
    const {
  return ios::ProviderStateServiceFactory::GetForProfile(profile_);
}

std::string AutocompleteProviderClientImpl::GetAcceptLanguages() const {
  return profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

std::string
AutocompleteProviderClientImpl::GetEmbedderRepresentationOfAboutScheme() const {
  return kChromeUIScheme;
}

std::vector<std::u16string> AutocompleteProviderClientImpl::GetBuiltinURLs() {
  std::vector<std::string> chrome_builtins(
      kChromeHostURLs, kChromeHostURLs + kNumberOfChromeHostURLs);
  std::sort(chrome_builtins.begin(), chrome_builtins.end());

  std::vector<std::u16string> builtins;
  for (auto& url : chrome_builtins) {
    builtins.push_back(base::ASCIIToUTF16(url));
  }
  return builtins;
}

std::vector<std::u16string>
AutocompleteProviderClientImpl::GetBuiltinsToProvideAsUserTypes() {
  return {base::ASCIIToUTF16(kChromeUIChromeURLsURL),
          base::ASCIIToUTF16(kChromeUIVersionURL)};
}

component_updater::ComponentUpdateService*
AutocompleteProviderClientImpl::GetComponentUpdateService() {
  return GetApplicationContext()->GetComponentUpdateService();
}

signin::IdentityManager* AutocompleteProviderClientImpl::GetIdentityManager()
    const {
  return IdentityManagerFactory::GetForProfile(profile_);
}

bool AutocompleteProviderClientImpl::IsOffTheRecord() const {
  return profile_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::IsIncognitoProfile() const {
  return profile_->IsOffTheRecord();
}

bool AutocompleteProviderClientImpl::IsGuestSession() const {
  return false;
}

bool AutocompleteProviderClientImpl::SearchSuggestEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled);
}

bool AutocompleteProviderClientImpl::IsPersonalizedUrlDataCollectionActive()
    const {
  return url_consent_helper_->IsEnabled();
}

bool AutocompleteProviderClientImpl::IsAuthenticated() const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  signin::ConsentLevel level =
      base::FeatureList::IsEnabled(kIosAutocompleteProviderRequireSync)
          ? signin::ConsentLevel::kSync
          : signin::ConsentLevel::kSignin;
  return identity_manager && identity_manager->HasPrimaryAccount(level);
}

bool AutocompleteProviderClientImpl::IsSyncActive() const {
  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile_);
  // TODO(crbug.com/40066949): Remove usage of IsSyncFeatureActive() after kSync
  // users are migrated to kSignin in phase 3. See ConsentLevel::kSync
  // documentation for details.
  return sync && sync->IsSyncFeatureActive();
}

void AutocompleteProviderClientImpl::Classify(
    const std::u16string& text,
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
    const std::u16string& term) {
  GetHistoryService()->DeleteMatchingURLsForKeyword(keyword_id, term);
}

void AutocompleteProviderClientImpl::PrefetchImage(const GURL& url) {}

const TabMatcher& AutocompleteProviderClientImpl::GetTabMatcher() const {
  return tab_matcher_;
}

bool AutocompleteProviderClientImpl::in_background_state() const {
  return in_background_state_;
}

void AutocompleteProviderClientImpl::set_in_background_state(
    bool in_background_state) {
  in_background_state_ = in_background_state;
}
