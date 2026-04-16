// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_cobrowse_omnibox_client.h"

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
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/omnibox_shortcuts_helper.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_omnibox_client_delegate.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/vector_icon_types.h"
#import "url/gurl.h"

ComposeboxCobrowseOmniboxClient::ComposeboxCobrowseOmniboxClient(
    Browser* browser,
    feature_engagement::Tracker* tracker,
    id<ComposeboxOmniboxClientDelegate> delegate)
    : browser_(browser),
      profile_(browser->GetProfile()),
      engagement_tracker_(tracker),
      delegate_(delegate) {
  CHECK(engagement_tracker_);
}

ComposeboxCobrowseOmniboxClient::~ComposeboxCobrowseOmniboxClient() {}

std::unique_ptr<AutocompleteProviderClient>
ComposeboxCobrowseOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(profile_);
}

bool ComposeboxCobrowseOmniboxClient::CurrentPageExists() const {
  return YES;
}

const GURL& ComposeboxCobrowseOmniboxClient::GetURL() const {
  return GURL::EmptyGURL();
}

bool ComposeboxCobrowseOmniboxClient::IsLoading() const {
  return NO;
}

bool ComposeboxCobrowseOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

bool ComposeboxCobrowseOmniboxClient::IsDefaultSearchProviderEnabled() const {
  return true;
}

SessionID ComposeboxCobrowseOmniboxClient::GetSessionID() const {
  return SessionID::NewUnique();
}

PrefService* ComposeboxCobrowseOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* ComposeboxCobrowseOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* ComposeboxCobrowseOmniboxClient::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

AutocompleteControllerEmitter*
ComposeboxCobrowseOmniboxClient::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* ComposeboxCobrowseOmniboxClient::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier&
ComposeboxCobrowseOmniboxClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier*
ComposeboxCobrowseOmniboxClient::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool ComposeboxCobrowseOmniboxClient::ShouldDefaultTypedNavigationsToHttps()
    const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int ComposeboxCobrowseOmniboxClient::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->GetHttpsPortForTesting();
}

bool ComposeboxCobrowseOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting()
    const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image ComposeboxCobrowseOmniboxClient::GetExtensionIcon(
    const TemplateURL* template_url) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
}

std::u16string ComposeboxCobrowseOmniboxClient::GetFormattedFullURL() const {
  return u"";
}

std::u16string ComposeboxCobrowseOmniboxClient::GetURLForDisplay() const {
  return u"";
}

GURL ComposeboxCobrowseOmniboxClient::GetNavigationEntryURL() const {
  return GURL::EmptyGURL();
}

metrics::OmniboxEventProto::PageClassification
ComposeboxCobrowseOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::PageClassification::
      OmniboxEventProto_PageClassification_ANDROID_HUB;
}

security_state::SecurityLevel
ComposeboxCobrowseOmniboxClient::GetSecurityLevel() const {
  if (web::WebState* web_state = delegate_.webState) {
    return security_state::GetSecurityLevelForWebState(web_state);
  }
  return security_state::SecurityLevel::NONE;
}

net::CertStatus ComposeboxCobrowseOmniboxClient::GetCertStatus() const {
  return 0;
}

const gfx::VectorIcon& ComposeboxCobrowseOmniboxClient::GetVectorIcon() const {
  return gfx::VectorIcon::EmptyIcon();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ComposeboxCobrowseOmniboxClient::GetLensOverlaySuggestInputs() const {
  return [delegate_ suggestInputs];
}

void ComposeboxCobrowseOmniboxClient::ProcessExtensionMatch(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  // Extensions are not supported on iOS.
}

void ComposeboxCobrowseOmniboxClient::OnFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  // NO-OP
}

void ComposeboxCobrowseOmniboxClient::OnTextChanged(
    const AutocompleteMatch& current_match,
    bool user_input_in_progress,
    const std::u16string& user_text,
    const AutocompleteResult& result,
    bool has_focus) {
  [delegate_
      omniboxDidChangeText:user_text
             isSearchQuery:AutocompleteMatch::IsSearchType(current_match.type)
       userInputInProgress:user_input_in_progress];
}

void ComposeboxCobrowseOmniboxClient::OnThumbnailOnlyAccept() {
  UrlLoadParams params = CreateOmniboxUrlLoadParams(
      GURL(),
      /*post_content=*/nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED,
      /*destination_url_entered_without_scheme=*/false,
      profile_->IsOffTheRecord());
  [delegate_ omniboxDidAcceptText:u""
                   destinationURL:GURL()
                    URLLoadParams:params
                     isSearchType:NO];
}

void ComposeboxCobrowseOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {}

void ComposeboxCobrowseOmniboxClient::DiscardNonCommittedNavigations() {}

const std::u16string& ComposeboxCobrowseOmniboxClient::GetTitle() const {
  return delegate_.webState->GetTitle();
}

gfx::Image ComposeboxCobrowseOmniboxClient::GetFavicon() const {
  return gfx::Image();
}

void ComposeboxCobrowseOmniboxClient::OnAutocompleteAccept(
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
  [delegate_ omniboxDidAcceptText:text + match.inline_autocompletion
                   destinationURL:destination_url
                    URLLoadParams:CreateOmniboxUrlLoadParams(
                                      destination_url, post_content,
                                      disposition, transition,
                                      destination_url_entered_without_scheme,
                                      profile_->IsOffTheRecord())
                     isSearchType:AutocompleteMatch::IsSearchType(match.type)];
}

base::WeakPtr<OmniboxClientIOS> ComposeboxCobrowseOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

omnibox::InputState ComposeboxCobrowseOmniboxClient::GetInputState() const {
  return [delegate_ inputState];
}
