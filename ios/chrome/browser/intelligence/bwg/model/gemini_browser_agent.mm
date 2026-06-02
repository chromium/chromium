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
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_consent_provider_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_scroll_observer.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
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
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
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
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
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
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

// The floaty has innate padding which causes the floaty to be farther away from
// the bottom toolbar. To properly position the floaty closer to the toolbar,
// this constant is used to remove some of that innate padding.
const CGFloat kFloatyIntrinsicPaddingCorrection = 8.0;

// The vertical offset clearance required to position the dormant Live session
// snackbar cleanly above the floaty pill. Note this includes the full floaty
// height.
// TODO(crbug.com/512576285): Confirm offset value with UI.
// TODO(crbug.com/513881624): Get the actual floaty height separately, if
// possible, so this constant can just represent the offset.
const CGFloat kDormantSnackbarOffsetFromFloaty = 100.0;

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

// Returns a NotificationCenterBlock that ignores its arguments and invokes
// closure.
NotificationCenterBlock ClosureToNotificationCenterBlock(
    base::RepeatingClosure closure) {
  return base::CallbackToBlock(
      base::IgnoreArgs<NSNotification*>(std::move(closure)));
}

}  // namespace

@interface GeminiSceneStateObserver
    : NSObject <SceneStateObserver, IncognitoStateObserver>

- (instancetype)initWithBrowserAgent:(GeminiBrowserAgent*)browserAgent
                          sceneState:(SceneState*)sceneState;

- (void)disconnect;

@end

@implementation GeminiSceneStateObserver {
  raw_ptr<GeminiBrowserAgent> _browserAgent;
  __weak SceneState* _sceneState;
}

- (instancetype)initWithBrowserAgent:(GeminiBrowserAgent*)browserAgent
                          sceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _browserAgent = browserAgent;
    _sceneState = sceneState;
    [_sceneState addObserver:self];
    [_sceneState.incognitoState addObserver:self];
  }
  return self;
}

- (void)disconnect {
  [_sceneState removeObserver:self];
  [_sceneState.incognitoState removeObserver:self];
  _browserAgent = nullptr;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (_browserAgent) {
    _browserAgent->OnSceneActivationLevelChanged(level);
  }
}

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  if (_browserAgent) {
    _browserAgent->OnWillEnterIncognito();
  }
}

@end

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

    gemini_consent_provider_handler_ = [[GeminiConsentProviderHandler alloc]
        initWithPrefService:browser_->GetProfile()->GetPrefs()];
    bwg_gateway_.consentProviderHandler = gemini_consent_provider_handler_;

    if (gemini::IsFeatureAvailable(gemini::Feature::kImageRemix,
                                   browser_->GetProfile())) {
      gemini_camera_handler_ = [[GeminiCameraHandler alloc]
          initWithPrefService:browser_->GetProfile()->GetPrefs()];
      bwg_gateway_.cameraHandler = gemini_camera_handler_;
    }

    if (IsGeminiActorEnabled() && IsActorEnabled()) {
      gemini_actuation_handler_ = [[GeminiActuationHandler alloc]
          initWithActorService:actor::ActorServiceFactory::GetForProfile(
                                   browser_->GetProfile())
                  webStateList:browser_->GetWebStateList()];
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

    SceneState* scene_state = browser_->GetSceneState();
    if (scene_state) {
      scene_state_observer_ =
          [[GeminiSceneStateObserver alloc] initWithBrowserAgent:this
                                                      sceneState:scene_state];
    }

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
  last_known_gemini_availability_ = IsGeminiAvailableForActiveWebState();
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
  gemini_consent_provider_handler_ = nil;

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
  [scene_state_observer_ disconnect];
  scene_state_observer_ = nil;

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

void GeminiBrowserAgent::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GeminiBrowserAgent::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool GeminiBrowserAgent::IsGeminiAvailableForActiveWebState() const {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  return tab_helper && tab_helper->IsGeminiAvailableForWebState();
}

bool GeminiBrowserAgent::IsGeminiChatAvailableForActiveWebState() const {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(web_state);
  return tab_helper && tab_helper->IsGeminiChatAvailableForWebState();
}

bool GeminiBrowserAgent::IsInGeminiLiveMode() const {
  return IsGeminiLiveEnabled() && ios::provider::GetCurrentMode() ==
                                      ios::provider::GeminiViewMode::kLive;
}

void GeminiBrowserAgent::UpdateGeminiAvailability() {
  bool available = IsGeminiAvailableForActiveWebState();
  if (available != last_known_gemini_availability_) {
    last_known_gemini_availability_ = available;
    for (auto& observer : observers_) {
      observer.OnGeminiAvailabilityChanged(available);
    }
  }
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
    // floaty should not be hidden on keyboard updates. However, focusing the
    // omnibox should always hide the floaty. While standard Chat mode is
    // collapsed when minimized (bypassing this check), Live mode operates in
    // the kExpanded state while active. Checking `is_omnibox_focused` ensures
    // that Live mode is also successfully hidden when focusing the omnibox.
    if (ShouldIgnoreKeyboardUpdate()) {
      return;
    }

    is_hidden_by_keyboard_ = true;
    HideFloatyIfInvoked(/*animated=*/false,
                        gemini::FloatyUpdateSource::Keyboard);
    return;
  }

  if (IsOnlyHiddenByKeyboard()) {
    if (IsOmniboxFocused()) {
      return;
    }
    ShowFloatyIfInvoked(/*animated=*/false,
                        gemini::FloatyUpdateSource::Keyboard);
    is_hidden_by_keyboard_ = false;
  }
}

