// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_utils.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"
#import "url/url_constants.h"

namespace {

// The maximum time to wait for full page load before timing out extraction.
const base::TimeDelta kFullPageContextTimeout = base::Seconds(3);

// Returns true if `mime_type` represents an extractable web page (HTML or
// Image).
bool IsExtractableMimeType(const std::string& mime_type) {
  const std::string image = "image";
  const bool is_image = mime_type.compare(0, image.size(), image) == 0;
  return is_image ||
         base::EqualsCaseInsensitiveASCII(mime_type,
                                          kHyperTextMarkupLanguageMimeType) ||
         base::EqualsCaseInsensitiveASCII(mime_type, kXHTMLMimeType) ||
         base::EqualsCaseInsensitiveASCII(mime_type, kXMLMimeType);
}

// Helper to convert PageContextWrapperError to
// GeminiPageContextComputationState.
ios::provider::GeminiPageContextComputationState
GeminiPageContextComputationStateFromPageContextWrapperError(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kForceDetachError:
      return ios::provider::GeminiPageContextComputationState::kProtected;
    case PageContextWrapperError::kPageUnsafeError:
      return ios::provider::GeminiPageContextComputationState::kBlocked;
    case PageContextWrapperError::kPageNotExtractableError:
      return ios::provider::GeminiPageContextComputationState::kError;
    default:
      return ios::provider::GeminiPageContextComputationState::kError;
  }
}

// Converts an array of ZeroStateSuggestion to an array of NSString.
NSArray<NSString*>* ConvertZeroStateSuggestionsToStrings(
    NSArray<ZeroStateSuggestion*>* suggestions) {
  if (!suggestions) {
    return nil;
  }
  NSMutableArray<NSString*>* string_suggestions =
      [NSMutableArray arrayWithCapacity:suggestions.count];
  for (ZeroStateSuggestion* suggestion in suggestions) {
    [string_suggestions addObject:suggestion.text];
  }
  return string_suggestions;
}

}  // namespace

GeminiTabHelper::GeminiTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  optimization_guide_decider_ =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  web_state_observation_.Observe(web_state);

  if (IsZeroStateSuggestionsEnabled() ||
      IsZeroStateSuggestionsCentralizationEnabled()) {
    zero_state_suggestions_service_ =
        std::make_unique<ai::ZeroStateSuggestionsService>(web_state);
  }
}

