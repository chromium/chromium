// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_scroll_observer.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_startup_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

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

// The floaty has innate padding which causes the floaty to be farther away from
// the bottom toolbar. To properly position the floaty closer to the toolbar,
// this constant is used to remove some of that innate padding.
const CGFloat kFloatyIntrinsicPaddingCorrection = 8.0;

// Used for forcing fullscreen progress value.
const CGFloat kFullscreenEnabled = 0.0;

// Used for forcing non-fullscreen progress value.
const CGFloat kFullscreenDisabled = 1.0;

// Used for the duration of the floaty animation when changing opacity.
const CGFloat kFloatyAnimationDuration = 0.1;

// Opacity for a shown floaty.
const CGFloat kFloatyShownOpacity = 1.0;

// Opacity for a hidden floaty.
const CGFloat kFloatyHiddenOpacity = 0.0;

// The timeout for the fullscreen disabler.
const double kFullscreenDisablerTimeoutSeconds = 3.0;

// Used to check if floaty visibility updates are part of a UIView dismissal or
// presentation.
const double kViewTransitionTime = 0.8;

// The maximum time to wait for full page load before timing out.
// Timeout will cause page context to be generated without waiting for full page
// load.
const base::TimeDelta kFullPageContextTimeout = base::Seconds(3);

// Block accepted by -startGeminiFREWithCompletion:
using BlockWithSuccess = void (^)(BOOL success);

// Returns a BlockWithSuccess that call `closure` if called with YES.
BlockWithSuccess BlockRunningClosureIfSuccess(base::RepeatingClosure closure) {
  return base::CallbackToBlock(base::BindRepeating(
      [](const base::RepeatingClosure& closure, BOOL success) {
        if (success) {
          closure.Run();
        }
      },
      std::move(closure)));
}

// Type of the block expected by NSNotificationCenter.
using NotificationCenterBlock = void (^)(NSNotification*);

// Returns a NotificationCenterBlock that ignores its arguments and
// invokes closure.
NotificationCenterBlock ClosureToNotificationCenterBlock(
    base::RepeatingClosure closure) {
  return base::CallbackToBlock(
      base::IgnoreArgs<NSNotification*>(std::move(closure)));
}

}  // namespace

GeminiBrowserAgent::GeminiBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  browser_->AddObserver(this);
  if (IsGeminiCopresenceEnabled()) {
    StartObserving(browser_);

    pref_change_registrar_.Init(browser_->GetProfile()->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kIOSBWGPageContentSetting,
        base::BindRepeating(&GeminiBrowserAgent::OnPageContentPrefChanged,
                            base::Unretained(this)));
  }

  bwg_gateway_ = ios::provider::CreateBWGGateway();

  if (bwg_gateway_) {
    gemini_link_opening_handler_ = [[GeminiLinkOpeningHandler alloc]
        initWithURLLoader:UrlLoadingBrowserAgent::FromBrowser(browser_)
               dispatcher:browser_->GetCommandDispatcher()];
    gemini_page_state_change_handler_ = [[GeminiPageStateChangeHandler alloc]
        initWithPrefService:browser_->GetProfile()->GetPrefs()];
    bwg_gateway_.pageStateChangeHandler = gemini_page_state_change_handler_;

    bwg_session_handler_ = [[GeminiSessionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    if (IsGeminiCopresenceEnabled()) {
      gemini_view_state_handler_ =
          [[GeminiViewStateChangeHandler alloc] initWithTarget:this];
      bwg_session_handler_.geminiViewStateDelegate = gemini_view_state_handler_;
      gemini_link_opening_handler_.geminiViewStateDelegate =
          gemini_view_state_handler_;
    }
    bwg_gateway_.sessionHandler = bwg_session_handler_;
    bwg_gateway_.linkOpeningHandler = gemini_link_opening_handler_;

    gemini_suggestion_handler_ = [[GeminiSuggestionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    bwg_gateway_.suggestionHandler = gemini_suggestion_handler_;

    if (gemini::IsFeatureAvailable(gemini::Feature::kImageRemix,
                                   browser_->GetProfile())) {
      gemini_camera_handler_ = [[GeminiCameraHandler alloc]
          initWithPrefService:browser_->GetProfile()->GetPrefs()];
      bwg_gateway_.cameraHandler = gemini_camera_handler_;
    }

    if (IsGeminiActorEnabled() && IsActorEnabled()) {
      gemini_actuation_handler_ = [[GeminiActuationHandler alloc]
          initWithActorService:actor::ActorServiceFactory::GetForProfile(
                                   browser_->GetProfile())];
      bwg_gateway_.actuationHandler = gemini_actuation_handler_;
    }

    ConfigureGemini();
  }

  // Sets up observation of fullscreen state.
  if (IsGeminiCopresenceEnabled()) {
    if (IsFullscreenRefactoringEnabled()) {
      FullscreenBrowserAgent* agent =
          FullscreenBrowserAgent::FromBrowser(browser_);
      CHECK(agent);
      fullscreen_observation_.Observe(agent);
    } else {
      FullscreenController::CreateForBrowser(browser_);
      fullscreen_controller_ = FullscreenController::FromBrowser(browser_);
      CHECK(fullscreen_controller_);
      fullscreen_controller_->AddObserver(this);
    }

    keyboard_show_observer_ = [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardWillShowNotification
                    object:nil
                     queue:nil
                usingBlock:ClosureToNotificationCenterBlock(base::BindRepeating(
                               &GeminiBrowserAgent::OnKeyboardStateChanged,
                               weak_factory_.GetWeakPtr(),
                               /*is_visible=*/true))];

    keyboard_hide_observer_ = [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardWillHideNotification
                    object:nil
                     queue:nil
                usingBlock:ClosureToNotificationCenterBlock(base::BindRepeating(
                               &GeminiBrowserAgent::OnKeyboardStateChanged,
                               weak_factory_.GetWeakPtr(),
                               /*is_visible=*/false))];

    scroll_observer_ = [[GeminiScrollObserver alloc]
        initWithScrollCallback:base::BindRepeating(
                                   &GeminiBrowserAgent::OnScrollEvent,
                                   weak_factory_.GetWeakPtr())];

    identity_manager_ =
        IdentityManagerFactory::GetForProfile(browser_->GetProfile());
    if (identity_manager_) {
      identity_manager_->AddObserver(this);
    }
  }
}

GeminiBrowserAgent::~GeminiBrowserAgent() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }

  if (browser_) {
    browser_->RemoveObserver(this);
  }

  [gemini_link_opening_handler_ disconnect];
  gemini_link_opening_handler_ = nil;

  [gemini_view_state_handler_ disconnect];
  gemini_view_state_handler_ = nil;

  gemini_actuation_handler_ = nil;

  if (keyboard_show_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:keyboard_show_observer_];
    keyboard_show_observer_ = nil;
  }
  if (keyboard_hide_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:keyboard_hide_observer_];
    keyboard_hide_observer_ = nil;
  }

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (active_web_state) {
    [active_web_state->GetWebViewProxy().scrollViewProxy
        removeObserver:scroll_observer_];
  }
  scroll_observer_ = nil;

  if (fullscreen_controller_) {
    fullscreen_controller_->RemoveObserver(this);
    fullscreen_controller_ = nullptr;
  }

  if (IsGeminiCopresenceWithFullscreenDisablerEnabled()) {
    ResetFullscreenDisabler();
  }

  StopObserving();
}

