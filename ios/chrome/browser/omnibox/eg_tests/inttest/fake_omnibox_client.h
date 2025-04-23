// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_OMNIBOX_CLIENT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_OMNIBOX_CLIENT_H_

#import <memory>
#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/vector_icon_types.h"

class ProfileIOS;

/// Fake OmniboxClient class used in tests.
class FakeOmniboxClient : public OmniboxClient {
 public:
  explicit FakeOmniboxClient(ProfileIOS* profile);
  FakeOmniboxClient(const FakeOmniboxClient&) = delete;
  FakeOmniboxClient& operator=(const FakeOmniboxClient&) = delete;
  ~FakeOmniboxClient() override;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  const std::u16string& GetTitle() const override;
  gfx::Image GetFavicon() const override;
  ukm::SourceId GetUKMSourceId() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  bool IsDefaultSearchProviderEnabled() const override;
  SessionID GetSessionID() const override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  AutocompleteControllerEmitter* GetAutocompleteControllerEmitter() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  bool ShouldDefaultTypedNavigationsToHttps() const override;
  int GetHttpsPortForTesting() const override;
  bool IsUsingFakeHttpsForHttpsUpgradeTesting() const override;
  gfx::Image GetExtensionIcon(const TemplateURL* template_url) const override;
  gfx::Image GetSizedIcon(const SkBitmap* bitmap) const override;
  gfx::Image GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                          SkColor vector_icon_color) const override;
  gfx::Image GetSizedIcon(const gfx::Image& icon) const override;
  std::u16string GetFormattedFullURL() const override;
  std::u16string GetURLForDisplay() const override;
  GURL GetNavigationEntryURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
  net::CertStatus GetCertStatus() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;
  void ProcessExtensionMatch(const std::u16string& text,
                             const TemplateURL* template_url,
                             const AutocompleteMatch& match,
                             WindowOpenDisposition disposition) override;
  void OnInputStateChanged() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnUserPastedInOmniboxResultingInValidURL() override;
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       bool should_preload,
                       const BitmapFetchedCallback& on_bitmap_fetched) override;
  gfx::Image GetFaviconForPageUrl(
      const GURL& page_url,
      FaviconFetchedCallback on_favicon_fetched) override;
  gfx::Image GetFaviconForDefaultSearchProvider(
      FaviconFetchedCallback on_favicon_fetched) override;
  gfx::Image GetFaviconForKeywordSearchProvider(
      const TemplateURL* template_url,
      FaviconFetchedCallback on_favicon_fetched) override;
  void OnTextChanged(const AutocompleteMatch& current_match,
                     bool user_input_in_progress,
                     const std::u16string& user_text,
                     const AutocompleteResult& result,
                     bool has_focus) override;
  void OnRevert() override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void OnBookmarkLaunched() override;
  void DiscardNonCommittedNavigations() override;
  void FocusWebContents() override;
  void ShowFeedbackPage(const std::u16string& input_text,
                        const GURL& destination_url) override;
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;
  void OnThumbnailOnlyAccept() override;
  void OnInputInProgress(bool in_progress) override;
  void OnPopupVisibilityChanged(bool popup_is_open) override;
  void OnThumbnailRemoved() override;
  void OpenIphLink(GURL gurl) override;
  bool IsHistoryEmbeddingsEnabled() const override;
  base::WeakPtr<OmniboxClient> AsWeakPtr() override;

  // Setters for return values (for those not retrieved from ProfileIOS)
  void set_current_page_exists(bool exists) { current_page_exists_ = exists; }
  void set_url(const GURL& url) { url_ = url; }
  void set_title(const std::u16string& title) { title_ = title; }
  void set_favicon(const gfx::Image& favicon) { favicon_ = favicon; }
  void set_ukm_source_id(ukm::SourceId id) { ukm_source_id_ = id; }
  void set_is_loading(bool loading) { is_loading_ = loading; }
  void set_is_paste_and_go_enabled(bool enabled) {
    is_paste_and_go_enabled_ = enabled;
  }
  void set_is_default_search_provider_enabled(bool enabled) {
    is_default_search_provider_enabled_ = enabled;
  }
  void set_session_id(SessionID session_id) { session_id_ = session_id; }
  void set_autocomplete_controller_emitter(
      AutocompleteControllerEmitter* emitter) {
    autocomplete_controller_emitter_ = emitter;
  }
  void set_should_default_typed_navigations_to_https(bool should_default) {
    should_default_typed_navigations_to_https_ = should_default;
  }
  void set_formatted_full_url(const std::u16string& url) {
    formatted_full_url_ = url;
  }
  void set_url_for_display(const std::u16string& url) {
    url_for_display_ = url;
  }
  void set_navigation_entry_url(const GURL& url) {
    navigation_entry_url_ = url;
  }
  void set_page_classification(
      metrics::OmniboxEventProto::PageClassification classification) {
    page_classification_ = classification;
  }
  void set_security_level(security_state::SecurityLevel level) {
    security_level_ = level;
  }
  void set_cert_status(net::CertStatus status) { cert_status_ = status; }
  void set_lens_overlay_suggest_inputs(
      const std::optional<lens::proto::LensOverlaySuggestInputs>& inputs) {
    lens_overlay_suggest_inputs_ = inputs;
  }
  void set_on_autocomplete_accept_destination_url(const GURL& url) {
    on_autocomplete_accept_destination_url_ = url;
  }
  GURL get_on_autocomplete_accept_destination_url() {
    return on_autocomplete_accept_destination_url_;
  }
  void set_is_history_embeddings_enabled(bool enabled) {
    is_history_embeddings_enabled_ = enabled;
  }

 private:
  raw_ptr<ProfileIOS> profile_;
  AutocompleteSchemeClassifierImpl scheme_classifier_;

  // For overriding return values
  bool current_page_exists_ = false;
  GURL url_ = GURL();
  std::u16string title_ = u"";
  gfx::Image favicon_ = gfx::Image();
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
  bool is_loading_ = false;
  bool is_paste_and_go_enabled_ = false;
  bool is_default_search_provider_enabled_ = true;
  SessionID session_id_ = SessionID::InvalidValue();
  raw_ptr<PrefService> prefs_ = nullptr;
  raw_ptr<AutocompleteControllerEmitter> autocomplete_controller_emitter_ =
      nullptr;
  bool should_default_typed_navigations_to_https_ = false;
  std::u16string formatted_full_url_ = u"";
  std::u16string url_for_display_ = u"";
  GURL navigation_entry_url_ = GURL();
  metrics::OmniboxEventProto::PageClassification page_classification_ =
      metrics::OmniboxEventProto::OTHER;
  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;
  net::CertStatus cert_status_ = 0;
  std::optional<lens::proto::LensOverlaySuggestInputs>
      lens_overlay_suggest_inputs_ = std::nullopt;
  GURL on_autocomplete_accept_destination_url_ = GURL();
  bool is_history_embeddings_enabled_ = false;

  base::WeakPtrFactory<FakeOmniboxClient> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_EG_TESTS_INTTEST_FAKE_OMNIBOX_CLIENT_H_