void GeminiBrowserAgent::OnSceneActivationLevelChanged(
    SceneActivationLevel level) {
  if (level == SceneActivationLevelBackground) {
    if (is_floaty_invoked_ && IsInGeminiLiveMode()) {
      ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kFloaty,
                                  /*animated=*/false);
    }
  }
  UpdateGeminiLiveIconVisibility();
}

void GeminiBrowserAgent::OnWillEnterIncognito() {
  ForceDismissFloaty();
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

void GeminiBrowserAgent::ShowLiveSessionDormantSnackbar() {
  PrepareFloatyToBeShown();
  fullscreen_disabler_timer_.Stop();

  id<SnackbarCommands> snackbar_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SnackbarCommands);
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_GEMINI_LIVE_CONTINUE_SESSION_SNACKBAR)];
  base::WeakPtr<GeminiBrowserAgent> weak_self = weak_factory_.GetWeakPtr();
  message.completionHandler = ^(BOOL completed) {
    if (weak_self) {
      weak_self->SetIsShowingLiveSessionDormantSnackbar(false);
    }
  };

  CGFloat floaty_offset = GetFullyExpandedFloatyOffset();
  CGFloat snackbar_offset = floaty_offset + kDormantSnackbarOffsetFromFloaty;

  [snackbar_handler showSnackbarMessage:message bottomOffset:snackbar_offset];
}

void GeminiBrowserAgent::SetIsShowingLiveSessionDormantSnackbar(bool showing) {
  is_showing_live_session_dormant_snackbar_ = showing;
  if (!showing) {
    ResetFullscreenDisabler();
  }
}

