// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
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
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_snapshot_utils.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/zero_state_suggestions_service_impl.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

NSMutableArray<NSString*>* ZeroStateSuggestionsAsNSArray(
    std::vector<std::string> suggestions) {
  NSMutableArray<NSString*>* ns_suggestions =
      [NSMutableArray arrayWithCapacity:suggestions.size()];
  for (const std::string& suggestion : suggestions) {
    [ns_suggestions addObject:base::SysUTF8ToNSString(suggestion)];
  }
  return ns_suggestions;
}

}  // namespace

struct BwgTabHelper::ZeroStateSuggestions {
  ZeroStateSuggestions() = default;
  ~ZeroStateSuggestions() = default;

  // The zero-state suggestions service.
  mojo::Remote<ai::mojom::ZeroStateSuggestionsService> service;
  std::unique_ptr<ai::ZeroStateSuggestionsServiceImpl> service_impl;

  // The zero-state suggestions data for the current page.
  std::optional<std::vector<std::string>> suggestions;
  bool can_apply = false;
};

BwgTabHelper::BwgTabHelper(web::WebState* web_state) : web_state_(web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  optimization_guide_decider_ =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  web_state_observation_.Observe(web_state);

  if (IsZeroStateSuggestionsEnabled()) {
    zero_state_suggestions_ = std::make_unique<ZeroStateSuggestions>();
    mojo::PendingReceiver<ai::mojom::ZeroStateSuggestionsService>
        zero_state_suggestions_receiver =
            zero_state_suggestions_->service.BindNewPipeAndPassReceiver();
    zero_state_suggestions_->service_impl =
        std::make_unique<ai::ZeroStateSuggestionsServiceImpl>(
            std::move(zero_state_suggestions_receiver), web_state);
  }
}