GeminiTabHelper::~GeminiTabHelper() {
  for (auto& observer : observers_) {
    observer.OnGeminiTabHelperDestroyed(this);
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
  optimization_guide_decider_ = nullptr;
}

void GeminiTabHelper::AddObserver(GeminiTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void GeminiTabHelper::RemoveObserver(GeminiTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool GeminiTabHelper::HasObserver(GeminiTabHelperObserver* observer) {
  return observers_.HasObserver(observer);
}

void GeminiTabHelper::GeneratePageContext(
    base::RepeatingCallback<void(GeminiPageContext*)> callback) {
  page_context_consumer_callback_ = std::move(callback);

  // Call back immediately if the page context cannot be extracted.
  if (!CanExtractPageContextForGemini()) {
    if (page_context_consumer_callback_) {
      page_context_consumer_callback_.Run(GetPartialPageContext());
    }
    return;
  }

  // If the page is still loading, defer extraction untl a certain time has
  // elapsed, followed by a best-effort extraction.
  if (web_state_->IsLoading()) {
    base::RepeatingCallback<void()> pageContextPopulateCallback =
        base::BindRepeating(&GeminiTabHelper::PopulatePageContextFields,
                            weak_ptr_factory_.GetWeakPtr());
    SetPageLoadedCallback(std::move(pageContextPopulateCallback));

    page_context_timeout_timer_.Start(
        FROM_HERE, kFullPageContextTimeout,
        base::BindOnce(&GeminiTabHelper::ForcePageContextGeneration,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Otherwise, extract full page context.
  PopulatePageContextFields();
}

void GeminiTabHelper::ForcePageContextGeneration() {
  page_context_timeout_timer_.Stop();
  if (page_loaded_callback_) {
    // Override the wait for PageLoaded.
    // Run the callback but do not reset it, so it can run again when the page
    // actually finishes loading (to get the full context).
    page_loaded_callback_.Run();
  }
}

void GeminiTabHelper::CancelPageContextGeneration() {
  page_context_timeout_timer_.Stop();
  page_loaded_callback_.Reset();
}

void GeminiTabHelper::ExecuteZeroStateSuggestions(
    base::OnceCallback<void(NSArray<NSString*>*)> callback) {
  CHECK(IsZeroStateSuggestionsEnabled() ||
        IsZeroStateSuggestionsCentralizationEnabled());
  if (gemini_contextual_eligibility_ == ContextualEligibility::kIneligible ||
      !zero_state_suggestions_service_) {
    std::move(callback).Run(nil);
    return;
  }

  base::OnceCallback<void(NSArray<ZeroStateSuggestion*>*)> conversion_callback =
      base::BindOnce(
          [](base::OnceCallback<void(NSArray<NSString*>*)> result_callback,
             NSArray<ZeroStateSuggestion*>* suggestions) {
            std::move(result_callback)
                .Run(ConvertZeroStateSuggestionsToStrings(suggestions));
          },
          std::move(callback));

  zero_state_suggestions_service_->FetchZeroStateSuggestions(
      std::move(conversion_callback));
}

bool GeminiTabHelper::ShouldPreventContextualPanelEntryPoint() {
  return prevent_contextual_panel_entry_point_;
}

void GeminiTabHelper::SetPreventContextualPanelEntryPoint(bool shouldPrevent) {
  prevent_contextual_panel_entry_point_ = shouldPrevent;
}

void GeminiTabHelper::SetPageLoadedCallback(base::RepeatingClosure callback) {
  page_loaded_callback_ = std::move(callback);
}

UIImage* GeminiTabHelper::GetFavicon() {
  if (current_favicon_) {
    return current_favicon_;
  }

  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state_);
  if (driver) {
    gfx::Image cached_favicon = driver->GetFavicon();
    if (!cached_favicon.IsEmpty()) {
      current_favicon_ = cached_favicon.ToUIImage();
      return current_favicon_;
    }
  }

  current_favicon_ = gemini::GetDefaultFavicon();
  return current_favicon_;
}

GeminiPageContext* GeminiTabHelper::GetPartialPageContext(bool forced) {
  bool is_eligible = forced ? CanExtractPageContextForWebState(web_state_)
                            : CanExtractPageContextForGemini();

  return gemini::CreatePartialPageContextForWebState(web_state_, is_eligible);
}

bool GeminiTabHelper::ShouldBlockFloatyFromShowing() {
  return is_external_overlay_presented_ || is_alert_presented_ ||
         is_banner_presented_ || is_snackbar_presented_;
}

void GeminiTabHelper::UpdatePresentedSource(gemini::FloatyUpdateSource source,
                                            bool is_presented) {
  switch (source) {
    case gemini::FloatyUpdateSource::Alert:
      is_alert_presented_ = is_presented;
      break;
    case gemini::FloatyUpdateSource::Banner:
      is_banner_presented_ = is_presented;
      break;
    case gemini::FloatyUpdateSource::Overlay:
      is_external_overlay_presented_ = is_presented;
      break;
    case gemini::FloatyUpdateSource::Snackbar:
      is_snackbar_presented_ = is_presented;
      break;
    default:
      break;
  }
}

void GeminiTabHelper::DeactivateGeminiSession() {
  CancelPageContextGeneration();
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  gemini::DeleteGeminiSessionInStorage(pref_service);
}

bool GeminiTabHelper::IsLastInteractionUrlDifferent() {
  std::optional<std::string> last_interaction_url;

  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  last_interaction_url =
      pref_service->GetString(prefs::kLastGeminiInteractionURL);

  if (!last_interaction_url.has_value()) {
    return true;
  }

  return !web_state_->GetVisibleURL().EqualsIgnoringRef(
      GURL(last_interaction_url.value()));
}

bool GeminiTabHelper::ShouldShowSuggestionChips() {
  return !google_util::IsGoogleSearchUrl(web_state_->GetVisibleURL());
}

std::string GeminiTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

void GeminiTabHelper::SetGeminiHandler(id<GeminiCommands> handler) {
  gemini_handler_ = handler;
}

void GeminiTabHelper::SetHelpCommandsHandler(id<HelpCommands> handler) {
  help_commands_handler_ = handler;
}

void GeminiTabHelper::SetLocationBarBadgeCommandsHandler(
    id<LocationBarBadgeCommands> handler) {
  location_bar_badge_commands_handler_ = handler;
}

bool GeminiTabHelper::IsGeminiAvailableForWebState() {
  if (IsInGeminiLiveMode()) {
    return true;
  }
  return IsGeminiChatAvailableForWebState();
}

bool GeminiTabHelper::IsContextualEntryPointAllowed() {
  // Block context-based entry points on protected URLs.
  if (web_state_ &&
      ios::provider::IsProtectedUrl(web_state_->GetLastCommittedURL().spec())) {
    return false;
  }
  return true;
}

bool GeminiTabHelper::IsGeminiChatAvailableForWebState() {
  // With NextIA, all URLs are eligible, including when there's no web state.
  if (IsChromeNextIaEnabled()) {
    return true;
  }

  if (!web_state_) {
    return false;
  }

  if (!web_state_->IsVisible()) {
    return false;
  }

  const GURL& url = web_state_->GetVisibleURL();

  bool is_ntp = IsUrlNtp(url);
  bool is_aim_url = IsAimZeroStateURL(url) || IsAimURL(url);

  // With Page Action Menu, AIM and NTP are ineligible, and SRP is conditionally
  // enabled.
  if (IsPageActionMenuEnabled()) {
    return !is_aim_url && !is_ntp;
  }

  // By default, the NTP is ineligible, and only extractable pages are eligible.
  return !is_ntp && CanExtractPageContextForWebState(web_state_);
}

IOSGeminiInvocationPageType GeminiTabHelper::GetCurrentPageType() {
  if (!web_state_) {
    return IOSGeminiInvocationPageType::kNoWebState;
  }

  const GURL& url = web_state_->GetVisibleURL();
  if (IsUrlNtp(url) || url.spec() == kChromeUIAboutNewTabURL) {
    return IOSGeminiInvocationPageType::kNewTabPage;
  }
  if (url.SchemeIs(kChromeUIScheme) || url.SchemeIs(url::kAboutScheme)) {
    return IOSGeminiInvocationPageType::kChromeInternalOther;
  }

  const std::string mime_type = web_state_->GetContentsMimeType();
  if (base::EqualsCaseInsensitiveASCII(mime_type,
                                       kAdobePortableDocumentFormatMimeType)) {
    return IOSGeminiInvocationPageType::kPdfDocument;
  }

  if (url.SchemeIsHTTPOrHTTPS() && IsExtractableMimeType(mime_type)) {
    return IOSGeminiInvocationPageType::kExtractableWebPage;
  }

  return IOSGeminiInvocationPageType::kOtherNonExtractable;
}

#pragma mark - WebStateObserver

void GeminiTabHelper::WasShown(web::WebState* web_state) {
  // In NextIA or Live mode, the floaty remains persistently visible across tab
  // switches, but the page context needs to be updated to match the newly
  // visible tab.
  if (IsNextIaOrLiveMode()) {
    NotifyPageContextUpdated(web_state);
  } else {
    [gemini_handler_
        updateFloatyVisibilityIfEligibleAnimated:NO
                                      fromSource:gemini::FloatyUpdateSource::
                                                     WebNavigation];
  }
}

void GeminiTabHelper::WasHidden(web::WebState* web_state) {
  // In NextIA or Live mode, the floaty remains persistently visible when a tab
  // is hidden (e.g., during a tab switch), but we must update the page context
  // immediately to ensure the hidden tab's content is detached and blocked.
  if (IsNextIaOrLiveMode()) {
    NotifyPageContextUpdated(web_state);
  } else {
    [gemini_handler_
        hideFloatyIfInvokedAnimated:NO
                         fromSource:gemini::FloatyUpdateSource::WebNavigation];
  }
}

void GeminiTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Reset active timers and loading state when traversing to a new location.
  page_context_timeout_timer_.Stop();
  page_loaded_callback_.Reset();

  const GURL& new_url = navigation_context->GetUrl();
  const GURL& new_url_without_ref = new_url.GetWithoutRef();
  // No change in URL means we don't need to recompute optimization guides.
  if (new_url_without_ref == current_url_.GetWithoutRef()) {
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  current_url_ = new_url;
  NotifyPageContextUpdated(web_state_);

  // Reset gemini eligibility. The eligibility is decided by the optimization
  // guide with GLIC_ZERO_STATE_SUGGESTIONS.
  gemini_contextual_eligibility_ = ContextualEligibility::kIneligible;
  if (zero_state_suggestions_service_) {
    zero_state_suggestions_service_->ClearCachedSuggestions();
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  GeminiService* gemini_service = GeminiServiceFactory::GetForProfile(profile);
  const bool gemini_available = IsGeminiAvailableForWebState() &&
                                gemini_service &&
                                gemini_service->IsProfileEligibleForGemini();

  base::UmaHistogramBoolean("IOS.Gemini.PageEligible", gemini_available);

  if (gemini_available &&
      profile->GetPrefs()->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    bool is_proactive_fetch_permitted =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());

    // `is_proactive_fetch_permitted` is true if proactive fetches are allowed
    // (i.e. MSBB enabled and non-incognito).
    if (is_proactive_fetch_permitted) {
      optimization_guide_decider_->CanApplyOptimization(
          new_url_without_ref,
          optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
          base::BindOnce(&GeminiTabHelper::OnGeminiEligibilityDecision,
                         weak_ptr_factory_.GetWeakPtr(), new_url_without_ref,
                         is_proactive_fetch_permitted));
    } else {
      optimization_guide_decider_->CanApplyOptimizationOnDemand(
          {new_url_without_ref},
          {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS},
          optimization_guide::proto::RequestContext::
              CONTEXT_GLIC_ZERO_STATE_SUGGESTIONS,
          base::BindRepeating(
              &GeminiTabHelper::OnGeminiEligibilityOnDemandDecision,
              weak_ptr_factory_.GetWeakPtr()),
          std::nullopt);
    }
  }
}

void GeminiTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsGeminiAvailableForWebState()) {
    RecordGeminiPageAvailability(IOSGeminiPageAvailability::kAvailable);
  } else {
    RecordGeminiPageAvailability(IOSGeminiPageAvailability::kUnavailable);
  }
  [gemini_handler_
      updateFloatyVisibilityIfEligibleAnimated:NO
                                    fromSource:gemini::FloatyUpdateSource::
                                                   WebNavigation];

  const GURL& current_url = navigation_context->GetUrl().GetWithoutRef();
  if (previous_main_frame_url_ == current_url) {
    return;
  }

  current_title_ = web_state->GetTitle();
  NotifyPageContextUpdated(web_state_);

  previous_main_frame_url_ = current_url;

  latest_load_contextual_cueing_metadata_.reset();

  if (!optimization_guide_decider_ || !current_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // Don't re-trigger Gemini contextual cues for same-document navigations.
  if (!navigation_context->IsSameDocument()) {
    optimization_guide_decider_->CanApplyOptimization(
        current_url, optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
        base::BindOnce(&GeminiTabHelper::OnCanApplyContextualCueingDecision,
                       weak_ptr_factory_.GetWeakPtr(), current_url));
  }
}

void GeminiTabHelper::TitleWasSet(web::WebState* web_state) {
  const std::u16string& new_title = web_state->GetTitle();
  if (new_title != current_title_) {
    current_title_ = new_title;
    NotifyPageContextUpdated(web_state);
  }
}

void GeminiTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  page_context_timeout_timer_.Stop();
  if (page_loaded_callback_) {
    page_loaded_callback_.Run();
    page_loaded_callback_.Reset();
  }

  MaybePresentImageRemixTooltip();
}

void GeminiTabHelper::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<web::FaviconURL>& candidates) {
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state);
  if (!driver) {
    return;
  }

  UIImage* new_favicon = nil;
  gfx::Image cached_favicon = driver->GetFavicon();
  if (!cached_favicon.IsEmpty()) {
    new_favicon = cached_favicon.ToUIImage();
  } else {
    UIImageConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:gfx::kFaviconSize
                            weight:UIImageSymbolWeightBold
                             scale:UIImageSymbolScaleMedium];
    new_favicon =
        DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  }

  if (new_favicon != current_favicon_ &&
      ![new_favicon isEqual:current_favicon_]) {
    current_favicon_ = new_favicon;
    NotifyPageContextUpdated(web_state_);
  }
}

