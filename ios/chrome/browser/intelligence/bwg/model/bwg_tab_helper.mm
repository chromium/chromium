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
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_snapshot_utils.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/zero_state_suggestions_service_impl.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Gets the session dictionary for `cliend_id` from `profile`'s prefs, if the
// session is not expired.
std::optional<const base::Value::Dict*> GetSessionDictFromPrefs(
    std::string client_id,
    ProfileIOS* profile) {
  const base::Value::Dict& sessions_map =
      profile->GetPrefs()->GetDict(prefs::kBwgSessionMap);
  if (sessions_map.empty()) {
    return std::nullopt;
  }

  const base::Value::Dict* current_session_dict =
      sessions_map.FindDict(client_id);
  if (!current_session_dict) {
    return std::nullopt;
  }

  std::optional<double> creation_timestamp =
      current_session_dict->FindDouble(kLastInteractionTimestampDictKey);
  if (!creation_timestamp) {
    return std::nullopt;
  }

  // Return the session dict if it hasn't yet expired.
  int64_t latest_valid_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch() -
      BWGSessionValidityDuration().InMilliseconds();
  if (*creation_timestamp > latest_valid_timestamp) {
    return current_session_dict;
  }

  return std::nullopt;
}

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
  GURL url;
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
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
  optimization_guide_decider_ = nullptr;
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
        zero_state_suggestions_->url) {
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

void BwgTabHelper::SetPageLoadedCallback(base::OnceClosure callback) {
  page_loaded_callback_ = std::move(callback);
}

NSString* BwgTabHelper::GetContextualCueLabel() {
  return contextual_cue_label_;
}

void BwgTabHelper::SetContextualCueLabel(NSString* cue_label) {
  contextual_cue_label_ = cue_label;
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

  if (IsGeminiCrossTabEnabled()) {
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    last_interaction_url =
        pref_service->GetString(prefs::kLastGeminiInteractionURL);
  } else {
    last_interaction_url = GetURLOnLastInteraction();
  }

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
  CleanupSessionFromPrefs(GetClientId());
}

void BwgTabHelper::PrepareBwgFreBackgrounding() {
  cached_snapshot_ =
      bwg_snapshot_utils::GetCroppedFullscreenSnapshot(web_state_->GetView());
  is_bwg_session_active_in_background_ = true;
}

std::string BwgTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> BwgTabHelper::GetServerId() {
  if (IsGeminiCrossTabEnabled()) {
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
  } else {
    std::optional<const base::Value::Dict*> session_dict =
        GetSessionDictFromPrefs(
            GetClientId(),
            ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
    if (!session_dict.has_value()) {
      return std::nullopt;
    }

    const std::string* server_id =
        session_dict.value()->FindString(kServerIDDictKey);
    if (server_id) {
      return *server_id;
    }
  }
  return std::nullopt;
}

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

void BwgTabHelper::SetSnackbarCommandsHandler(id<SnackbarCommands> handler) {
  CHECK(IsAskGeminiSnackbarEnabled() || IsWebPageReportedImagesSheetEnabled());
  snackbar_commands_handler_ = handler;
}

void BwgTabHelper::SetLocationBarBadgeCommandsHandler(
    id<LocationBarBadgeCommands> handler) {
  location_bar_badge_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_in_background_) {
    [bwg_commands_handler_
        startBWGFlowWithEntryPoint:bwg::EntryPoint::TabReopen];
    cached_snapshot_ = nil;
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_ui_showing_) {
    cached_snapshot_ =
        bwg_snapshot_utils::GetCroppedFullscreenSnapshot(web_state_->GetView());
    is_bwg_session_active_in_background_ = true;
    [bwg_commands_handler_ dismissBWGFlowWithCompletion:nil];
  }

  UpdateWebStateSnapshotInStorage();
}

void BwgTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Cancel the callback that runs on page load, since we're now going to a new
  // page.
  page_loaded_callback_.Reset();

  if (IsZeroStateSuggestionsEnabled()) {
    const GURL& current_url = navigation_context->GetUrl().GetWithoutRef();
    if (current_url != zero_state_suggestions_->url) {
      weak_ptr_factory_.InvalidateWeakPtrs();
      ClearZeroStateSuggestions();
      zero_state_suggestions_->url = current_url;
      ProfileIOS* profile =
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
      if (profile->GetPrefs()->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
        optimization_guide_decider_->CanApplyOptimization(
            current_url, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
            base::BindOnce(
                &BwgTabHelper::OnCanApplyZeroStateSuggestionsDecision,
                weak_ptr_factory_.GetWeakPtr(), current_url));
      }
    }
  }
}

void BwgTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!IsAskGeminiChipEnabled()) {
    return;
  }

  const GURL& current_url = navigation_context->GetUrl().GetWithoutRef();
  if (previous_main_frame_url_ == current_url ||
      navigation_context->IsSameDocument()) {
    return;
  }

  previous_main_frame_url_ = current_url;
  latest_load_contextual_cueing_metadata_.reset();

  if (!optimization_guide_decider_ || !current_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (IsAskGeminiChipEnabled()) {
    optimization_guide_decider_->CanApplyOptimization(
        current_url, optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
        base::BindOnce(&BwgTabHelper::OnCanApplyContextualCueingDecision,
                       weak_ptr_factory_.GetWeakPtr(), current_url));
  }
}

void BwgTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (page_loaded_callback_) {
    std::move(page_loaded_callback_).Run();
  }

  if (IsWebPageReportedImagesSheetEnabled()) {
    PrepareWebPageReportedImagesSnackbar();
  }
}

void BwgTabHelper::WebStateDestroyed(web::WebState* web_state) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_observation_.Reset();
  if (!IsGeminiCrossTabEnabled()) {
    CleanupSessionFromPrefs(GetClientId());
  }
  web_state_ = nullptr;
  if (IsAskGeminiChipEnabled()) {
    optimization_guide_decider_ = nullptr;
    latest_load_contextual_cueing_metadata_.reset();
  }
}

#pragma mark - Private

void BwgTabHelper::ClearZeroStateSuggestions() {
  if (!IsZeroStateSuggestionsEnabled()) {
    return;
  }

  zero_state_suggestions_->url = GURL();
  zero_state_suggestions_->suggestions.reset();
  zero_state_suggestions_->can_apply = false;
}

void BwgTabHelper::CreateOrUpdateSessionInPrefs(std::string client_id,
                                                std::string server_id) {
  if (client_id.empty() || server_id.empty()) {
    return;
  }

  if (IsGeminiCrossTabEnabled()) {
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    pref_service->SetTime(prefs::kLastGeminiInteractionTimestamp,
                          base::Time::Now());
    pref_service->SetString(prefs::kLastGeminiInteractionURL,
                            web_state_->GetVisibleURL().spec());
    pref_service->SetString(prefs::kGeminiConversationId, server_id);
  } else {
    base::Value::Dict session_info_dict;
    session_info_dict.Set(kServerIDDictKey, server_id);
    session_info_dict.Set(
        kLastInteractionTimestampDictKey,
        static_cast<double>(base::Time::Now().InMillisecondsSinceUnixEpoch()));
    session_info_dict.Set(kURLOnLastInteractionDictKey,
                          web_state_->GetVisibleURL().spec());

    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
    ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
    update->Set(client_id, std::move(session_info_dict));
  }
}