BwgTabHelper::~BwgTabHelper() {
  for (auto& observer : observers_) {
    observer.OnGeminiTabHelperDestroyed(this);
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
  optimization_guide_decider_ = nullptr;
}

void BwgTabHelper::AddObserver(GeminiTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void BwgTabHelper::RemoveObserver(GeminiTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool BwgTabHelper::HasObserver(GeminiTabHelperObserver* observer) {
  return observers_.HasObserver(observer);
}

void BwgTabHelper::SetupPageContextGeneration(
    base::RepeatingCallback<void(PageContextWrapperCallbackResponse)>
        callback) {
  page_context_wrapper_response_ready_callback_ = std::move(callback);

  // If the page is still loading, wait for it to finish before extracting the
  // page context.
  if (web_state_->IsLoading()) {
    // TODO(crbug.com/466107255): Move waiting for page loading responsibility
    // to GeminiBrowserAgent.
    base::RepeatingCallback<void()> pageContextPopulateCallback =
        base::BindRepeating(&BwgTabHelper::PopulatePageContextFields,
                            weak_ptr_factory_.GetWeakPtr());
    SetPageLoadedCallback(std::move(pageContextPopulateCallback));
    return;
  }

  PopulatePageContextFields();
}

void BwgTabHelper::ForcePageContextGeneration() {
  if (page_loaded_callback_) {
    // Override the wait for PageLoaded.
    // Run the callback but do not reset it, so it can run again when the page
    // actually finishes loading (to get the full context).
    page_loaded_callback_.Run();
  }
}

void BwgTabHelper::ExecuteZeroStateSuggestions(
    base::OnceCallback<void(NSArray<NSString*>*)> callback) {
  CHECK(IsZeroStateSuggestionsEnabled());

  if (!zero_state_suggestions_->can_apply) {
    std::move(callback).Run(nil);
    return;
  }

  if (zero_state_suggestions_->suggestions.has_value()) {
    // Ensure the cached suggestions are for the current URL.
    if (web_state_->GetVisibleURL().GetWithoutRef() ==
        current_url_.GetWithoutRef()) {
      std::move(callback).Run(ZeroStateSuggestionsAsNSArray(
          zero_state_suggestions_->suggestions.value()));
    } else {
      // The cached suggestions are stale and thus obsolete.
      std::move(callback).Run(nil);
    }
    return;
  }

  if (!zero_state_suggestions_->service) {
    std::move(callback).Run(nil);
    return;
  }

  base::OnceCallback<void(ai::mojom::ZeroStateSuggestionsResponseResultPtr)>
      service_callback =
          base::BindOnce(&BwgTabHelper::ParseSuggestionsResponse,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  zero_state_suggestions_->service->FetchZeroStateSuggestions(
      std::move(service_callback));
}

void BwgTabHelper::SetBwgUiShowing(bool showing) {
  is_bwg_ui_showing_ = showing;

  // The UI was foregrounded, so it can no longer be active in the background.
  if (is_bwg_ui_showing_) {
    is_bwg_session_active_in_background_ = false;
  }

  // UI was hidden but the session is not active, so update the snapshot to
  // remove the overlay from it.
  if (!is_bwg_ui_showing_ && !is_bwg_session_active_in_background_) {
    cached_snapshot_ = nil;
  }
}

void BwgTabHelper::SetIsFirstRun(bool is_first_run) {
  is_first_run_ = is_first_run;
}

bool BwgTabHelper::GetIsFirstRun() {
  return is_first_run_;
}

bool BwgTabHelper::ShouldPreventContextualPanelEntryPoint() {
  return prevent_contextual_panel_entry_point_;
}

void BwgTabHelper::SetPreventContextualPanelEntryPoint(bool shouldPrevent) {
  prevent_contextual_panel_entry_point_ = shouldPrevent;
}

void BwgTabHelper::SetPageLoadedCallback(base::RepeatingClosure callback) {
  page_loaded_callback_ = std::move(callback);
}

GeminiPageContext* BwgTabHelper::GetPartialPageContext() {
  GeminiPageContext* gemini_page_context = [[GeminiPageContext alloc] init];
  gemini_page_context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kPending;
  gemini_page_context.favicon = current_favicon_;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url(current_url_.spec());
  page_context->set_title(base::UTF16ToUTF8(current_title_));
  gemini_page_context.uniquePageContext = std::move(page_context);

  return gemini_page_context;
}

bool BwgTabHelper::ShouldBlockFloatyFromShowing() {
  return is_external_overlay_presented_ || is_alert_presented_ ||
         is_banner_presented_ || is_snackbar_presented_;
}

void BwgTabHelper::UpdatePresentedSource(gemini::FloatyUpdateSource source,
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

bool BwgTabHelper::GetIsBwgSessionActiveInBackground() {
  return is_bwg_session_active_in_background_;
}

void BwgTabHelper::DeactivateBWGSession() {
  is_bwg_session_active_in_background_ = false;
  is_bwg_ui_showing_ = false;
  cached_snapshot_ = nil;
}

bool BwgTabHelper::IsLastInteractionUrlDifferent() {
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

bool BwgTabHelper::ShouldShowSuggestionChips() {
  return !google_util::IsGoogleSearchUrl(web_state_->GetVisibleURL());
}

void BwgTabHelper::CreateOrUpdateBwgSessionInStorage(std::string server_id) {
  CreateOrUpdateSessionInPrefs(GetClientId(), server_id);
}

void BwgTabHelper::DeleteBwgSessionInStorage() {
  CleanupSessionFromPrefs();
}

void BwgTabHelper::PrepareBwgFreBackgrounding() {
  if (!IsGeminiCopresenceEnabled()) {
    // TODO(crbug.com/486134176) Clean up snapshot logic to rely on the default
    // snapshot mechanism once copresence is launched.
    cached_snapshot_ =
        bwg_snapshot_utils::GetCroppedFullscreenSnapshot(web_state_->GetView());
  }
  is_bwg_session_active_in_background_ = true;
}

std::string BwgTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> BwgTabHelper::GetServerId() {
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

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

void BwgTabHelper::SetHelpCommandsHandler(id<HelpCommands> handler) {
  help_commands_handler_ = handler;
}

void BwgTabHelper::SetLocationBarBadgeCommandsHandler(
    id<LocationBarBadgeCommands> handler) {
  location_bar_badge_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_in_background_) {
    if (!IsGeminiCopresenceEnabled()) {
      [bwg_commands_handler_
          startGeminiFlowWithStartupState:
              [[GeminiStartupState alloc]
                  initWithEntryPoint:gemini::EntryPoint::TabReopen]];
    }
    cached_snapshot_ = nil;
  }

  if (IsGeminiCopresenceEnabled()) {
    [bwg_commands_handler_
        updateFloatyVisibilityIfEligibleAnimated:NO
                                      fromSource:gemini::FloatyUpdateSource::
                                                     WebNavigation];
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_ui_showing_) {
    // Only capture the window snapshot if Copresence is disabled. This ensures
    // Copresence uses the default snapshot mechanism to avoid UI corruption.
    if (!IsGeminiCopresenceEnabled()) {
      // TODO(crbug.com/486134176) Clean up snaoshot logic to rely on the
      // default snapshot mechanism once copresence is launched.
      cached_snapshot_ = bwg_snapshot_utils::GetCroppedFullscreenSnapshot(
          web_state_->GetView());
    }
    is_bwg_session_active_in_background_ = true;

    if (!IsGeminiCopresenceEnabled()) {
      [bwg_commands_handler_ dismissGeminiFlowWithCompletion:nil];
    }
  }

  UpdateWebStateSnapshotInStorage();

  if (!IsGeminiCopresenceEnabled()) {
    return;
  }

  [bwg_commands_handler_
      hideFloatyIfInvokedAnimated:NO
                       fromSource:gemini::FloatyUpdateSource::WebNavigation];
}

void BwgTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Cancel the callback that runs on page load, since we're now going to a new
  // page.
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
    ClearZeroStateSuggestions();
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  raw_ptr<BwgService> bwg_service = BwgServiceFactory::GetForProfile(profile);
  const bool gemini_available =
      bwg_service && bwg_service->IsBwgAvailableForWebState(web_state_);
  if (gemini_available &&
      profile->GetPrefs()->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    bool can_request_metadata =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            profile->IsOffTheRecord(), profile->GetPrefs());
    if (can_request_metadata) {
      optimization_guide_decider_->CanApplyOptimization(
          new_url_without_ref,
          optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
          base::BindOnce(&BwgTabHelper::OnGeminiEligibilityDecision,
                         weak_ptr_factory_.GetWeakPtr(), new_url_without_ref,
                         can_request_metadata));
    } else {
      optimization_guide_decider_->CanApplyOptimizationOnDemand(
          {new_url_without_ref},
          {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS},
          optimization_guide::proto::RequestContext::
              CONTEXT_GLIC_ZERO_STATE_SUGGESTIONS,
          base::BindRepeating(
              &BwgTabHelper::OnGeminiEligibilityOnDemandDecision,
              weak_ptr_factory_.GetWeakPtr()),
          std::nullopt);
    }
  }
}

void BwgTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsGeminiCopresenceEnabled()) {
    [bwg_commands_handler_
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

    optimization_guide_decider_->CanApplyOptimization(
        current_url, optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
        base::BindOnce(&BwgTabHelper::OnCanApplyContextualCueingDecision,
                       weak_ptr_factory_.GetWeakPtr(), current_url));
  }
}

void BwgTabHelper::TitleWasSet(web::WebState* web_state) {
  if (IsGeminiCopresenceEnabled()) {
    const std::u16string& new_title = web_state->GetTitle();
    if (new_title != current_title_) {
      current_title_ = new_title;
      NotifyPageContextUpdated(web_state);
    }
  }
}

void BwgTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (page_loaded_callback_) {
    page_loaded_callback_.Run();
    page_loaded_callback_.Reset();
  }
}

void BwgTabHelper::FaviconUrlUpdated(
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

void BwgTabHelper::WebStateDestroyed(web::WebState* web_state) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_observation_.Reset();
  web_state_ = nullptr;
  if (IsAskGeminiChipEnabled()) {
    optimization_guide_decider_ = nullptr;
    latest_load_contextual_cueing_metadata_.reset();
  }
}

#pragma mark - Private

void BwgTabHelper::PopulatePageContextFields() {
  // Cancel any ongoing page context operation.
  if (page_context_wrapper_) {
    page_context_wrapper_ = nil;
  }

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRefactoredExtractor(IsPageContextExtractorRefactoredEnabled())
          .SetGraftCrossOriginFrameContent(IsGeminiRichAPCExtractionEnabled())
          .SetUseRichExtraction(IsGeminiRichAPCExtractionEnabled())
          .Build();

  // Create a new wrapper.
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state_
                  config:config
      completionCallback:page_context_wrapper_response_ready_callback_];

  // Configure it to fetch full context.
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetSnapshot:YES];

  // Start populating the page context fields.
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void BwgTabHelper::ClearZeroStateSuggestions() {
  if (!IsZeroStateSuggestionsEnabled()) {
    return;
  }

  zero_state_suggestions_->suggestions.reset();
  zero_state_suggestions_->can_apply = false;
}

void BwgTabHelper::NotifyPageContextUpdated(web::WebState* web_state) {
  for (auto& observer : observers_) {
    observer.OnPageContextUpdated(web_state);
  }
}

void BwgTabHelper::CreateOrUpdateSessionInPrefs(std::string client_id,
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

void BwgTabHelper::CleanupSessionFromPrefs() {
  PrefService* pref_service =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
  pref_service->ClearPref(prefs::kGeminiConversationId);
}

void BwgTabHelper::UpdateWebStateSnapshotInStorage() {
  if (!cached_snapshot_) {
    return;
  }

  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);

  if (!snapshot_tab_helper) {
    return;
  }

  snapshot_tab_helper->UpdateSnapshotStorageWithImage(cached_snapshot_);
}

void BwgTabHelper::OnCanApplyContextualCueingDecision(
    const GURL& main_frame_url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  CHECK(IsAskGeminiChipEnabled());
  // The URL has changed so the metadata is obsolete.
  if (previous_main_frame_url_ != main_frame_url) {
    return;
  }

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }

  latest_load_contextual_cueing_metadata_ = metadata.ParsedMetadata<
      optimization_guide::proto::GlicContextualCueingMetadata>();

  if (!latest_load_contextual_cueing_metadata_) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  // TODO(crbug.com/461595639): Remove pref checks to fully migrate logic to
  // FET.
  bool floaty_shown = profile->GetPrefs()->GetBoolean(prefs::kIOSBwgConsent);
  bool bwg_promo_shown =
      profile->GetPrefs()->GetInteger(prefs::kIOSBWGPromoImpressionCount) > 0;
  bool should_wait_for_new_user =
      !ShouldSkipBWGPromoNewUserDelay() && IsFirstRunRecent(base::Days(1));

  // Show promo if eligible.
  if (IsGeminiNavigationPromoEnabled() && !should_wait_for_new_user &&
      !floaty_shown && !bwg_promo_shown &&
      feature_engagement::TrackerFactory::GetForProfile(profile)
          ->WouldTriggerHelpUI(
              feature_engagement::kIPHiOSGeminiFullscreenPromoFeature)) {
    [bwg_commands_handler_ showBWGPromoIfPageIsEligible];
    return;
  }

  UIImage* badge_image =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kBadgeSymbolPointSize];
  NSString* cue_label =
      l10n_util::GetNSString(IDS_IOS_ASK_GEMINI_CHIP_CUE_LABEL);
  LocationBarBadgeConfiguration* badge_config =
      [[LocationBarBadgeConfiguration alloc]
           initWithBadgeType:LocationBarBadgeType::kGeminiContextualCueChip
          accessibilityLabel:cue_label
                  badgeImage:badge_image];

  badge_config.badgeText = cue_label;
  badge_config.shouldHideBadgeAfterChipCollapse = true;
  [location_bar_badge_commands_handler_ updateBadgeConfig:badge_config];
}