void GeminiTabHelper::WebStateDestroyed(web::WebState* web_state) {
  page_context_timeout_timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_observation_.Reset();
  web_state_ = nullptr;
  optimization_guide_decider_ = nullptr;
  latest_load_contextual_cueing_metadata_.reset();
}

#pragma mark - Private

void GeminiTabHelper::PopulatePageContextFields() {
  page_context_timeout_timer_.Stop();
  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRefactoredExtractor(IsPageContextExtractorRefactoredEnabled())
          .SetGraftCrossOriginFrameContent(IsGeminiRichAPCExtractionEnabled())
          .SetUseRichExtraction(IsGeminiRichAPCExtractionEnabled())
          .SetExtractPaidContent(IsGeminiRichAPCExtractionEnabled())
          .Build();

  // Create a new wrapper.
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state_
                  config:config
      completionCallback:base::BindRepeating(
                             &GeminiTabHelper::OnPageContextWrapperResponse,
                             weak_ptr_factory_.GetWeakPtr())];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetSnapshot:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void GeminiTabHelper::OnPageContextWrapperResponse(
    PageContextWrapperCallbackResponse expected_page_context) {
  GeminiPageContext* gemini_page_context = [[GeminiPageContext alloc] init];
  gemini_page_context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kSuccess;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto =
      nullptr;

  if (expected_page_context.has_value()) {
    page_context_proto = std::move(expected_page_context.value());
  } else {
    gemini_page_context.geminiPageContextComputationState =
        GeminiPageContextComputationStateFromPageContextWrapperError(
            expected_page_context.error());
  }
  gemini_page_context.uniquePageContext = std::move(page_context_proto);
  gemini_page_context.favicon = GetFavicon();

  if (page_context_consumer_callback_) {
    page_context_consumer_callback_.Run(gemini_page_context);
  }
}