void GeminiBrowserAgent::BrowserDestroyed(Browser* browser) {
  [gemini_link_opening_handler_ disconnect];
  gemini_link_opening_handler_ = nil;

  gemini_actuation_handler_ = nil;

  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }

  browser->RemoveObserver(this);
}

void GeminiBrowserAgent::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  signin::PrimaryAccountChangeEvent::Type event_type =
      event.GetEventTypeFor(signin::ConsentLevel::kSignin);

  if (event_type == signin::PrimaryAccountChangeEvent::Type::kSet) {
    ConfigureGemini();
  }

  CHECK(IsGeminiCopresenceEnabled());
  if (event_type != signin::PrimaryAccountChangeEvent::Type::kNone) {
    browser_->GetProfile()->GetPrefs()->ClearPref(prefs::kGeminiConversationId);

    if (is_floaty_invoked_) {
      ForceDismissFloaty();
    }
  }
}

void GeminiBrowserAgent::ConfigureGemini() {
  if (!IsGeminiDynamicSettingsEnabled()) {
    return;
  }

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  if (!auth_service || !auth_service->HasPrimaryIdentity()) {
    return;
  }

  GeminiStartupConfiguration* config =
      [[GeminiStartupConfiguration alloc] init];
  config.authService = auth_service;
  config.gateway = bwg_gateway_;
  config.imageRemixEnabled = gemini::IsFeatureAvailable(
      gemini::Feature::kImageRemix, browser_->GetProfile());

  ios::provider::ConfigureWithStartupConfiguration(config);
}

void GeminiBrowserAgent::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
    if (is_floaty_invoked_) {
      ForceDismissFloaty();
    }
  }
}

void GeminiBrowserAgent::OnKeyboardStateChanged(bool is_visible) {
  CHECK(IsGeminiCopresenceEnabled());
  if (is_visible == is_keyboard_visible_) {
    return;
  }

  is_keyboard_visible_ = is_visible;
  if (is_visible) {
    // If the floaty is expanded but not thinking or temporarily hidden, the
    // floaty should not be re-shown on keyboard updates.
    bool is_expanded_not_thinking =
        last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
        ios::provider::GetCurrentClientMode() !=
            ios::provider::GeminiClientMode::kThinking;
    if (is_expanded_not_thinking || is_floaty_temporarily_hidden_) {
      return;
    }

    is_hidden_by_keyboard_ = true;
    HideFloatyIfInvoked(/*animated=*/false,
                        gemini::FloatyUpdateSource::Keyboard);
    return;
  }

  if (IsOnlyHiddenByKeyboard()) {
    ShowFloatyIfInvoked(/*animated=*/false,
                        gemini::FloatyUpdateSource::Keyboard);
    is_hidden_by_keyboard_ = false;
  }
}

void GeminiBrowserAgent::FullscreenProgressUpdatedForAnimation() {
  if (FullscreenController* controller =
          FullscreenController::FromBrowser(browser_)) {
    FullscreenProgressUpdated(controller, controller->GetProgress() < 0.5
                                              ? kFullscreenEnabled
                                              : kFullscreenDisabled);
  }
}

