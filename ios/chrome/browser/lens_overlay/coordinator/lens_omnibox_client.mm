// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"

#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_log.h"
#import "components/omnibox/browser/shortcuts_backend.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_web_provider.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/vector_icon_types.h"
#import "url/gurl.h"

LensOmniboxClient::LensOmniboxClient(
    ProfileIOS* profile,
    feature_engagement::Tracker* tracker,
    id<LensWebProvider> web_provider,
    id<LensOmniboxClientDelegate> omnibox_delegate)
    : profile_(profile),
      engagement_tracker_(tracker),
      web_provider_(web_provider),
      delegate_(omnibox_delegate),
      thumbnail_removed_in_session_(NO) {
  CHECK(engagement_tracker_);
}

LensOmniboxClient::~LensOmniboxClient() = default;

std::unique_ptr<AutocompleteProviderClient>
LensOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(profile_);
}

bool LensOmniboxClient::CurrentPageExists() const {
  return web_provider_.webState != nullptr;
}

const GURL& LensOmniboxClient::GetURL() const {
  if (web::WebState* web_state = web_provider_.webState) {
    return web_state->GetVisibleURL();
  }
  return GURL::EmptyGURL();
}

bool LensOmniboxClient::IsLoading() const {
  if (web::WebState* web_state = web_provider_.webState) {
    return web_state->IsLoading();
  }
  return false;
}

bool LensOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

bool LensOmniboxClient::IsDefaultSearchProviderEnabled() const {
  return true;
}

SessionID LensOmniboxClient::GetSessionID() const {
  if (web::WebState* web_state = web_provider_.webState) {
    return IOSChromeSessionTabHelper::FromWebState(web_state)->session_id();
  }
  return SessionID::InvalidValue();
}

PrefService* LensOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* LensOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* LensOmniboxClient::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

AutocompleteControllerEmitter*
LensOmniboxClient::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* LensOmniboxClient::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& LensOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* LensOmniboxClient::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool LensOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int LensOmniboxClient::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->GetHttpsPortForTesting();
}

bool LensOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image LensOmniboxClient::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
}

std::u16string LensOmniboxClient::GetFormattedFullURL() const {
  std::optional<TemplateURLService::SearchMetadata> metadata =
      ios::TemplateURLServiceFactory::GetForProfile(profile_)
          ->ExtractSearchMetadata(GetURL());
  if (metadata) {
    return metadata->search_terms;
  }
  return u"";
}

std::u16string LensOmniboxClient::GetURLForDisplay() const {
  return u"";
}

GURL LensOmniboxClient::GetNavigationEntryURL() const {
  return GURL();
}

metrics::OmniboxEventProto::PageClassification
LensOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
}

security_state::SecurityLevel LensOmniboxClient::GetSecurityLevel() const {
  if (web::WebState* web_state = web_provider_.webState) {
    return security_state::GetSecurityLevelForWebState(web_state);
  }
  return security_state::SecurityLevel::NONE;
}

net::CertStatus LensOmniboxClient::GetCertStatus() const {
  return 0;
}

const gfx::VectorIcon& LensOmniboxClient::GetVectorIcon() const {
  static const gfx::VectorIcon kEmptyVectorIcon = {};
  return kEmptyVectorIcon;
}

std::optional<lens::proto::LensOverlaySuggestInputs>
LensOmniboxClient::GetLensOverlaySuggestInputs() const {
  return lens_overlay_suggest_inputs_;
}

bool LensOmniboxClient::ProcessExtensionKeyword(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  // Extensions are not supported on iOS.
  return false;
}

void LensOmniboxClient::DiscardNonCommittedNavigations() {
  web_provider_.webState->GetNavigationManager()->DiscardNonCommittedItems();
}

const std::u16string& LensOmniboxClient::GetTitle() const {
  if (web::WebState* web_state = web_provider_.webState) {
    return web_state->GetTitle();
  }
  return base::EmptyString16();
}

gfx::Image LensOmniboxClient::GetFavicon() const {
  if (web::WebState* web_state = web_provider_.webState) {
    favicon::WebFaviconDriver::FromWebState(web_state)->GetFavicon();
  }
  return gfx::Image();
}

void LensOmniboxClient::OnThumbnailRemoved() {
  thumbnail_removed_in_session_ = YES;
}

void LensOmniboxClient::OnFocusChanged(OmniboxFocusState state,
                                       OmniboxFocusChangeReason reason) {
  thumbnail_removed_in_session_ = NO;
}

void LensOmniboxClient::OnAutocompleteAccept(
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
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  [delegate_ omniboxDidAcceptText:match.fill_into_edit
                   destinationURL:destination_url
                 thumbnailRemoved:thumbnail_removed_in_session_];
}

base::WeakPtr<OmniboxClient> LensOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
