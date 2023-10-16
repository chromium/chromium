// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"

#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/omnibox_log.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_provider_client_impl.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/https_upgrades/https_upgrade_service_factory.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/ui/omnibox/web_location_bar.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

ChromeOmniboxClientIOS::ChromeOmniboxClientIOS(
    WebLocationBar* location_bar,
    ChromeBrowserState* browser_state,
    feature_engagement::Tracker* tracker)
    : location_bar_(location_bar),
      browser_state_(browser_state),
      engagement_tracker_(tracker) {
  CHECK(engagement_tracker_);
}

ChromeOmniboxClientIOS::~ChromeOmniboxClientIOS() {}

std::unique_ptr<AutocompleteProviderClient>
ChromeOmniboxClientIOS::CreateAutocompleteProviderClient() {
  return std::make_unique<AutocompleteProviderClientImpl>(browser_state_);
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
  return browser_state_->GetPrefs();
}

bookmarks::BookmarkModel* ChromeOmniboxClientIOS::GetBookmarkModel() {
  return ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
      browser_state_);
}

AutocompleteControllerEmitter*
ChromeOmniboxClientIOS::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* ChromeOmniboxClientIOS::GetTemplateURLService() {
  return ios::TemplateURLServiceFactory::GetForBrowserState(browser_state_);
}

const AutocompleteSchemeClassifier&
ChromeOmniboxClientIOS::GetSchemeClassifier() const {
  return scheme_classifier_;
}

AutocompleteClassifier* ChromeOmniboxClientIOS::GetAutocompleteClassifier() {
  return ios::AutocompleteClassifierFactory::GetForBrowserState(browser_state_);
}

bool ChromeOmniboxClientIOS::ShouldDefaultTypedNavigationsToHttps() const {
  return base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps);
}

int ChromeOmniboxClientIOS::GetHttpsPortForTesting() const {
  return HttpsUpgradeServiceFactory::GetForBrowserState(browser_state_)
      ->GetHttpsPortForTesting();
}

bool ChromeOmniboxClientIOS::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return HttpsUpgradeServiceFactory::GetForBrowserState(browser_state_)
      ->IsUsingFakeHttpsForTesting();
}

gfx::Image ChromeOmniboxClientIOS::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  // Extensions are not supported on iOS.
  return gfx::Image();
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
  // TODO(crbug.com/754050): OnFocusChanged is not the correct place to be
  // canceling prerenders, but this is the closest match to the original
  // location of this code, which was in OmniboxViewIOS::OnDidEndEditing().  The
  // goal of this code is to cancel prerenders when the omnibox loses focus.
  // Otherwise, they will live forever in cases where the user navigates to a
  // different URL than what is prerendered.
  if (state == OMNIBOX_FOCUS_NONE) {
    PrerenderService* service =
        PrerenderServiceFactory::GetForBrowserState(browser_state_);
    if (service) {
      service->CancelPrerender();
    }
  }
}

void ChromeOmniboxClientIOS::OnUserPastedInOmniboxResultingInValidURL() {
  base::RecordAction(
      base::UserMetricsAction("Mobile.Omnibox.iOS.PastedValidURL"));

  if (!browser_state_->IsOffTheRecord() &&
      HasRecentValidURLPastesAndRecordsCurrentPaste()) {
    engagement_tracker_->NotifyEvent(
        feature_engagement::events::kBlueDotPromoCriterionMet);
    engagement_tracker_->NotifyEvent(
        feature_engagement::events::kDefaultBrowserVideoPromoConditionsMet);
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

  PrerenderService* service =
      PrerenderServiceFactory::GetForBrowserState(browser_state_);
  if (!service) {
    return;
  }

  const AutocompleteMatch& match = result.match_at(0);
  bool is_inline_autocomplete = !match.inline_autocompletion.empty();

  // TODO(crbug.com/228480): When prerendering the result of a paste
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
  if (!browser_state_->IsOffTheRecord() &&
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
  location_bar_->OnNavigate(destination_url, post_content, disposition,
                            transition, destination_url_entered_without_scheme,
                            match);
}

LocationBarModel* ChromeOmniboxClientIOS::GetLocationBarModel() {
  return location_bar_->GetLocationBarModel();
}