void GeminiBrowserAgent::ShowSignInRequiredSnackbar(
    gemini::EntryPoint entry_point) {
  RecordSignInRequiredSnackbarShown(entry_point);
  id<SnackbarCommands> snackbar_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SnackbarCommands);
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_GEMINI_SIGN_IN_REQUIRED_SNACKBAR)];
  [snackbar_handler showSnackbarMessage:message];
}

void GeminiBrowserAgent::StartGeminiFlow(UIViewController* base_view_controller,
                                         GeminiStartupState* startup_state) {
  gemini::EntryPoint entry_point = startup_state.entryPoint;
  bool will_show_first_run = !HasCompletedFirstRun();
  RecordGeminiEntryPointClick(entry_point, will_show_first_run);

  // TODO(crbug.com/507509815): Link to Gemini sign in flow.
  if (IsAppStoreInAppEventsEnabled() &&
      entry_point == gemini::EntryPoint::ExternalAppStoreEvent) {
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
    if (!auth_service || !auth_service->HasPrimaryIdentity()) {
      ShowSignInRequiredSnackbar(entry_point);
      return;
    }
  }

  // Check if the user has already consented or if the consent flow should be
  // skipped.
  bool skip_consent = BWGPromoConsentVariationsParam() ==
                      BWGPromoConsentVariations::kSkipConsent;

  if (!will_show_first_run || skip_consent) {
    PresentFloaty(base_view_controller, startup_state,
                  /*was_first_run_shown=*/false);
    return;
  }

  id<BWGCommands> gemini_commands_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);

  auto present_floaty_closure = base::BindRepeating(
      &GeminiBrowserAgent::PresentFloaty, weak_factory_.GetWeakPtr(),
      base_view_controller, startup_state, /*first_run_shown=*/true);

  [gemini_commands_handler
      startGeminiFREWithCompletion:BlockRunningClosureIfSuccess(
                                       std::move(present_floaty_closure))
                    fromEntryPoint:entry_point];
}

bool GeminiBrowserAgent::HasCompletedFirstRun() {
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();

  // If we are forcing the FRE, reset the consent pref and return false.
  if (BWGPromoConsentVariationsParam() ==
      BWGPromoConsentVariations::kForceFRE) {
    gemini::ResetGeminiConsent(pref_service);
    return false;
  }

  return pref_service->GetBoolean(prefs::kIOSBwgConsent);
}

CGFloat GeminiBrowserAgent::GetFloatyOffset() {
  CHECK(IsFullscreenInitialized());
  CGFloat max_bottom_inset =
      IsFullscreenRefactoringEnabled()
          ? FullscreenBrowserAgent::FromBrowser(browser_)->max_insets().bottom
          : fullscreen_controller_->GetMaxViewportInsets().bottom;

  SceneState* scene_state = browser_->GetSceneState();

  if (!IsFullscreenRefactoringEnabled() && IsChromeNextIaEnabled()) {
    // The legacy FullscreenController is unaware of the App Bar's height.
    // If the App Bar is at the bottom, explicitly account for it to ensure
    // the floaty positions correctly above it.
    LayoutGuideCenter* layout_guide_center =
        LayoutGuideCenterForScene(scene_state);
    UIView* app_bar_view =
        [layout_guide_center referencedViewUnderName:kAppBarGuide];
    if (app_bar_view &&
        scene_state.layoutState.appBarPosition == AppBarPosition::kBottom) {
      max_bottom_inset += kAppBarHeight;
    }
  }

  if (scene_state && scene_state.window && IsLandscape(scene_state.window)) {
    max_bottom_inset += scene_state.window.safeAreaInsets.bottom;
  }

  CGFloat offset = (max_bottom_inset * GetFloatyProgress()) -
                   kFloatyIntrinsicPaddingCorrection;

  return offset;
}

CGFloat GeminiBrowserAgent::GetFloatyProgress() {
  if (IsFullscreenRefactoringEnabled()) {
    // If there is a collapsing bottom toolbar, track the bottom progress.
    // Otherwise (e.g., in landscape where there is no bottom toolbar), fall
    // back to tracking the top progress.
    FullscreenBrowserAgent* agent =
        FullscreenBrowserAgent::FromBrowser(browser_);
    return (agent->max_insets().bottom > 0) ? agent->bottom_progress()
                                            : agent->top_progress();
  }
  return fullscreen_controller_->GetProgress();
}

void GeminiBrowserAgent::InvokeFloaty(GeminiConfiguration* config) {
  if (!IsGeminiCopresenceEnabled()) {
    ios::provider::StartBwgOverlay(config);
    return;
  }

  PrepareFloatyToBeShown();
  ios::provider::StartBwgOverlay(config);
  last_shown_view_state_ = ios::provider::GetCurrentGeminiViewState();
  is_floaty_invoked_ = true;
}

void GeminiBrowserAgent::ForceShowFloatyIfInvoked() {
  if (!is_floaty_invoked_ || !IsFullscreenInitialized()) {
    return;
  }

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                kFloatyShownOpacity);
  is_floaty_temporarily_hidden_ = false;
}

bool GeminiBrowserAgent::ShouldShowFloatyForSource(
    gemini::FloatyUpdateSource source) {
  bool is_source_query_response =
      source == gemini::FloatyUpdateSource::ForcedFromQueryResponse;

  // Re-show the floaty if a user receives a query response.
  return is_floaty_temporarily_hidden_ ? !is_source_query_response
                                       : is_source_query_response;
}

