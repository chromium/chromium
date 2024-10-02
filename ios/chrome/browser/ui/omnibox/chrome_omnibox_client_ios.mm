// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"

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
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/autocomplete/model/shortcuts_backend_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/web_location_bar.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

ChromeOmniboxClientIOS::ChromeOmniboxClientIOS(
    WebLocationBar* location_bar,
    ProfileIOS* profile,
    feature_engagement::Tracker* tracker)
    : location_bar_(location_bar),
      profile_(profile),
      engagement_tracker_(tracker),
      web_state_tracker_() {
  CHECK(engagement_tracker_);
}

ChromeOmniboxClientIOS::~ChromeOmniboxClientIOS() {
  web_state_tracker_.clear();
}

std::unique_ptr<AutocompleteProviderClient>
ChromeOmniboxClientIOS::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(profile_);
}

bool ChromeOmniboxClientIOS::CurrentPageExists() const {
  return (location_bar_->GetWebState() != nullptr);
}

const GURL& ChromeOmniboxClientIOS::GetURL() const {
  return CurrentPageExists() ? location_bar_->GetWebState()->GetVisibleURL()
                             : GURL::EmptyGURL();
}

bool ChromeOmniboxClientIOS::IsLoading() const {
  return location_bar_->GetWebState()->IsLoading();
}

bool ChromeOmniboxClientIOS::IsPasteAndGoEnabled() const {
  return false;
}

bool ChromeOmniboxClientIOS::IsDefaultSearchProviderEnabled() const {
  // iOS does not have Enterprise policies
  return true;
}

SessionID ChromeOmniboxClientIOS::GetSessionID() const {
  return IOSChromeSessionTabHelper::FromWebState(location_bar_->GetWebState())
      ->session_id();
}

PrefService* ChromeOmniboxClientIOS::GetPrefs() {
  return profile_->GetPrefs();
}

const PrefService* ChromeOmniboxClientIOS::GetPrefs() const {
  return profile_->GetPrefs();
}

bookmarks::BookmarkModel* ChromeOmniboxClientIOS::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

AutocompleteControllerEmitter*
ChromeOmniboxClientIOS::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* ChromeOmniboxClientIOS::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(profile_);
}

const AutocompleteSchemeClassifier&
ChromeOmniboxClientIOS::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClientIOS::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(profile_);
}

bool ChromeOmniboxClientIOS::ShouldDefaultTypedNavigationsToHttps() const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int ChromeOmniboxClientIOS::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->GetHttpsPortForTesting();
}

bool ChromeOmniboxClientIOS::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return HttpsUpgradeServiceFactory::GetForProfile(profile_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image ChromeOmniboxClientIOS::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
}

std::u16string ChromeOmniboxClientIOS::GetFormattedFullURL() const {
  return location_bar_->GetLocationBarModel()->GetFormattedFullURL();
}

std::u16string ChromeOmniboxClientIOS::GetURLForDisplay() const {
  return location_bar_->GetLocationBarModel()->GetURLForDisplay();
}

GURL ChromeOmniboxClientIOS::GetNavigationEntryURL() const {
  return location_bar_->GetLocationBarModel()->GetURL();
}

metrics::OmniboxEventProto::PageClassification
ChromeOmniboxClientIOS::GetPageClassification(bool is_prefetch) const {
  return location_bar_->GetLocationBarModel()->GetPageClassification(
      is_prefetch);
}

security_state::SecurityLevel ChromeOmniboxClientIOS::GetSecurityLevel() const {
  return location_bar_->GetLocationBarModel()->GetSecurityLevel();
}

net::CertStatus ChromeOmniboxClientIOS::GetCertStatus() const {
  return location_bar_->GetLocationBarModel()->GetCertStatus();
}

const gfx::VectorIcon& ChromeOmniboxClientIOS::GetVectorIcon() const {
  return location_bar_->GetLocationBarModel()->GetVectorIcon();
}

