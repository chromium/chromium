// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_omnibox_client.h"

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
#import "ios/chrome/browser/aim/prototype/coordinator/aim_omnibox_client_delegate.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/location_bar/model/web_location_bar.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

AIMOmniboxClient::AIMOmniboxClient(WebLocationBar* location_bar,
                                   Browser* browser,
                                   feature_engagement::Tracker* tracker,
                                   id<AIMOmniboxClientDelegate> delegate)
    : location_bar_(location_bar),
      browser_(browser),
      profile_(browser->GetProfile()),
      engagement_tracker_(tracker),
      web_state_tracker_(),
      delegate_(delegate) {
  CHECK(engagement_tracker_);
}

AIMOmniboxClient::~AIMOmniboxClient() {
  web_state_tracker_.clear();
}

std::unique_ptr<AutocompleteProviderClient>
AIMOmniboxClient::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(profile_);
}

bool AIMOmniboxClient::CurrentPageExists() const {
  return (location_bar_->GetWebState() != nullptr);
}

const GURL& AIMOmniboxClient::GetURL() const {
  return CurrentPageExists() ? location_bar_->GetWebState()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

bool AIMOmniboxClient::IsLoading() const {
  return location_bar_->GetWebState()->IsLoading();
}

bool AIMOmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

bool AIMOmniboxClient::IsDefaultSearchProviderEnabled() const {
  // iOS does not have Enterprise policies
  return true;
}

SessionID AIMOmniboxClient::GetSessionID() const {
  return location_bar_->GetWebState()->GetUniqueIdentifier().ToSessionID();
}

PrefService* AIMOmniboxClient::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* AIMOmniboxClient::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* AIMOmniboxClient::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

AutocompleteControllerEmitter*
AIMOmniboxClient::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* AIMOmniboxClient::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForProfile(profile_);
}

const AutocompleteSchemeClassifier& AIMOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* AIMOmniboxClient::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForProfile(profile_);
}

bool AIMOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int AIMOmniboxClient::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->GetHttpsPortForTesting();
}

bool AIMOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image AIMOmniboxClient::GetExtensionIcon(
    const TemplateURL* template_url) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
}

std::u16string AIMOmniboxClient::GetFormattedFullURL() const {
  return location_bar_->GetLocationBarModel()->GetFormattedFullURL();
}

std::u16string AIMOmniboxClient::GetURLForDisplay() const {
  return location_bar_->GetLocationBarModel()->GetURLForDisplay();
}

GURL AIMOmniboxClient::GetNavigationEntryURL() const {
  return location_bar_->GetLocationBarModel()->GetURL();
}

metrics::OmniboxEventProto::PageClassification
AIMOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return location_bar_->GetLocationBarModel()->GetPageClassification(
      is_prefetch);
}

security_state::SecurityLevel AIMOmniboxClient::GetSecurityLevel() const {
  return location_bar_->GetLocationBarModel()->GetSecurityLevel();
}

net::CertStatus AIMOmniboxClient::GetCertStatus() const {
  return location_bar_->GetLocationBarModel()->GetCertStatus();
}

const gfx::VectorIcon& AIMOmniboxClient::GetVectorIcon() const {
  return location_bar_->GetLocationBarModel()->GetVectorIcon();
}

void AIMOmniboxClient::ProcessExtensionMatch(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  // Extensions are not supported on iOS.
}

void AIMOmniboxClient::OnFocusChanged(OmniboxFocusState state,
                                      OmniboxFocusChangeReason reason) {
  // TODO(crbug.com/40534385): OnFocusChanged is not the correct place to be
  // canceling prerenders, but this is the closest match to the original
  // location of this code, which was in OmniboxViewIOS::OnDidEndEditing().  The
  // goal of this code is to cancel prerenders when the omnibox loses focus.
  // Otherwise, they will live forever in cases where the user navigates to a
  // different URL than what is prerendered.
  if (state == OMNIBOX_FOCUS_NONE) {
    PrerenderBrowserAgent* agent = PrerenderBrowserAgent::FromBrowser(browser_);
    if (agent) {
      agent->CancelPrerender();
    }
  }
}

void AIMOmniboxClient::OnUserPastedInOmniboxResultingInValidURL() {
  base::RecordAction(
      base::UserMetricsAction("Mobile.Omnibox.iOS.PastedValidURL"));

  if (!profile_->IsOffTheRecord()) {
    default_browser::NotifyOmniboxURLCopyPaste(engagement_tracker_);
  }
}