GeminiPageContext* GeminiBrowserAgent::CreateGeminiPageContext(
    ios::provider::GeminiPageContextComputationState computation_state,
    std::unique_ptr<optimization_guide::proto::PageContext>
        page_context_proto) {
  GeminiPageContext* page_context = [[GeminiPageContext alloc] init];
  page_context.geminiPageContextComputationState = computation_state;
  page_context.uniquePageContext = std::move(page_context_proto);
  page_context.favicon = FetchPageFavicon();
  ApplyUserPrefsToPageContext(page_context);
  return page_context;
}

void GeminiBrowserAgent::UpdateActiveTabHelperWithPresentedSource(
    gemini::FloatyUpdateSource source,
    bool is_presented) {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper) {
    return;
  }
  gemini_tab_helper->UpdatePresentedSource(source, is_presented);
}

void GeminiBrowserAgent::UpdateForTraitCollection(
    UITraitCollection* traitCollection) {
  if (is_floaty_temporarily_hidden_) {
    return;
  }

  // Update the offset for a device orientation update to landscape or portrait.
  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::PresentFloaty(UIViewController* base_view_controller,
                                       GeminiStartupState* startup_state,
                                       bool first_run_shown) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Trigger zero state suggestions.
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper) {
    return;
  }

  if (IsZeroStateSuggestionsAskGeminiEnabled()) {
    gemini_tab_helper->ExecuteZeroStateSuggestions(base::DoNothing());
  }

  base::WeakPtr<GeminiBrowserAgent> weak_ptr = weak_factory_.GetWeakPtr();

  // Present the overlay immediately without page context.
  PresentFloatyWithPendingContext(base_view_controller, startup_state);

  base::UmaHistogramLongTimes(first_run_shown ? kStartupTimeWithFREHistogram
                                              : kStartupTimeNoFREHistogram,
                              base::TimeTicks::Now() - start_time);

  if (CanExtractPageContextForWebState(web_state)) {
    // Start the timeout timer to force page context generation if page load
    // takes too long.
    page_context_timeout_timer_.Start(
        FROM_HERE, kFullPageContextTimeout,
        base::BindOnce(
            &GeminiBrowserAgent::TriggerBestEffortPageContextGeneration,
            weak_factory_.GetWeakPtr()));

    gemini_tab_helper->SetupPageContextGeneration(base::BindRepeating(
        &GeminiBrowserAgent::CancelTimeoutAndUpdateFloatyPageContext,
        weak_factory_.GetWeakPtr()));
  } else {
    GeminiPageContext* gemini_page_context =
        gemini_tab_helper->GetPartialPageContext();
    ApplyUserPrefsToPageContext(gemini_page_context);
    ios::provider::UpdatePageContext(gemini_page_context);
  }
}

void GeminiBrowserAgent::PresentFloatyWithPendingContext(
    UIViewController* base_view_controller,
    GeminiStartupState* startup_state) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (!active_web_state) {
    return;
  }

  std::unique_ptr<optimization_guide::proto::PageContext> partial_page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  partial_page_context->set_url(active_web_state->GetVisibleURL().spec());
  partial_page_context->set_title(
      base::UTF16ToUTF8(active_web_state->GetTitle()));

  ios::provider::GeminiPageContextComputationState computation_state =
      ios::provider::GeminiPageContextComputationState::kPending;
  if (!CanExtractPageContextForWebState(active_web_state)) {
    computation_state =
        IsGeminiFloatyAllPagesEnabled()
            ? ios::provider::GeminiPageContextComputationState::kBlocked
            : ios::provider::GeminiPageContextComputationState::kError;
  }

  PresentFloatyWithState(base_view_controller, std::move(partial_page_context),
                         computation_state, startup_state);
}

void GeminiBrowserAgent::UpdateFloatyPageContext(
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
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
  gemini_page_context.favicon = FetchPageFavicon();

  ApplyUserPrefsToPageContext(gemini_page_context);
  ios::provider::UpdatePageContext(gemini_page_context);
}

void GeminiBrowserAgent::CancelTimeoutAndUpdateFloatyPageContext(
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  page_context_timeout_timer_.Stop();
  UpdateFloatyPageContext(std::move(expected_page_context));
}

void GeminiBrowserAgent::OnViewStateChanged(
    ios::provider::GeminiViewState view_state) {
  if (view_state == ios::provider::GeminiViewState::kExpanded) {
    UpdateGeminiPageContext();
  }
}

void GeminiBrowserAgent::OnProcessingStatusChanged(
    ios::provider::GeminiClientMode processing_status) {
  // TODO(crbug.com/504758406): Update context on speaking state when available.
  if (!IsGeminiLiveEnabled() ||
      ios::provider::GetCurrentMode() != ios::provider::GeminiViewMode::kLive) {
    return;
  }
  switch (processing_status) {
    case ios::provider::GeminiClientMode::kListening:
      UpdateGeminiPageContext();
      break;
    case ios::provider::GeminiClientMode::kDormant:
      ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kFloaty,
                                  /*animated=*/true);
      break;
    default:
      // No-op.
      break;
  }
}