void GeminiTabHelper::NotifyPageContextUpdated(web::WebState* web_state) {
  // Cancel any ongoing page context generation which is now obsolete.
  CancelPageContextGeneration();
  for (auto& observer : observers_) {
    observer.OnPageContextUpdated(web_state);
  }
}

void GeminiTabHelper::OnCanApplyContextualCueingDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // Record every decision before checking if the url changed.
  RecordGeminiGlicContextualCueDecision(decision);

  // The URL has changed so the metadata is obsolete.
  if (previous_main_frame_url_ != main_frame_url) {
    return;
  }

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }

  latest_load_contextual_cueing_metadata_ = metadata.ParsedMetadata<
      optimization_guide::proto::GlicContextualCueingMetadata>();

  if (!web_state_ || !web_state_->IsVisible() ||
      !latest_load_contextual_cueing_metadata_) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  // TODO(crbug.com/461595639): Remove pref checks to fully migrate logic to
  // FET.
  bool floaty_shown = profile->GetPrefs()->GetBoolean(prefs::kIOSBwgConsent);
  bool should_wait_for_new_user =
      !ShouldSkipBWGPromoNewUserDelay() && IsFirstRunRecent(base::Days(1));

  // Show promo if eligible.
  if (IsGeminiNavigationPromoEnabled() && !should_wait_for_new_user &&
      !floaty_shown && !gemini::DidUserSeeGeminiPromo(profile->GetPrefs()) &&
      feature_engagement::TrackerFactory::GetForProfile(profile)
          ->WouldTriggerHelpUI(
              feature_engagement::kIPHiOSGeminiFullscreenPromoFeature)) {
    [gemini_handler_ showGeminiPromoIfPageIsEligible];
    return;
  }

  UIImage* badge_image;
  BOOL should_hide_badge_after_chip_collapse = NO;
  if (IsChromeNextIaEnabled()) {
    badge_image = CustomSymbolTemplateWithPointSize(kTextSparkSymbol,
                                                    kBadgeSymbolPointSize);
    should_hide_badge_after_chip_collapse = NO;
  } else {
    badge_image =
        [GeminiUIUtils brandedGeminiSymbolWithPointSize:kBadgeSymbolPointSize];
    should_hide_badge_after_chip_collapse = YES;
  }
  NSString* cue_label =
      l10n_util::GetNSString(IDS_IOS_ASK_GEMINI_CHIP_CUE_LABEL);
  LocationBarBadgeConfiguration* badge_config =
      [[LocationBarBadgeConfiguration alloc]
           initWithBadgeType:LocationBarBadgeType::kGeminiContextualCueChip
          accessibilityLabel:cue_label
                  badgeImage:badge_image];

  badge_config.badgeText = cue_label;
  badge_config.shouldHideBadgeAfterChipCollapse =
      should_hide_badge_after_chip_collapse;
  bool success = false;
  if ([(id)location_bar_badge_commands_handler_
          respondsToSelector:@selector(updateBadgeConfig:)]) {
    [location_bar_badge_commands_handler_ updateBadgeConfig:badge_config];
    success = true;
  }
  base::UmaHistogramBoolean("IOS.Gemini.LocationBarBadgeUpdateSuccess",
                            success);
}

