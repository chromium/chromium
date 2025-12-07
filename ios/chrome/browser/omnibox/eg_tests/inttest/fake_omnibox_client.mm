// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/eg_tests/inttest/fake_omnibox_client.h"

#import "base/feature_list.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

FakeOmniboxClient::FakeOmniboxClient(ProfileIOS* profile) : profile_(profile) {
  prefs_ = profile_->GetPrefs();
}

FakeOmniboxClient::~FakeOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
FakeOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(profile_);
}

bool FakeOmniboxClient::CurrentPageExists() const {
  return current_page_exists_;
}

const GURL& FakeOmniboxClient::GetURL() const {
  return url_;
}

const std::u16string& FakeOmniboxClient::GetTitle() const {
  return title_;
}

gfx::Image FakeOmniboxClient::GetFavicon() const {
  return favicon_;
}

ukm::SourceId FakeOmniboxClient::GetUKMSourceId() const {
  return ukm_source_id_;
}

bool FakeOmniboxClient::IsLoading() const {
  return is_loading_;
}

bool FakeOmniboxClient::IsPasteAndGoEnabled() const {
  return is_paste_and_go_enabled_;
}

bool FakeOmniboxClient::IsDefaultSearchProviderEnabled() const {
  return is_default_search_provider_enabled_;
}

SessionID FakeOmniboxClient::GetSessionID() const {
  return session_id_;
}

PrefService* FakeOmniboxClient::GetPrefs() {
  return prefs_;
}

const PrefService* FakeOmniboxClient::GetPrefs() const {
  return prefs_;
}

bookmarks::BookmarkModel* FakeOmniboxClient::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

AutocompleteControllerEmitter*
FakeOmniboxClient::GetAutocompleteControllerEmitter() {
  return autocomplete_controller_emitter_;
}

TemplateURLService* FakeOmniboxClient::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& FakeOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* FakeOmniboxClient::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool FakeOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return should_default_typed_navigations_to_https_;
}

int FakeOmniboxClient::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->GetHttpsPortForTesting();
}

bool FakeOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image FakeOmniboxClient::GetExtensionIcon(
    const TemplateURL* template_url) const {
  return gfx::Image();
}

gfx::Image FakeOmniboxClient::GetSizedIcon(const SkBitmap* bitmap) const {
  return gfx::Image();
}

gfx::Image FakeOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image();
}

gfx::Image FakeOmniboxClient::GetSizedIcon(const gfx::Image& icon) const {
  return gfx::Image();
}

std::u16string FakeOmniboxClient::GetFormattedFullURL() const {
  return formatted_full_url_;
}

std::u16string FakeOmniboxClient::GetURLForDisplay() const {
  return url_for_display_;
}

GURL FakeOmniboxClient::GetNavigationEntryURL() const {
  return navigation_entry_url_;
}

metrics::OmniboxEventProto::PageClassification
FakeOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return page_classification_;
}

security_state::SecurityLevel FakeOmniboxClient::GetSecurityLevel() const {
  return security_level_;
}

net::CertStatus FakeOmniboxClient::GetCertStatus() const {
  return cert_status_;
}

const gfx::VectorIcon& FakeOmniboxClient::GetVectorIcon() const {
  return gfx::VectorIcon::EmptyIcon();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
FakeOmniboxClient::GetLensOverlaySuggestInputs() const {
  return lens_overlay_suggest_inputs_;
}

void FakeOmniboxClient::ProcessExtensionMatch(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {}

void FakeOmniboxClient::OnInputStateChanged() {}

void FakeOmniboxClient::OnFocusChanged(OmniboxFocusState state,
                                       OmniboxFocusChangeReason reason) {}

void FakeOmniboxClient::OnUserPastedInOmniboxResultingInValidURL() {}

void FakeOmniboxClient::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    bool should_preload,
    const BitmapFetchedCallback& on_bitmap_fetched) {}

gfx::Image FakeOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image FakeOmniboxClient::GetFaviconForDefaultSearchProvider(
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image FakeOmniboxClient::GetFaviconForKeywordSearchProvider(
    const TemplateURL* template_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

void FakeOmniboxClient::OnTextChanged(const AutocompleteMatch& current_match,
                                      bool user_input_in_progress,
                                      const std::u16string& user_text,
                                      const AutocompleteResult& result,
                                      bool has_focus) {}

void FakeOmniboxClient::OnRevert() {}

void FakeOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {}

void FakeOmniboxClient::OnBookmarkLaunched() {}

void FakeOmniboxClient::DiscardNonCommittedNavigations() {}

void FakeOmniboxClient::FocusWebContents() {}

void FakeOmniboxClient::ShowFeedbackPage(const std::u16string& input_text,
                                         const GURL& destination_url) {}

void FakeOmniboxClient::OnAutocompleteAccept(
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
    const AutocompleteMatch& alternative_nav_match) {
  on_autocomplete_accept_destination_url_ = destination_url;
}

void FakeOmniboxClient::OnThumbnailOnlyAccept() {}

void FakeOmniboxClient::OnInputInProgress(bool in_progress) {}

void FakeOmniboxClient::OnPopupVisibilityChanged(bool popup_is_open) {}

void FakeOmniboxClient::OnThumbnailRemoved() {}

void FakeOmniboxClient::OpenIphLink(GURL gurl) {}

bool FakeOmniboxClient::IsHistoryEmbeddingsEnabled() const {
  return is_history_embeddings_enabled_;
}

base::WeakPtr<OmniboxClient> FakeOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