void GeminiBrowserAgent::CollapseFloatyIfInvoked() {
  if (!is_floaty_invoked_) {
    return;
  }

  ios::provider::UpdateGeminiViewState(
      ios::provider::GeminiViewState::kCollapsed, /*animated=*/true);
}

void GeminiBrowserAgent::SetLastShownViewState(
    ios::provider::GeminiViewState view_state) {
  if (view_state == ios::provider::GeminiViewState::kHidden ||
      view_state == last_shown_view_state_) {
    return;
  }

  if (view_state == ios::provider::GeminiViewState::kExpanded) {
    PrepareFloatyToBeShown();
    RecordFloatyCollapsedToExpanded();
    RecordFloatyMinimizedTime(elapsed_minimized_floaty_time_);
    elapsed_minimized_floaty_time_ = base::TimeTicks();
  } else if (view_state == ios::provider::GeminiViewState::kCollapsed) {
    if (IsGeminiCopresenceWithFullscreenDisablerEnabled()) {
      ResetFullscreenDisabler();
    }
    RecordFloatyExpandedToCollapsed();
    elapsed_minimized_floaty_time_ = base::TimeTicks::Now();
  }
  last_shown_view_state_ = view_state;
}

void GeminiBrowserAgent::DismissGeminiFromOtherWindows(
    base::OnceClosure completion) {
  // Collect all browsers (excluding the current one) for all profiles.
  std::vector<base::WeakPtr<Browser>> other_browsers;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
    const std::set<Browser*>& browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
    for (Browser* browser : browsers) {
      if (browser == browser_) {
        continue;
      }
      other_browsers.push_back(browser->AsWeakPtr());
    }
  }

  if (other_browsers.empty()) {
    std::move(completion).Run();
    return;
  }

  // Gate the completion behind this barrier closure which executes it when all
  // other browsers have dismissed their Gemini sessions.
  base::RepeatingClosure barrier =
      base::BarrierClosure(other_browsers.size(), std::move(completion));

  // Dismiss Gemini in all the other browsers for all profiles.
  for (base::WeakPtr<Browser> browser : other_browsers) {
    if (!browser) {
      barrier.Run();
      continue;
    }
    id<BWGCommands> gemini_commands_handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), BWGCommands);
    [gemini_commands_handler
        dismissGeminiFlowWithCompletion:base::CallbackToBlock(barrier)];
  }
}

void GeminiBrowserAgent::DismissFloaty() {
  if (IsGeminiCopresenceWithFullscreenDisablerEnabled()) {
    ResetFullscreenDisabler();
  }

  // If the floaty is temporarily hidden i.e. as part of a view controller being
  // shown underneath the Gemini floaty, don't clean up and reset internal
  // Gemini properties. Clean up should occur if a user taps the floaty to
  // dismiss it or the browser agent destructing.
  if (is_floaty_temporarily_hidden_) {
    return;
  }

  RecordFloatyDismissedState(last_shown_view_state_);

  is_floaty_invoked_ = false;
  active_hiding_sources_.clear();
  is_hidden_by_keyboard_ = false;
  elapsed_minimized_floaty_time_ = base::TimeTicks();
  // TODO(crbug.com/484045717): Refactor to merge these two provider calls.
  if (IsGeminiCopresenceEnabled()) {
    ios::provider::UpdateGeminiViewState(
        ios::provider::GeminiViewState::kHidden,
        /*animated=*/false);
  } else {
    ios::provider::ResetGemini();
  }
}

void GeminiBrowserAgent::ForceDismissFloaty() {
  is_floaty_temporarily_hidden_ = false;
  DismissFloaty();
}

bool GeminiBrowserAgent::ShouldSourceReshowFloaty(
    gemini::FloatyUpdateSource source) const {
  if (!IsGeminiCopresenceTrackSourcesEnabled()) {
    return false;
  }

  switch (source) {
    case gemini::FloatyUpdateSource::Unknown:
    case gemini::FloatyUpdateSource::ContextMenu:
    case gemini::FloatyUpdateSource::WebContextMenu:
    case gemini::FloatyUpdateSource::IneligibleSite:
    case gemini::FloatyUpdateSource::SearchRelatedPage:
    case gemini::FloatyUpdateSource::ForcedFromQueryResponse:
    case gemini::FloatyUpdateSource::TabGrid:
    case gemini::FloatyUpdateSource::Banner:
    case gemini::FloatyUpdateSource::Alert:
    case gemini::FloatyUpdateSource::Snackbar:
    case gemini::FloatyUpdateSource::Overlay:
    case gemini::FloatyUpdateSource::ForcedFromScroll:
    case gemini::FloatyUpdateSource::WebNavigation:
    case gemini::FloatyUpdateSource::GestureIph:
      return false;
    case gemini::FloatyUpdateSource::ViewTransition:
    case gemini::FloatyUpdateSource::Keyboard:

      return true;
  }
}

void GeminiBrowserAgent::HideFloatyIfInvoked(
    bool animated,
    gemini::FloatyUpdateSource source) {
  UpdateActiveTabHelperWithPresentedSource(source, /*is_presented=*/true);

  if (!is_floaty_invoked_) {
    return;
  }

  floaty_hidden_timestamp_ = base::TimeTicks::Now();
  if (ShouldSourceReshowFloaty(source)) {
    active_hiding_sources_.insert(source);
  }

  if (is_floaty_temporarily_hidden_) {
    return;
  }

  is_floaty_temporarily_hidden_ = true;
  ios::provider::GeminiViewState current_view_state =
      ios::provider::GetCurrentGeminiViewState();
  SetLastShownViewState(current_view_state);
  RecordFloatyHiddenFromSource(source);

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                kFloatyHiddenOpacity);
}