void AIMOmniboxClient::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    bool should_prerender,
    const BitmapFetchedCallback& on_bitmap_fetched) {
  if (result.empty()) {
    return;
  }

  PrerenderBrowserAgent* agent = PrerenderBrowserAgent::FromBrowser(browser_);
  if (!agent) {
    return;
  }

  const AutocompleteMatch& match = result.match_at(0);
  bool is_inline_autocomplete = !match.inline_autocompletion.empty();

  // TODO(crbug.com/40311794): When prerendering the result of a paste
  // operation, we should change the transition to LINK instead of TYPED.

  // Only prerender HISTORY_URL matches, which come from the history DB.  Do
  // not prerender other types of matches, including matches from the search
  // provider.
  if (is_inline_autocomplete &&
      match.type == AutocompleteMatchType::HISTORY_URL) {
    ui::PageTransition transition = ui::PageTransitionFromInt(
        match.transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    agent->StartPrerender(
        match.destination_url, web::Referrer(), transition,
        is_inline_autocomplete
            ? PrerenderBrowserAgent::PrerenderPolicy::kNoDelay
            : PrerenderBrowserAgent::PrerenderPolicy::kDefaultDelay);
  } else {
    agent->CancelPrerender();
  }
}

void AIMOmniboxClient::OnTextChanged(const AutocompleteMatch& current_match,
                                     bool user_input_in_progress,
                                     const std::u16string& user_text,
                                     const AutocompleteResult& result,
                                     bool has_focus) {
  const AutocompleteMatch* default_match = result.default_match();
  std::u16string text = u"";
  if (!user_input_in_progress && default_match) {
    // Handle pre-edit state where the user text is empty.
    text = default_match->fill_into_edit;
  } else {
    text = user_text;
  }
  [delegate_
      omniboxDidChangeText:text
             isSearchQuery:AutocompleteMatch::IsSearchType(current_match.type)
       userInputInProgress:user_input_in_progress];
}

void AIMOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  // If a search was done, donate the Search In Chrome intent to the OS for
  // future Siri suggestions.
  if (!profile_->IsOffTheRecord() &&
      (log->input_type == metrics::OmniboxInputType::QUERY ||
       log->input_type == metrics::OmniboxInputType::UNKNOWN)) {
    [IntentDonationHelper donateIntent:IntentType::kSearchInChrome];
  }

  engagement_tracker_->NotifyEvent(
      feature_engagement::events::kOpenUrlFromOmnibox);
}

void AIMOmniboxClient::DiscardNonCommittedNavigations() {
  location_bar_->GetWebState()
      ->GetNavigationManager()
      ->DiscardNonCommittedItems();
}

const std::u16string& AIMOmniboxClient::GetTitle() const {
  return CurrentPageExists() ? location_bar_->GetWebState()->GetTitle()
                             : base::EmptyString16();
}

gfx::Image AIMOmniboxClient::GetFavicon() const {
  return favicon::WebFaviconDriver::FromWebState(location_bar_->GetWebState())
      ->GetFavicon();
}

void AIMOmniboxClient::OnAutocompleteAccept(
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
  if (location_bar_->GetWebState()) {
    web::WebState* web_state = location_bar_->GetWebState();
    const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
    if (web_state_tracker_.find(web_state_id) == web_state_tracker_.end()) {
      scoped_observations_.AddObservation(web_state);
    }
    const ShortcutElement shortcutElement{text, match};
    web_state_tracker_.insert_or_assign(web_state_id, shortcutElement);
  }
  [delegate_ omniboxDidAcceptText:match.fill_into_edit
                   destinationURL:destination_url
                     isSearchType:AutocompleteMatch::IsSearchType(match.type)];
}

base::WeakPtr<OmniboxClient> AIMOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AIMOmniboxClient::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  ShortcutElement shortcut = web_state_tracker_.extract(web_state_id).mapped();
  scoped_observations_.RemoveObservation(web_state);

  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ios::ShortcutsBackendFactory::GetForProfile(profile_);

  // Add the shortcut if the navigation from the omnibox was successful.
  if (!navigation_context->GetError() && shortcuts_backend &&
      (navigation_context->GetPageTransition() &
       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    shortcuts_backend->AddOrUpdateShortcut(shortcut.text, shortcut.match);
  }
}

void AIMOmniboxClient::WebStateDestroyed(web::WebState* web_state) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  web_state_tracker_.erase(web_state_id);
  scoped_observations_.RemoveObservation(web_state);
}