bool ChromeOmniboxClientIOS::ProcessExtensionKeyword(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {
  // Extensions are not supported on iOS.
  return false;
}

void ChromeOmniboxClientIOS::OnFocusChanged(OmniboxFocusState state,
                                            OmniboxFocusChangeReason reason) {
  // TODO(crbug.com/40534385): OnFocusChanged is not the correct place to be
  // canceling prerenders, but this is the closest match to the original
  // location of this code, which was in OmniboxViewIOS::OnDidEndEditing().  The
  // goal of this code is to cancel prerenders when the omnibox loses focus.
  // Otherwise, they will live forever in cases where the user navigates to a
  // different URL than what is prerendered.
  if (state == OMNIBOX_FOCUS_NONE) {
    PrerenderService* service =
        PrerenderServiceFactory::GetForProfile(profile_);
    if (service) {
      service->CancelPrerender();
    }
  }
}

void ChromeOmniboxClientIOS::OnUserPastedInOmniboxResultingInValidURL() {
  base::RecordAction(
      base::UserMetricsAction("Mobile.Omnibox.iOS.PastedValidURL"));

  if (!profile_->IsOffTheRecord()) {
    default_browser::NotifyOmniboxURLCopyPaste(engagement_tracker_);
  }
}

void ChromeOmniboxClientIOS::OnResultChanged(
    const AutocompleteResult& result,
    bool default_match_changed,
    bool should_prerender,
    const BitmapFetchedCallback& on_bitmap_fetched) {
  if (result.empty()) {
    return;
  }

  PrerenderService* service = PrerenderServiceFactory::GetForProfile(profile_);
  if (!service) {
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
    service->StartPrerender(match.destination_url, web::Referrer(), transition,
                            location_bar_->GetWebState(),
                            is_inline_autocomplete);
  } else {
    service->CancelPrerender();
  }
}

void ChromeOmniboxClientIOS::OnURLOpenedFromOmnibox(OmniboxLog* log) {
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

void ChromeOmniboxClientIOS::DiscardNonCommittedNavigations() {
  location_bar_->GetWebState()
      ->GetNavigationManager()
      ->DiscardNonCommittedItems();
}

const std::u16string& ChromeOmniboxClientIOS::GetTitle() const {
  return CurrentPageExists() ? location_bar_->GetWebState()->GetTitle()
                             : base::EmptyString16();
}

gfx::Image ChromeOmniboxClientIOS::GetFavicon() const {
  return favicon::WebFaviconDriver::FromWebState(location_bar_->GetWebState())
      ->GetFavicon();
}

void ChromeOmniboxClientIOS::OnAutocompleteAccept(
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
  if (location_bar_->GetWebState()) {
    web::WebState* web_state = location_bar_->GetWebState();
    const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
    if (web_state_tracker_.find(web_state_id) == web_state_tracker_.end()) {
      scoped_observations_.AddObservation(web_state);
    }
    const ShortcutElement shortcutElement{text, match};
    web_state_tracker_.insert_or_assign(web_state_id, shortcutElement);
  }

  location_bar_->OnNavigate(destination_url, post_content, disposition,
                            transition, destination_url_entered_without_scheme,
                            match);
}

base::WeakPtr<OmniboxClient> ChromeOmniboxClientIOS::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ChromeOmniboxClientIOS::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  ShortcutElement shortcut = web_state_tracker_.extract(web_state_id).mapped();
  scoped_observations_.RemoveObservation(web_state);

  scoped_refptr<ShortcutsBackend> shortcuts_backend =
      ios::ShortcutsBackendFactory::GetInstance()->GetForProfile(profile_);

  // Add the shortcut if the navigation from the omnibox was successful.
  if (!navigation_context->GetError() && shortcuts_backend &&
      (navigation_context->GetPageTransition() &
       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    shortcuts_backend->AddOrUpdateShortcut(shortcut.text, shortcut.match);
  }
}

void ChromeOmniboxClientIOS::WebStateDestroyed(web::WebState* web_state) {
  const int32_t web_state_id = web_state->GetUniqueIdentifier().identifier();
  web_state_tracker_.erase(web_state_id);
  scoped_observations_.RemoveObservation(web_state);
}
