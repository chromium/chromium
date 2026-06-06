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
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
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
    case PageContextWrapperError::kPageNotExtractableError:
      return ios::provider::GeminiPageContextComputationState::kError;
    default:
      return ios::provider::GeminiPageContextComputationState::kError;
  }
}

}  // namespace

GeminiTabHelper::GeminiTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  optimization_guide_decider_ =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  web_state_observation_.Observe(web_state);

  if (IsZeroStateSuggestionsEnabled()) {
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
  CHECK(IsZeroStateSuggestionsEnabled());
  zero_state_suggestions_service_->FetchZeroStateSuggestions(
      std::move(callback));
}

void GeminiTabHelper::SetIsFirstRun(bool is_first_run) {
  is_first_run_ = is_first_run;
}

bool GeminiTabHelper::GetIsFirstRun() {
  return is_first_run_;
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

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:gfx::kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  current_favicon_ =
      DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
  return current_favicon_;
}

GeminiPageContext* GeminiTabHelper::GetPartialPageContext() {
  GeminiPageContext* gemini_page_context = [[GeminiPageContext alloc] init];
  gemini_page_context.favicon = GetFavicon();

  if (!CanExtractPageContextForGemini()) {
    gemini_page_context.geminiPageContextComputationState =
        ios::provider::GeminiPageContextComputationState::kBlocked;
    // Attachment state will be explicitly determined by the browser agent
    // applying user prefs.
    return gemini_page_context;
  }

  gemini_page_context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kPending;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url(current_url_.spec());
  page_context->set_title(base::UTF16ToUTF8(current_title_));
  gemini_page_context.uniquePageContext = std::move(page_context);

  return gemini_page_context;
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
  GeminiTabHelper::DeleteGeminiSessionInStorage();
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

void GeminiTabHelper::CreateOrUpdateGeminiSessionInStorage(
    std::string server_id) {
  CreateOrUpdateSessionInPrefs(GetClientId(), server_id);
}

void GeminiTabHelper::DeleteGeminiSessionInStorage() {
  CleanupSessionFromPrefs();
}

std::string GeminiTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> GeminiTabHelper::GetServerId() {
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  base::Time last_interaction_timestamp =
      pref_service->GetTime(prefs::kLastGeminiInteractionTimestamp);
  const std::string server_id =
      pref_service->GetString(prefs::kGeminiConversationId);
  if (base::Time::Now() - last_interaction_timestamp <
      BWGSessionValidityDuration()) {
    if (!server_id.empty()) {
      return server_id;
    }
  }
  return std::nullopt;
}

void GeminiTabHelper::SetGeminiCommandsHandler(id<BWGCommands> handler) {
  gemini_commands_handler_ = handler;
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

  // With copresence, AIM and NTP are ineligible, and SRP is conditionally
  // enabled.
  if (IsGeminiCopresenceEnabled()) {
    return !is_aim_url && !is_ntp;
  }

  // By default, the NTP is ineligible, and only extractable pages are eligible
  // (unless `IsGeminiFloatyAllPagesEnabled` is enabled).
  return !is_ntp && (CanExtractPageContextForWebState(web_state_) ||
                     IsGeminiFloatyAllPagesEnabled());
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
  } else if (IsGeminiCopresenceEnabled()) {
    [gemini_commands_handler_
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
  } else if (IsGeminiCopresenceEnabled()) {
    [gemini_commands_handler_
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
  if (IsGeminiCopresenceEnabled()) {
    NotifyPageContextUpdated(web_state_);
  }

  if (IsZeroStateSuggestionsEnabled()) {
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
    bool can_request_metadata =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());
    if (can_request_metadata) {
      optimization_guide_decider_->CanApplyOptimization(
          new_url_without_ref,
          optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
          base::BindOnce(&GeminiTabHelper::OnGeminiEligibilityDecision,
                         weak_ptr_factory_.GetWeakPtr(), new_url_without_ref,
                         can_request_metadata));
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
  if (IsGeminiCopresenceEnabled()) {
    if (IsGeminiAvailableForWebState()) {
      RecordGeminiPageAvailability(IOSGeminiPageAvailability::kAvailable);
    } else {
      RecordGeminiPageAvailability(IOSGeminiPageAvailability::kUnavailable);
    }
    [gemini_commands_handler_
        updateFloatyVisibilityIfEligibleAnimated:NO
                                      fromSource:gemini::FloatyUpdateSource::
                                                     WebNavigation];
  }

  const GURL& current_url = navigation_context->GetUrl().GetWithoutRef();
  if (previous_main_frame_url_ == current_url) {
    return;
  }

  if (IsGeminiCopresenceEnabled()) {
    current_title_ = web_state->GetTitle();
    NotifyPageContextUpdated(web_state_);
  }

  previous_main_frame_url_ = current_url;

  if (IsAskGeminiChipEnabled()) {
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
}

void GeminiTabHelper::TitleWasSet(web::WebState* web_state) {
  if (IsGeminiCopresenceEnabled()) {
    const std::u16string& new_title = web_state->GetTitle();
    if (new_title != current_title_) {
      current_title_ = new_title;
      NotifyPageContextUpdated(web_state);
    }
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
}

void GeminiTabHelper::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<web::FaviconURL>& candidates) {
  if (IsGeminiCopresenceEnabled()) {
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
}

void GeminiTabHelper::WebStateDestroyed(web::WebState* web_state) {
  page_context_timeout_timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_observation_.Reset();
  web_state_ = nullptr;
  if (IsAskGeminiChipEnabled()) {
    optimization_guide_decider_ = nullptr;
    latest_load_contextual_cueing_metadata_.reset();
  }
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

void GeminiTabHelper::CreateOrUpdateSessionInPrefs(std::string client_id,
                                                   std::string server_id) {
  if (client_id.empty() || server_id.empty()) {
    return;
  }

  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  pref_service->SetTime(prefs::kLastGeminiInteractionTimestamp,
                        base::Time::Now());
  pref_service->SetString(prefs::kLastGeminiInteractionURL,
                          web_state_->GetVisibleURL().spec());
  pref_service->SetString(prefs::kGeminiConversationId, server_id);
}

void GeminiTabHelper::CleanupSessionFromPrefs() {
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  pref_service->ClearPref(prefs::kGeminiConversationId);
}

void GeminiTabHelper::OnCanApplyContextualCueingDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  CHECK(IsAskGeminiChipEnabled());

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
    [gemini_commands_handler_ showBWGPromoIfPageIsEligible];
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
bool GeminiTabHelper::ComputeGeminiEligibility(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // If the optimization guide decision is not true, default to eligible.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return true;
  }

  optimization_guide::OptimizationMetadata mutable_metadata = metadata;
  auto suggestions_metadata = mutable_metadata.ParsedMetadata<
      optimization_guide::proto::GlicZeroStateSuggestionsMetadata>();

  // If no metadata is parsed successfully, default to eligible.
  if (!suggestions_metadata) {
    return true;
  }

  return suggestions_metadata->contextual_suggestions_eligible();
}

void GeminiTabHelper::OnGeminiEligibilityDecision(
    const GURL& url_without_ref,
    bool user_enabled_request_metadata,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // The URL has changed so the metadata is obsolete.
  if (url_without_ref != current_url_.GetWithoutRef()) {
    return;
  }

  const bool eligible = ComputeGeminiEligibility(decision, metadata);
  if (IsZeroStateSuggestionsEnabled()) {
    zero_state_suggestions_service_->SetCanApply(eligible);
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  if (eligible &&
      gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, profile) &&
      user_enabled_request_metadata &&
      feature_engagement::TrackerFactory::GetForProfile(profile)
          ->WouldTriggerHelpUI(
              feature_engagement::kIPHiOSGeminiImageRemixFeature) &&
      !IsUrlNtp(web_state_->GetVisibleURL())) {
    [help_commands_handler_
        presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix];
  }
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
        url_without_ref, false,
        optimization_guide::OptimizationGuideDecision::kTrue,
        optimization_guide::OptimizationMetadata());
    return;
  }

  // On demand decisions are made for users who have not enabled metadata
  // requests (MSBB).
  OnGeminiEligibilityDecision(url_without_ref, false, it->second.decision,
                              it->second.metadata);
}

bool GeminiTabHelper::CanExtractPageContextForGemini() {
  return CanExtractPageContextForWebState(web_state_) &&
         (!IsNextIaOrLiveMode() || web_state_->IsVisible());
}

bool GeminiTabHelper::IsInGeminiLiveMode() const {
  return IsGeminiLiveEnabled() && ios::provider::GetCurrentMode() ==
                                      ios::provider::GeminiViewMode::kLive;
}

bool GeminiTabHelper::IsNextIaOrLiveMode() const {
  return IsChromeNextIaEnabled() || IsInGeminiLiveMode();
}