void GeminiBrowserAgent::ShowFloatyIfInvoked(
    bool animated,
    gemini::FloatyUpdateSource source) {
  UpdateActiveTabHelperWithPresentedSource(source, /*is_presented=*/false);

  if (!is_floaty_invoked_ || !ShouldShowFloatyForSource(source)) {
    return;
  }

  // `HideFloatyIfInvoked()` may be called when a view controller
  // dismisses. If a view controller dismisses as part of presenting another
  // view controller, the floaty should not show.
  base::TimeDelta time_since_last_hidden =
      base::TimeTicks::Now() - floaty_hidden_timestamp_;
  bool triggered_during_transition =
      time_since_last_hidden <= base::Seconds(kViewTransitionTime);

  // Web navigations should not be seen as a transition as an old WebState can
  // be hidden quickly followed by a new WebState being shown where
  // hiding/showing the floaty are valid invocations.
  bool is_web_navigation = source == gemini::FloatyUpdateSource::WebNavigation;

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  bool should_block =
      gemini_tab_helper && gemini_tab_helper->ShouldBlockFloatyFromShowing();
  if ((!is_web_navigation && triggered_during_transition) || should_block) {
    return;
  }

  active_hiding_sources_.erase(source);
  if (is_web_navigation) {
    active_hiding_sources_.clear();
  }

  if (DoesFloatyHaveActiveHidingSources()) {
    return;
  }

  RecordGeminiViewStateHiddenToShown(last_shown_view_state_);
  RecordFloatyShownFromSource(source);
  is_floaty_temporarily_hidden_ = false;

  // Exit fullscreen to prepare floaty for incoming response stream.
  if (source == gemini::FloatyUpdateSource::ForcedFromQueryResponse) {
    PrepareFloatyToBeShown();
  }

  [UIView animateWithDuration:kFloatyAnimationDuration
                   animations:base::CallbackToBlock(base::BindRepeating(
                                  &GeminiBrowserAgent::ForceShowFloatyIfInvoked,
                                  weak_factory_.GetWeakPtr()))];
}

#pragma mark - TabsDependencyInstaller

void GeminiBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  // No-op. We only observe the active WebState, handled in
  // OnActiveWebStateChanged.
}

void GeminiBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  // No-op. If the active WebState is removed, OnActiveWebStateChanged will
  // handle the detachment.
}

void GeminiBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // No-op, handled by OnWebStateRemoved or OnGeminiTabHelperDestroyed.
}

void GeminiBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                                 web::WebState* new_active) {
  if (old_active) {
    GeminiTabHelper* old_tab_helper = GeminiTabHelper::FromWebState(old_active);
    if (old_tab_helper) {
      old_tab_helper->RemoveObserver(this);
    }
    [old_active->GetWebViewProxy().scrollViewProxy
        removeObserver:scroll_observer_];
  }

  if (new_active) {
    GeminiTabHelper* new_tab_helper = GetActiveTabHelper(new_active);
    if (new_tab_helper) {
      new_tab_helper->AddObserver(this);
      // Propagate the context of the new active tab.
      OnPageContextUpdated(new_active);
    }
    [new_active->GetWebViewProxy().scrollViewProxy
        addObserver:scroll_observer_];

    if (!IsGeminiChatPersistenceEnabled() || !is_floaty_invoked_) {
      return;
    }

    ios::provider::RequestUIChange(
        ios::provider::GeminiUIElementType::kZeroState);
  }
}

void GeminiBrowserAgent::OnScrollEvent() {
  if (!is_floaty_invoked_ || is_keyboard_visible_) {
    return;
  }

  // Catch-all in case the floaty is still in a temporarily hidden state. A
  // fullscreen update implies a user is interacting with the web page,
  // therefore we should force-show the floaty if invoked. Uses the command
  // handler to do eligibility checks outside of this browser agent before
  // showing the floaty.
  if (is_floaty_temporarily_hidden_) {
    id<BWGCommands> gemini_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);
    [gemini_handler
        updateFloatyVisibilityIfEligibleAnimated:NO
                                      fromSource:gemini::FloatyUpdateSource::
                                                     ForcedFromScroll];
    return;
  }
}

#pragma mark - GeminiTabHelperObserver

void GeminiBrowserAgent::OnPageContextUpdated(web::WebState* web_state) {
  GeminiTabHelper* tab_helper = GetActiveTabHelper(web_state);
  if (!tab_helper || (!is_floaty_invoked_ && IsGeminiCopresenceEnabled())) {
    return;
  }

  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  ApplyUserPrefsToPageContext(gemini_page_context);

  // This update is from the active tab. Propagate it to the provider.
  ios::provider::UpdatePageContext(gemini_page_context);
}

void GeminiBrowserAgent::OnGeminiTabHelperDestroyed(
    GeminiTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
}

#pragma mark - FullscreenControllerObserver