void GeminiBrowserAgent::StartGeminiFlow(UIViewController* base_view_controller,
                                         GeminiStartupState* startup_state) {
  gemini::EntryPoint entry_point = startup_state.entryPoint;
  bool will_show_first_run = !HasCompletedFirstRun();
  RecordGeminiEntryPointClick(entry_point, will_show_first_run);
  RecordInvocationPageType();

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

void GeminiBrowserAgent::UpdateGeminiLiveIconVisibility() {
  if (IsChromeNextIaEnabled()) {
    return;
  }
  CGFloat progress = 1.0;
  if (fullscreen_controller_) {
    progress = fullscreen_controller_->GetProgress();
  }
  BOOL visible = IsInGeminiLiveMode() && (progress < 0.1);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  if ([dispatcher dispatchingForProtocol:@protocol(OmniboxCommands)]) {
    id<OmniboxCommands> omnibox_handler =
        HandlerForProtocol(dispatcher, OmniboxCommands);
    [omnibox_handler setCustomLeadingViewVisible:visible animated:YES];
  }
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

CGFloat GeminiBrowserAgent::GetFullyExpandedFloatyOffset() {
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

  CGFloat offset = max_bottom_inset - kFloatyIntrinsicPaddingCorrection;

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
  for (auto& observer : observers_) {
    observer.OnFloatyInvokedChanged(is_floaty_invoked_);
  }
  UpdateGeminiLiveIconVisibility();
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

void GeminiBrowserAgent::UpdateActiveTabHelperWithPresentedSource(
    gemini::FloatyUpdateSource source,
    bool is_presented) {
  if (ShouldIgnoreUpdateForDormantSnackbar(source)) {
    return;
  }
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

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper || !web_state) {
    return;
  }

  // Fetch zero-state suggestions while the floaty is being presented.
  if (IsZeroStateSuggestionsEnabled()) {
    gemini_tab_helper->ExecuteZeroStateSuggestions(base::DoNothing());
  }

  // Get partial page context, which is synchronously available to allow for the
  // floaty to be presented immediately.
  GeminiPageContext* initial_page_context =
      gemini_tab_helper->GetPartialPageContext();

  // Set up the presentation, depending on whether the floaty is already
  // invoked.
  gemini::EntryPoint entry_point = startup_state.entryPoint;
  UIImage* image_attachment = startup_state.imageAttachment;
  NSString* prepopulated_prompt = startup_state.prepopulatedPrompt;

  if (IsGeminiCopresenceEnabled() && is_floaty_invoked_) {
    if (image_attachment) {
      ios::provider::AttachImage(image_attachment);
    }
    PropagatePageContextToProvider(initial_page_context);
    if (prepopulated_prompt) {
      ios::provider::UpdatePromptAction(entry_point, prepopulated_prompt);
    }
    ForceShowFloatyIfInvoked();
    ios::provider::UpdateGeminiViewState(
        ios::provider::GeminiViewState::kExpanded, /*animated=*/true);
  } else {
    SetSessionCommandHandlers();
    [gemini_page_state_change_handler_
        setBaseViewController:base_view_controller];

    ApplyUserPrefsToPageContext(initial_page_context);
    GeminiConfiguration* config = CreateGeminiConfiguration(
        base_view_controller, startup_state, web_state, initial_page_context);

    DismissGeminiFromOtherWindows(base::BindOnce(
        &GeminiBrowserAgent::InvokeFloaty, weak_factory_.GetWeakPtr(), config));
  }

  base::UmaHistogramLongTimes(first_run_shown ? kStartupTimeWithFREHistogram
                                              : kStartupTimeNoFREHistogram,
                              base::TimeTicks::Now() - start_time);

  // Request full page context generation, which will update the floaty once
  // it's available.
  gemini_tab_helper->GeneratePageContext(base::BindRepeating(
      &GeminiBrowserAgent::OnPageContextGenerated, weak_factory_.GetWeakPtr()));
}

void GeminiBrowserAgent::OnProcessingStatusChanged(
    ios::provider::GeminiClientMode processing_status) {
  UpdateGeminiLiveIconVisibility();
  if (!IsInGeminiLiveMode()) {
    return;
  }

  processing_status_ = processing_status;
  switch (processing_status) {
    case ios::provider::GeminiClientMode::kTranscribing:
      RequestPageContextGeneration();
      break;
    case ios::provider::GeminiClientMode::kResponding: {
      // Update partial page context (i.e., live sharing context label) when
      // transitioning out of the transcribing (i.e., speaking) state.
      UpdateFloatyWithPartialPageContext();
      break;
    }
    case ios::provider::GeminiClientMode::kDormant:
      is_showing_live_session_dormant_snackbar_ = true;
      ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kFloaty,
                                  /*animated=*/true);
      ShowLiveSessionDormantSnackbar();
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

void GeminiBrowserAgent::OnGeminiLiveUserDidBargeIn() {
  processing_status_ = ios::provider::GeminiClientMode::kTranscribing;
  RequestPageContextGeneration();
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

void GeminiBrowserAgent::OnLiveButtonTapped() {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile());
  if (tracker) {
    tracker->NotifyEvent(feature_engagement::events::kIOSGeminiLiveUsed);
  }
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

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile());
  if (tracker) {
    if (has_triggered_gemini_live_iph_) {
      tracker->Dismissed(feature_engagement::kIPHiOSGeminiLiveIPHFeature);
      has_triggered_gemini_live_iph_ = false;
    }
    if (has_triggered_gemini_live_new_badge_) {
      tracker->Dismissed(feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature);
      has_triggered_gemini_live_new_badge_ = false;
    }
  }

  // TODO(crbug.com/517583120): Remove when the temporary actuation prototype is
  // cleaned up.
  if (IsGeminiActorEnabled() && IsActorEnabled()) {
    if (actor::ActorService* actor_service =
            actor::ActorServiceFactory::GetForProfile(browser_->GetProfile())) {
      actor_service->StopAllTasks();
    }
  }

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (tab_helper) {
    tab_helper->CancelPageContextGeneration();
  }

  RecordFloatyDismissedState(last_shown_view_state_);

  is_floaty_invoked_ = false;
  for (auto& observer : observers_) {
    observer.OnFloatyInvokedChanged(is_floaty_invoked_);
  }
  active_hiding_sources_.clear();
  is_hidden_by_keyboard_ = false;
  processing_status_ = ios::provider::GeminiClientMode::kUnknown;
  elapsed_minimized_floaty_time_ = base::TimeTicks();
  // TODO(crbug.com/484045717): Refactor to merge these two provider calls.
  if (IsGeminiCopresenceEnabled()) {
    ios::provider::UpdateGeminiViewState(
        ios::provider::GeminiViewState::kHidden,
        /*animated=*/false);
  } else {
    ios::provider::ResetGemini();
  }
  UpdateGeminiLiveIconVisibility();
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

bool GeminiBrowserAgent::ShouldIgnoreUpdateForDormantSnackbar(
    gemini::FloatyUpdateSource source) const {
  return is_showing_live_session_dormant_snackbar_ &&
         source == gemini::FloatyUpdateSource::Snackbar;
}

void GeminiBrowserAgent::HideFloatyIfInvoked(
    bool animated,
    gemini::FloatyUpdateSource source) {
  if (ShouldIgnoreUpdateForDormantSnackbar(source)) {
    return;
  }
  UpdateActiveTabHelperWithPresentedSource(source, /*is_presented=*/true);

  if (!is_floaty_invoked_) {
    return;
  }

  if (IsInGeminiLiveMode()) {
    UpdateLiveModeUI();
    // In Gemini Live mode, the overlay is persistent. Navigation, tab grid
    // transitions, or entering an ineligible page should not temporarily hide
    // the floaty overlay.
    if (source == gemini::FloatyUpdateSource::WebNavigation ||
        source == gemini::FloatyUpdateSource::TabGrid ||
        source == gemini::FloatyUpdateSource::IneligibleSite) {
      return;
    }
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
  if (ShouldIgnoreUpdateForDormantSnackbar(source)) {
    return;
  }
  UpdateActiveTabHelperWithPresentedSource(source, /*is_presented=*/false);

  if (!is_floaty_invoked_) {
    return;
  }

  if (IsInGeminiLiveMode()) {
    UpdateLiveModeUIAndMaybeContext();
    if (source == gemini::FloatyUpdateSource::WebNavigation) {
      return;
    }
    ForceShowFloatyIfInvoked();
    return;
  }

  if (!ShouldShowFloatyForSource(source)) {
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

void GeminiBrowserAgent::OnWebStateInserted(web::WebState* web_state) {}

void GeminiBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {}

void GeminiBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {}

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

    if (IsGeminiChatPersistenceEnabled() && is_floaty_invoked_) {
      ios::provider::RequestUIChange(
          ios::provider::GeminiUIElementType::kZeroState);
    }
  }

  UpdateLiveModeUI();
  UpdateGeminiAvailability();
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
  UpdateGeminiAvailability();

  if (IsInGeminiLiveMode()) {
    // Update page context for Gemini Live only when the user is not speaking,
    // as when they start wording their query, the page context should be locked
    // in.
    if (processing_status_ == ios::provider::GeminiClientMode::kTranscribing) {
      return;
    }
    if (UpdateLiveModeUIAndMaybeContext()) {
      return;
    }
  }

  GeminiTabHelper* tab_helper = GetActiveTabHelper(web_state);
  if (!tab_helper || (!is_floaty_invoked_ && IsGeminiCopresenceEnabled())) {
    return;
  }

  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  PropagatePageContextToProvider(gemini_page_context);
}

void GeminiBrowserAgent::OnGeminiTabHelperDestroyed(
    GeminiTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
}

#pragma mark - FullscreenControllerObserver

void GeminiBrowserAgent::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  UpdateGeminiLiveIconVisibility();

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

bool GeminiBrowserAgent::IsOmniboxFocused() const {
  OmniboxPositionBrowserAgent* omnibox_agent =
      OmniboxPositionBrowserAgent::FromBrowser(browser_);
  return omnibox_agent && omnibox_agent->IsOmniboxFocused();
}

bool GeminiBrowserAgent::ShouldIgnoreKeyboardUpdate() const {
  bool is_expanded_not_thinking =
      last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
      ios::provider::GetCurrentClientMode() !=
          ios::provider::GeminiClientMode::kThinking;
  return !IsOmniboxFocused() &&
         (is_expanded_not_thinking || is_floaty_temporarily_hidden_);
}

bool GeminiBrowserAgent::IsChatEligiblePage() const {
  return IsGeminiChatAvailableForActiveWebState();
}

void GeminiBrowserAgent::UpdateLiveModeUI() {
  if (!IsInGeminiLiveMode()) {
    return;
  }
  ios::provider::SetLiveStopButtonHidden(!IsChatEligiblePage());
}

bool GeminiBrowserAgent::UpdateLiveModeUIAndMaybeContext() {
  UpdateLiveModeUI();
  if (IsChatEligiblePage()) {
    UpdateFloatyWithPartialPageContext();
    RequestPageContextGeneration();
    return true;
  }
  return false;
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
  if (!is_floaty_invoked_ || is_floaty_temporarily_hidden_) {
    return;
  }

  if (last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
      is_keyboard_visible_) {
    return;
  }

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::DidUpdateObscuredInsetRange(
    FullscreenBrowserAgent* agent) {
  if (!is_floaty_invoked_ || is_floaty_temporarily_hidden_) {
    return;
  }

  if (last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
      is_keyboard_visible_) {
    return;
  }

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::WillShutDown(FullscreenBrowserAgent* agent) {
  fullscreen_observation_.Reset();
}

#pragma mark - Private

void GeminiBrowserAgent::RequestPageContextGeneration() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);

  if (tab_helper) {
    tab_helper->GeneratePageContext(
        base::BindRepeating(&GeminiBrowserAgent::OnPageContextGenerated,
                            weak_factory_.GetWeakPtr()));
  }

  // Show page attachment UI chip every time the floaty is expanded.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

void GeminiBrowserAgent::PropagatePageContextToProvider(
    GeminiPageContext* gemini_page_context) {
  if (!is_floaty_invoked_) {
    return;
  }
  ApplyUserPrefsToPageContext(gemini_page_context);
  ios::provider::UpdatePageContext(gemini_page_context);
}

void GeminiBrowserAgent::UpdateFloatyWithPartialPageContext() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (tab_helper) {
    GeminiPageContext* gemini_page_context =
        tab_helper->GetPartialPageContext();
    PropagatePageContextToProvider(gemini_page_context);
  }
}

GeminiConfiguration* GeminiBrowserAgent::CreateGeminiConfiguration(
    UIViewController* base_view_controller,
    GeminiStartupState* startup_state,
    web::WebState* web_state,
    GeminiPageContext* page_context) {
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper) {
    return nil;
  }

  GeminiConfiguration* config = [[GeminiConfiguration alloc] init];
  config.baseViewController = base_view_controller;
  config.authService =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  config.gateway = bwg_gateway_;
  config.imageAttachment = startup_state.imageAttachment;

  config.clientID = base::SysUTF8ToNSString(gemini_tab_helper->GetClientId());
  std::optional<std::string> maybe_server_id = gemini_tab_helper->GetServerId();
  config.serverID =
      maybe_server_id ? base::SysUTF8ToNSString(*maybe_server_id) : nil;
  config.shouldAnimatePresentation = YES;
  config.lastInteractionURLDifferent =
      gemini_tab_helper->IsLastInteractionUrlDifferent();
  config.shouldShowSuggestionChips =
      gemini_tab_helper->ShouldShowSuggestionChips();
  config.contextualCueChipLabel = startup_state.prepopulatedPrompt;
  config.entryPoint = startup_state.entryPoint;
  config.imageRemixIPHShouldShow =
      startup_state.entryPoint == gemini::EntryPoint::ImageRemixIPH;

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile());
  if (tracker) {
    config.shouldShowGeminiLiveIPH = tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSGeminiLiveIPHFeature);
    config.shouldShowGeminiLiveNewBadge = tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature);
    has_triggered_gemini_live_iph_ = config.shouldShowGeminiLiveIPH;
    has_triggered_gemini_live_new_badge_ = config.shouldShowGeminiLiveNewBadge;
  } else {
    config.shouldShowGeminiLiveIPH = NO;
    config.shouldShowGeminiLiveNewBadge = NO;
  }
  config.geminiLiveIPHText = l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_IPH);

  config.geminiLocationPermissionState =
      ios::provider::GeminiLocationPermissionState::kUnknown;
  config.pageContext = page_context;
  if (IsGeminiCopresenceEnabled()) {
    config.initialBottomOffset = GetFloatyOffset();
  }
  config.hostWindowScene = browser_->GetSceneState().scene;
  GeminiService* gemini_service =
      GeminiServiceFactory::GetForProfile(browser_->GetProfile());
  config.needsAccountCapabilityRestriction =
      gemini_service && gemini_service->HasGeminiInChromeCapability() &&
      !gemini_service->HasModelExecutionCapability();

  return config;
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