// Computes Gemini eligibility based on the presence of metadata.
GeminiTabHelper::ContextualEligibility
GeminiTabHelper::ComputeGeminiEligibility(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata,
    const bool was_proactive_fetch_used) {
  bool is_eligible = true;
  // If the optimization guide decision is not true, default to eligible.
  if (decision == optimization_guide::OptimizationGuideDecision::kTrue) {
    optimization_guide::OptimizationMetadata mutable_metadata = metadata;
    auto suggestions_metadata = mutable_metadata.ParsedMetadata<
        optimization_guide::proto::GlicZeroStateSuggestionsMetadata>();

    // If metadata is parsed successfully, read eligibility.
    if (suggestions_metadata) {
      is_eligible = suggestions_metadata->contextual_suggestions_eligible();
    }
  }

  if (!is_eligible) {
    return ContextualEligibility::kIneligible;
  }
  return was_proactive_fetch_used
             ? ContextualEligibility::kEligibleViaProactiveFetch
             : ContextualEligibility::kEligibleViaOnDemandFetch;
}

void GeminiTabHelper::OnGeminiEligibilityDecision(
    const GURL& url_without_ref,
    const bool was_proactive_fetch_used,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // The URL has changed so the metadata is obsolete.
  if (url_without_ref != current_url_.GetWithoutRef()) {
    return;
  }

  gemini_contextual_eligibility_ =
      ComputeGeminiEligibility(decision, metadata, was_proactive_fetch_used);

  MaybePresentImageRemixTooltip();
}