// Computes Gemini eligibility based on the presence of metadata.
bool BwgTabHelper::ComputeGeminiEligibility(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // When decision == `kTrue`, then the metadata drives the computation.
  // Otherwise, eligibility defaults to true.
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    return true;
  }

  optimization_guide::OptimizationMetadata mutable_metadata = metadata;
  auto suggestions_metadata = mutable_metadata.ParsedMetadata<
      optimization_guide::proto::GlicZeroStateSuggestionsMetadata>();
  // Defaults to true for cases where there are no metadata.
  if (!suggestions_metadata) {
    return true;
  }

  return suggestions_metadata->contextual_suggestions_eligible();
}

void BwgTabHelper::OnGeminiEligibilityDecision(
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
    zero_state_suggestions_->can_apply = eligible;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (eligible && IsGeminiImageRemixToolEnabled() &&
      user_enabled_request_metadata &&
      feature_engagement::TrackerFactory::GetForProfile(profile)
          ->WouldTriggerHelpUI(
              feature_engagement::kIPHiOSGeminiImageRemixFeature) &&
      !IsUrlNtp(web_state_->GetVisibleURL())) {
    [help_commands_handler_
        presentInProductHelpWithType:InProductHelpType::kGeminiImageRemix];
  }
}

void BwgTabHelper::OnGeminiEligibilityOnDemandDecision(
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

void BwgTabHelper::ParseSuggestionsResponse(
    base::OnceCallback<void(NSArray<NSString*>*)> callback,
    ai::mojom::ZeroStateSuggestionsResponseResultPtr result) {
  if (!result || result->is_error()) {
    std::move(callback).Run(nil);
    return;
  }

  std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
      response_proto_optional =
          result->get_response()
              .As<optimization_guide::proto::ZeroStateSuggestionsResponse>();
  if (!response_proto_optional.has_value()) {
    std::move(callback).Run(nil);
    return;
  }
  optimization_guide::proto::ZeroStateSuggestionsResponse response_proto =
      response_proto_optional.value();

  zero_state_suggestions_->suggestions.emplace();
  for (const auto& suggestion : response_proto.suggestions()) {
    zero_state_suggestions_->suggestions->push_back(suggestion.label());
  }

  std::move(callback).Run(ZeroStateSuggestionsAsNSArray(
      zero_state_suggestions_->suggestions.value()));
}