void GeminiBrowserAgent::ApplyUserPrefsToPageContext(
    GeminiPageContext* gemini_page_context) {
  if (!IsChatEligiblePage()) {
    gemini_page_context.geminiPageContextComputationState =
        ios::provider::GeminiPageContextComputationState::kBlocked;
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kDetached;
    gemini_page_context.uniquePageContext = nullptr;
    return;
  }

  // Disable the page context attachment state based on user prefs.
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kUserDisabled;
  } else if (IsGeminiCopresenceEnabled() && is_floaty_invoked_ &&
             ios::provider::GetCurrentPageContextAttachmentState() ==
                 ios::provider::GeminiPageContextAttachmentState::kDetached &&
             !IsInGeminiLiveMode()) {
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

void GeminiBrowserAgent::SetSessionCommandHandlers() {
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
  id<BWGCommands> gemini_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);

  bwg_session_handler_.settingsHandler = settings_handler;
  bwg_session_handler_.geminiHandler = gemini_handler;
}

void GeminiBrowserAgent::OnPageContentPrefChanged() {
  if (IsGeminiLiveEnabled() &&
      ios::provider::GetCurrentMode() == ios::provider::GeminiViewMode::kLive &&
      processing_status_ == ios::provider::GeminiClientMode::kTranscribing) {
    return;
  }

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper) {
    return;
  }

  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  PropagatePageContextToProvider(gemini_page_context);

  // Trigger UI update for the attachment chip.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