void BwgTabHelper::CleanupSessionFromPrefs(std::string session_id) {
  if (IsGeminiCrossTabEnabled()) {
    // TODO(crbug.com/436012307): Once this launches, remove `session_id` from
    // method.
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    pref_service->ClearPref(prefs::kGeminiConversationId);
    return;
  }

  if (session_id.empty()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
  update->Remove(session_id);
}

std::optional<std::string> BwgTabHelper::GetURLOnLastInteraction() {
  std::optional<const base::Value::Dict*> session_dict =
      GetSessionDictFromPrefs(
          GetClientId(),
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  if (!session_dict.has_value()) {
    return std::nullopt;
  }

  const std::string* last_interaction_url =
      session_dict.value()->FindString(kURLOnLastInteractionDictKey);
  if (last_interaction_url) {
    return *last_interaction_url;
  }

  return std::nullopt;
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

  // TODO (crbug.com/461595639): Remove pref checks to fully migrate logic to
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

  // Otherwise, show snackbar if eligible.
  if (IsAskGeminiSnackbarEnabled()) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = ^{
      [bwg_commands_handler_ startBWGFlowWithEntryPoint:bwg::EntryPoint::Promo];
    };
    action.title = [NSString stringWithFormat:@"✦ %@", @"Ask Gemini"];
    SnackbarMessage* message =
        [[SnackbarMessage alloc] initWithTitle:@"Ask about page?"];
    message.action = action;

    [snackbar_commands_handler_ showSnackbarMessage:message];
  } else {
    UIImage* badge_image =
        [BWGUIUtils brandedGeminiSymbolWithPointSize:kBadgeSymbolPointSize];
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
}

void BwgTabHelper::OnCanApplyZeroStateSuggestionsDecision(
    const GURL& url,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  // The URL has changed so the metadata is obsolete.
  if (url != zero_state_suggestions_->url) {
    return;
  }

  zero_state_suggestions_->can_apply =
      decision == optimization_guide::OptimizationGuideDecision::kTrue;
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

// TODO(crbug.com/456782848): Cleanup when no longer needed/wanted.
#pragma mark - Experimental. Do not use in production code.

// TODO(crbug.com/456782848): Cleanup when no longer needed/wanted.
void BwgTabHelper::PrepareWebPageReportedImagesSnackbar() {
  if (!IsWebPageReportedImagesSheetEnabled() || !web_state_) {
    return;
  }

  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();

  if (!main_frame) {
    return;
  }

  // Extract the OG image.
  main_frame->ExecuteJavaScript(
      u"(() => {"
      u"return document.querySelector('meta[property=\"og:image\"]')?.content;"
      u"})()",
      base::BindOnce(&BwgTabHelper::OnImageExtractedFromWebState,
                     weak_ptr_factory_.GetWeakPtr()));
}

// TODO(crbug.com/456782848): Cleanup when no longer needed/wanted.
void BwgTabHelper::OnImageExtractedFromWebState(const base::Value* value,
                                                NSError* error) {
  if (!IsWebPageReportedImagesSheetEnabled() || !web_state_) {
    return;
  }

  if (error) {
    DLOG(WARNING) << "Failed to fetch og:image."
                  << base::SysNSStringToUTF8([error localizedDescription]);
    return;
  }

  // Skip to the last step if no og:image was found.
  if (!value || !value->is_string()) {
    OnImageTranscoded(nil, nil);
    return;
  }

  // Fetch the image bytes.
  ImageFetchTabHelper* image_fetcher =
      ImageFetchTabHelper::FromWebState(web_state_.get());
  const GURL& lastCommittedURL = web_state_->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  if (!image_fetcher) {
    return;
  }

  image_fetcher->GetImageData(
      GURL(value->GetString()), referrer,
      base::CallbackToBlock(base::BindOnce(&BwgTabHelper::OnImageFetched,
                                           weak_ptr_factory_.GetWeakPtr())));
}

// TODO(crbug.com/456782848): Cleanup when no longer needed/wanted.
void BwgTabHelper::OnImageFetched(NSData* data) {
  if (!IsWebPageReportedImagesSheetEnabled() || !web_state_ || !data) {
    return;
  }

  image_transcoder_ = std::make_unique<web::JavaScriptImageTranscoder>();
  image_transcoder_->TranscodeImage(
      data, @"image/png", nil, nil, nil,
      base::BindOnce(&BwgTabHelper::OnImageTranscoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

// TODO(crbug.com/456782848): Cleanup when no longer needed/wanted.
void BwgTabHelper::OnImageTranscoded(NSData* png_data, NSError* error) {
  image_transcoder_ = nullptr;
  if (!IsWebPageReportedImagesSheetEnabled() || !web_state_) {
    return;
  }

  if (error) {
    DLOG(WARNING) << "Failed to transcode og:image."
                  << base::SysNSStringToUTF8([error localizedDescription]);
    return;
  }

  UIWindow* web_state_window = web_state_->GetView().window;
  UIViewController* parentVC = web_state_window.rootViewController;
  ProceduralBlock present_sheet = ^{
    if (!parentVC) {
      return;
    }

    // Create the presentation sheet.
    UIViewController* sheet = [[UIViewController alloc] init];
    sheet.view.backgroundColor = [UIColor blackColor];

    // Prepare the image if it exists.
    UIImage* image = nil;
    if (png_data) {
      image = [UIImage imageWithData:png_data];
      UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
      imageView.contentMode = UIViewContentModeScaleAspectFit;
      imageView.frame = sheet.view.bounds;
      imageView.autoresizingMask =
          UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
      [sheet.view addSubview:imageView];
    }

    // Prepare the label and its constraints.
    NSString* labelText =
        image ? [NSString stringWithFormat:@"og:image %.0fw x %.0fh",
                                           image.size.width, image.size.height]
              : @"no og:image reported";
    UILabel* label = [[UILabel alloc] init];
    label.text = labelText;
    label.font = [UIFont boldSystemFontOfSize:16];
    label.textColor = [UIColor whiteColor];
    label.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.6];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [sheet.view addSubview:label];
    [NSLayoutConstraint activateConstraints:@[
      [label.centerXAnchor constraintEqualToAnchor:sheet.view.centerXAnchor],
      [label.topAnchor
          constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.topAnchor],
    ]];

    [parentVC presentViewController:sheet animated:YES completion:nil];
  };

  // Show a snackbar which shows a sheet as action.
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:png_data ? @"og:image detected" : @"No og:image detected"];
  if (png_data) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = present_sheet;
    action.title = @"View image";
    message.action = action;
  }

  [snackbar_commands_handler_ showSnackbarMessage:message];
}