void GeminiTabHelper::MaybePresentImageRemixTooltip() {
  if (!web_state_ || !web_state_->IsVisible() || web_state_->IsLoading()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  const bool is_eligible = gemini_contextual_eligibility_ ==
                           ContextualEligibility::kEligibleViaProactiveFetch;

  // Use the page's contextual suggestions eligibility as a proxy to ensure that
  // the Image Remix feature is only shown on a safe, eligible subset of pages.
  if (is_eligible &&
      gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, profile) &&
      feature_engagement::TrackerFactory::GetForProfile(profile)
          ->WouldTriggerHelpUI(
              feature_engagement::kIPHiOSGeminiImageRemixFeature) &&
      !IsUrlNtp(web_state_->GetVisibleURL())) {
    // Post the task on the main thread since sometimes the UI is not yet fully
    // built and the command handler is not yet registered.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GeminiTabHelper::PresentImageRemixTooltip,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void GeminiTabHelper::PresentImageRemixTooltip() {
  if (gemini_contextual_eligibility_ !=
          ContextualEligibility::kEligibleViaProactiveFetch ||
      !web_state_ || !web_state_->IsVisible() || web_state_->IsLoading()) {
    return;
  }
  [help_commands_handler_
      presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix];
}

void GeminiTabHelper::OnGeminiEligibilityOnDemandDecision(
    const GURL& url_without_ref,
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions) {
  auto it =
      decisions.find(optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS);
  if (it == decisions.end()) {
    // If the optimization type is missing, treat it as kTrue.
    // On demand decisions are made for users who have not enabled metadata
    // requests (MSBB).
    OnGeminiEligibilityDecision(
        url_without_ref, /*was_proactive_fetch_used=*/false,
        optimization_guide::OptimizationGuideDecision::kTrue,
        optimization_guide::OptimizationMetadata());
    return;
  }

  // On demand decisions are made for users who have not enabled metadata
  // requests (MSBB).
  OnGeminiEligibilityDecision(url_without_ref,
                              /*was_proactive_fetch_used=*/false,
                              it->second.decision, it->second.metadata);
}

bool GeminiTabHelper::CanExtractPageContextForGemini() {
  return CanExtractPageContextForWebState(web_state_) &&
         (!IsNextIaOrLiveMode() || web_state_->IsVisible());
}

bool GeminiTabHelper::IsInGeminiLiveMode() const {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return gemini::IsFeatureAvailable(gemini::Feature::kLive, profile) &&
         ios::provider::GetCurrentMode() ==
             ios::provider::GeminiViewMode::kLive;
}

bool GeminiTabHelper::IsNextIaOrLiveMode() const {
  return IsChromeNextIaEnabled() || IsInGeminiLiveMode();
}