void GeminiBrowserAgent::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  if (!is_floaty_invoked_ || is_floaty_temporarily_hidden_) {
    return;
  }

  // Avoids fullscreen updates while the keyboard is being used with the
  // floaty. Happens when the omnibox is in the bottom toolbar and the omnibox
  // is minimized as part of the keyboard being displayed.
  if (last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
      is_keyboard_visible_) {
    return;
  }

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(), progress);
}

void GeminiBrowserAgent::FullscreenWillAnimate(FullscreenController* controller,
                                               FullscreenAnimator* animator) {
  [animator addAnimations:
                base::CallbackToBlock(base::BindRepeating(
                    &GeminiBrowserAgent::FullscreenProgressUpdatedForAnimation,
                    weak_factory_.GetWeakPtr()))];
}

void GeminiBrowserAgent::FullscreenDidAnimate(FullscreenController* controller,
                                              FullscreenAnimatorStyle style) {
  if (style == FullscreenAnimatorStyle::ENTER_FULLSCREEN) {
    FullscreenProgressUpdated(controller, kFullscreenEnabled);
  } else {
    FullscreenProgressUpdated(controller, kFullscreenDisabled);
  }
}

bool GeminiBrowserAgent::DoesFloatyHaveActiveHidingSources() const {
  if (!IsGeminiCopresenceTrackSourcesEnabled()) {
    return false;
  }
  return !active_hiding_sources_.empty();
}

bool GeminiBrowserAgent::IsOnlyHiddenByKeyboard() const {
  if (!IsGeminiCopresenceTrackSourcesEnabled()) {
    return is_hidden_by_keyboard_;
  }
  return active_hiding_sources_.size() == 1 &&
         active_hiding_sources_.contains(gemini::FloatyUpdateSource::Keyboard);
}

void GeminiBrowserAgent::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  controller->RemoveObserver(this);
  fullscreen_controller_ = nullptr;
}

void GeminiBrowserAgent::FullscreenViewportInsetRangeChanged(
    FullscreenController* controller,
    UIEdgeInsets min_viewport_insets,
    UIEdgeInsets max_viewport_insets) {
  FullscreenProgressUpdated(controller, controller->GetProgress());
}

#pragma mark - FullscreenBrowserAgentObserver

void GeminiBrowserAgent::WillUpdateState(FullscreenBrowserAgent* agent) {
  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::DidUpdateObscuredInsetRange(
    FullscreenBrowserAgent* agent) {
  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::WillShutDown(FullscreenBrowserAgent* agent) {
  fullscreen_observation_.Reset();
}

#pragma mark - Private

void GeminiBrowserAgent::UpdateGeminiPageContext() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);

  if (tab_helper) {
    if (CanExtractPageContextForWebState(active_web_state)) {
      tab_helper->SetupPageContextGeneration(
          base::BindRepeating(&GeminiBrowserAgent::UpdateFloatyPageContext,
                              weak_factory_.GetWeakPtr()));
    } else {
      GeminiPageContext* gemini_page_context =
          tab_helper->GetPartialPageContext();
      ApplyUserPrefsToPageContext(gemini_page_context);
      ios::provider::UpdatePageContext(gemini_page_context);
    }
  }
  // Show page attachment UI chip every time the floaty is expanded.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

void GeminiBrowserAgent::PrepareFloatyToBeShown() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!IsFullscreenInitialized() || !web_state) {
    return;
  }

  if (!IsGeminiCopresenceWithFullscreenDisablerEnabled()) {
    if (IsFullscreenRefactoringEnabled()) {
      [HandlerForProtocol(browser_->GetCommandDispatcher(), FullscreenCommands)
          exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::
                                        kUserInitiatedFinishedByCode
                           animated:YES];
    } else {
      fullscreen_controller_->ExitFullscreen();
    }
    return;
  }

  CRWWebViewScrollViewProxy* scroll_view_proxy =
      web_state->GetWebViewProxy().scrollViewProxy;
  CGPoint current_offset = scroll_view_proxy.contentOffset;
  [scroll_view_proxy setContentOffset:current_offset animated:NO];
  fullscreen_disabler_ =
      IsFullscreenRefactoringEnabled()
          ? std::make_unique<ScopedFullscreenDisabler>(
                HandlerForProtocol(browser_->GetCommandDispatcher(),
                                   FullscreenCommands),
                /*animated=*/true)
          : std::make_unique<ScopedFullscreenDisabler>(fullscreen_controller_);
  fullscreen_disabler_timer_.Start(
      FROM_HERE, base::Seconds(kFullscreenDisablerTimeoutSeconds),
      base::BindOnce(&GeminiBrowserAgent::ResetFullscreenDisabler,
                     weak_factory_.GetWeakPtr()));
}

bool GeminiBrowserAgent::IsFullscreenInitialized() {
  return IsFullscreenRefactoringEnabled()
             ? FullscreenBrowserAgent::FromBrowser(browser_) != nullptr
             : fullscreen_controller_ != nullptr;
}

void GeminiBrowserAgent::ResetFullscreenDisabler() {
  if (!fullscreen_disabler_) {
    return;
  }

  fullscreen_disabler_timer_.Stop();
  fullscreen_disabler_.reset();
}