void GeminiBrowserAgent::OnPageContextGenerated(
    GeminiPageContext* gemini_page_context) {
  PropagatePageContextToProvider(gemini_page_context);
}

void GeminiBrowserAgent::OnViewStateChanged(
    ios::provider::GeminiViewState view_state) {
  UpdateLiveModeUI();

  if (view_state == ios::provider::GeminiViewState::kExpanded) {
    if (is_floaty_temporarily_hidden_) {
      ForceShowFloatyIfInvoked();
      active_hiding_sources_.clear();
      is_hidden_by_keyboard_ = false;
    }
    RequestPageContextGeneration();
  } else if (view_state == ios::provider::GeminiViewState::kHidden) {
    // TODO(crbug.com/517583120): Remove when the temporary actuation prototype
    // is cleaned up.
    if (IsGeminiActorEnabled() && IsActorEnabled()) {
      if (actor::ActorService* actor_service =
              actor::ActorServiceFactory::GetForProfile(
                  browser_->GetProfile())) {
        actor_service->StopAllTasks();
      }
    }
  }
}

GeminiTabHelper* GeminiBrowserAgent::GetActiveTabHelper(
    web::WebState* web_state) const {
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

void GeminiBrowserAgent::RecordInvocationPageType() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  IOSGeminiInvocationPageType page_type =
      IOSGeminiInvocationPageType::kNoWebState;
  GeminiTabHelper* tab_helper = GetActiveTabHelper(web_state);
  if (tab_helper) {
    page_type = tab_helper->GetCurrentPageType();
  }
  RecordGeminiInvocationPageType(page_type);
}
