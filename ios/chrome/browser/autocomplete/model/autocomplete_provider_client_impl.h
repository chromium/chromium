// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_PROVIDER_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_PROVIDER_CLIENT_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/autocomplete/model/tab_matcher_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AutocompleteScoringModelService;
class OnDeviceTailModelService;
struct ProviderStateService;

namespace unified_consent {
class UrlKeyedDataCollectionConsentHelper;
}

namespace component_updater {
class ComponentUpdateService;
}

// AutocompleteProviderClientImpl provides iOS-specific implementation of
// AutocompleteProviderClient interface.
class AutocompleteProviderClientImpl : public AutocompleteProviderClient {
 public:
  explicit AutocompleteProviderClientImpl(ProfileIOS* profile);

  AutocompleteProviderClientImpl(const AutocompleteProviderClientImpl&) =
      delete;
  AutocompleteProviderClientImpl& operator=(
      const AutocompleteProviderClientImpl&) = delete;

  ~AutocompleteProviderClientImpl() override;

  // AutocompleteProviderClient implementation.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  PrefService* GetPrefs() const override;
  PrefService* GetLocalState() override;
  std::string GetApplicationLocale() const override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  history::HistoryService* GetHistoryService() override;
  scoped_refptr<history::TopSites> GetTopSites() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  history::URLDatabase* GetInMemoryDatabase() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  TemplateURLService* GetTemplateURLService() override;
  const TemplateURLService* GetTemplateURLService() const override;
  RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const override;
  ZeroSuggestCacheService* GetZeroSuggestCacheService() override;
  const ZeroSuggestCacheService* GetZeroSuggestCacheService() const override;
  OmniboxPedalProvider* GetPedalProvider() const override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;
  std::unique_ptr<KeywordExtensionsDelegate> GetKeywordExtensionsDelegate(
      KeywordProvider* keyword_provider) override;
  OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const override;
  AutocompleteScoringModelService* GetAutocompleteScoringModelService()
      const override;
  OnDeviceTailModelService* GetOnDeviceTailModelService() const override;
  ProviderStateService* GetProviderStateService() const override;
  std::string GetAcceptLanguages() const override;
  std::string GetEmbedderRepresentationOfAboutScheme() const override;
  std::vector<std::u16string> GetBuiltinURLs() override;
  std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() override;
  component_updater::ComponentUpdateService* GetComponentUpdateService()
      override;
  signin::IdentityManager* GetIdentityManager() const override;
  bool IsOffTheRecord() const override;
  bool IsIncognitoProfile() const override;
  bool IsGuestSession() const override;
  bool SearchSuggestEnabled() const override;
  bool IsPersonalizedUrlDataCollectionActive() const override;
  bool IsAuthenticated() const override;
  bool IsSyncActive() const override;
  void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) override;
  void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const std::u16string& term) override;
  void PrefetchImage(const GURL& url) override;
  const TabMatcher& GetTabMatcher() const override;
  bool in_background_state() const override;
  void set_in_background_state(bool in_background_state) override;

  // OmniboxAction::Client implementation.
  void OpenSharingHub() override {}
  void NewIncognitoWindow() override {}
  void OpenIncognitoClearBrowsingDataDialog() override {}
  void CloseIncognitoWindows() override {}
  void PromptPageTranslation() override {}

 private:
  raw_ptr<ProfileIOS> profile_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_consent_helper_;
  std::unique_ptr<OmniboxTriggeredFeatureService>
      omnibox_triggered_feature_service_;
  TabMatcherImpl tab_matcher_;
  std::unique_ptr<OmniboxPedalProvider> pedal_provider_;
  // Whether or not the app is currently in the background state.
  bool in_background_state_ = false;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_PROVIDER_CLIENT_IMPL_H_