void GeminiBrowserAgent::PresentFloatyWithState(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto,
    ios::provider::GeminiPageContextComputationState computation_state,
    GeminiStartupState* startup_state) {
  gemini::EntryPoint entry_point = startup_state.entryPoint;
  UIImage* image_attachment = startup_state.imageAttachment;
  NSString* prepopulated_prompt = startup_state.prepopulatedPrompt;
  // If floaty is invoked, update the persisted floaty instead of restarting the
  // Gemini instance.
  if (IsGeminiCopresenceEnabled() && is_floaty_invoked_) {
    GeminiPageContext* pageContext = CreateGeminiPageContext(
        computation_state, std::move(page_context_proto));
    if (image_attachment) {
      ios::provider::AttachImage(image_attachment);
    }
    ios::provider::UpdatePageContext(pageContext);
    if (prepopulated_prompt) {
      ios::provider::UpdatePromptAction(entry_point, prepopulated_prompt);
    }
    ForceShowFloatyIfInvoked();
    ios::provider::UpdateGeminiViewState(
        ios::provider::GeminiViewState::kExpanded, /*animated=*/true);
    return;
  }

  SetSessionCommandHandlers();
  [gemini_page_state_change_handler_
      setBaseViewController:base_view_controller];

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();

  GeminiConfiguration* config = [[GeminiConfiguration alloc] init];
  config.baseViewController = base_view_controller;
  config.authService =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  config.gateway = bwg_gateway_;
  config.imageAttachment = image_attachment;

  // Use the tab helper to set the initial floaty state, which includes the chat
  // IDs and whether it was backgrounded.
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  config.clientID = base::SysUTF8ToNSString(gemini_tab_helper->GetClientId());
  std::optional<std::string> maybe_server_id = gemini_tab_helper->GetServerId();
  config.serverID =
      maybe_server_id ? base::SysUTF8ToNSString(*maybe_server_id) : nil;
  config.shouldAnimatePresentation = YES;
  config.lastInteractionURLDifferent =
      gemini_tab_helper->IsLastInteractionUrlDifferent();
  config.shouldShowSuggestionChips =
      gemini_tab_helper->ShouldShowSuggestionChips();
  config.contextualCueChipLabel = prepopulated_prompt;
  config.entryPoint = entry_point;
  config.imageRemixIPHShouldShow =
      entry_point == gemini::EntryPoint::ImageRemixIPH;
  // TODO(crbug.com/481733906): Remove once ios_internal has migrated to
  // GeminiStartupConfiguration.
  config.imageRemixEnabled = gemini::IsFeatureAvailable(
      gemini::Feature::kImageRemix, browser_->GetProfile());

  // Set the location permission state.
  // TODO(crbug.com/426207968): Populate with actual value.
  config.geminiLocationPermissionState =
      ios::provider::GeminiLocationPermissionState::kUnknown;

  // Set the page context itself and page context computation/attachment state
  // for the current web state.
  config.pageContext =
      CreateGeminiPageContext(computation_state, std::move(page_context_proto));
  if (IsGeminiCopresenceEnabled()) {
    config.initialBottomOffset = GetFloatyOffset();
  }
  config.hostWindowScene = browser_->GetSceneState().scene;

  // Start the overlay and update the tab helper to reflect this.
  DismissGeminiFromOtherWindows(base::BindOnce(
      &GeminiBrowserAgent::InvokeFloaty, weak_factory_.GetWeakPtr(), config));
}

UIImage* GeminiBrowserAgent::FetchPageFavicon() {
  // Use the cached favicon of the web state. If it's not available, use a
  // default favicon instead.
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  gfx::Image cached_favicon =
      favicon::WebFaviconDriver::FromWebState(web_state)->GetFavicon();
  if (!cached_favicon.IsEmpty()) {
    return cached_favicon.ToUIImage();
  }
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:gfx::kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  return DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
}

void GeminiBrowserAgent::ApplyUserPrefsToPageContext(
    GeminiPageContext* gemini_page_context) {
  // Disable the page context attachment state based on user prefs.
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kUserDisabled;
  } else if (IsGeminiCopresenceEnabled() && is_floaty_invoked_ &&
             ios::provider::GetCurrentPageContextAttachmentState() ==
                 ios::provider::GeminiPageContextAttachmentState::kDetached) {
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kDetached;
  } else {
    // If page context is not disabled by the user, page context is always
    // available and should be attached. Note page context is only partially
    // available (e.g. title, url, favicon) while
    // `GeminiPageContextComputationState` is pending.
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kAttached;
  }
}

void GeminiBrowserAgent::TriggerBestEffortPageContextGeneration() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper || !active_web_state || !active_web_state->IsLoading()) {
    return;
  }
  tab_helper->ForcePageContextGeneration();
}

void GeminiBrowserAgent::SetSessionCommandHandlers() {
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
  id<BWGCommands> gemini_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);

  bwg_session_handler_.settingsHandler = settings_handler;
  bwg_session_handler_.geminiHandler = gemini_handler;
}

void GeminiBrowserAgent::OnPageContentPrefChanged() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper) {
    return;
  }

  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  ApplyUserPrefsToPageContext(gemini_page_context);

  ios::provider::UpdatePageContext(gemini_page_context);

  // Trigger UI update for the attachment chip.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

GeminiTabHelper* GeminiBrowserAgent::GetActiveTabHelper(
    web::WebState* web_state) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (active_web_state && active_web_state == web_state) {
    GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
    if (tab_helper) {
      return tab_helper;
    }
  }
  return nullptr;
}
