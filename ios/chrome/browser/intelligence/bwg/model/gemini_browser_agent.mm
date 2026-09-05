// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"

#import <AVFoundation/AVFoundation.h>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/containers/map_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/not_fatal_until.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
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
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_consent_provider_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
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
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/omnibox/model/omnibox_focus/omnibox_focus_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state_observer_bridge.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/custom_leading_view_type.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

// This is the inate padding of the Floaty default implementation. Remove it so
// we can have our own padding defined.
const CGFloat kFloatyIntrinsicPaddingCorrection = 34.0;

// Bottom margin for floaty.
const CGFloat kLegacyFloatingBottomMargin = 26;
const CGFloat kFloatingBottomMargin = 10;

// The vertical offset clearance required to position the dormant Live session
// snackbar cleanly above the floaty pill. Note this includes the full floaty
// height.
// TODO(crbug.com/512576285): Confirm offset value with UI.
// TODO(crbug.com/513881624): Get the actual floaty height separately, if
// possible, so these constants can just represent the offset.
const CGFloat kDormantSnackbarOffsetFromFloatyLegacy = 135.0;
const CGFloat kDormantSnackbarOffsetFromFloatyNext = 117.0;

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

// Used to check if floaty visibility updates are part of a UIView dismissal or
// presentation.
const double kViewTransitionTime = 0.8;

// Block accepted by -startGeminiFirstRunWithCompletion:
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

// Helper function to show the Settings redirection alert when microphone access
// is denied.
// TODO(crbug.com/521132540): Migrate this to the GeminiContainerViewController
// once the bottom sheet migration is complete.
void ShowMicrophoneSettingsAlert(UIViewController* base_view_controller,
                                 void (^completion)(BOOL granted)) {
  RecordLiveSettingsRedirectShown();
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_GEMINI_LIVE_MICROPHONE_ALERT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_GEMINI_LIVE_MICROPHONE_ALERT_DETAIL)
                preferredStyle:UIAlertControllerStyleAlert];

  [alert
      addAction:
          [UIAlertAction
              actionWithTitle:
                  l10n_util::GetNSString(
                      IDS_IOS_GEMINI_LIVE_MICROPHONE_ALERT_GO_TO_SETTINGS)
                        style:UIAlertActionStyleDefault
                      handler:^(UIAlertAction* action) {
                        RecordLiveSettingsRedirectOpenSettings();
                        NSURL* settingsURL = [NSURL
                            URLWithString:UIApplicationOpenSettingsURLString];
                        [[UIApplication sharedApplication] openURL:settingsURL
                                                           options:@{}
                                                 completionHandler:nil];
                        if (completion) {
                          completion(NO);
                        }
                      }]];

  [alert addAction:[UIAlertAction
                       actionWithTitle:
                           l10n_util::GetNSString(
                               IDS_IOS_GEMINI_LIVE_MICROPHONE_ALERT_NO_THANKS)
                                 style:UIAlertActionStyleCancel
                               handler:^(UIAlertAction* action) {
                                 RecordLiveSettingsRedirectCancel();
                                 if (completion) {
                                   completion(NO);
                                 }
                               }]];
  [base_view_controller presentViewController:alert
                                     animated:YES
                                   completion:nil];
}

// Helper function to show the in-app Gemini microphone permission alert.
void ShowGeminiMicrophonePermissionAlert(UIViewController* base_view_controller,
                                         base::WeakPtr<ProfileIOS> weak_profile,
                                         void (^completion)(BOOL granted)) {
  RecordLiveChromeMicPromptShown();
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_PERMISSION_MICROPHONE_PROMPT_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_GEMINI_PERMISSION_MICROPHONE_PROMPT_BODY)
                preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* acceptAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                RecordLiveChromeMicPromptAllowed();
                if (weak_profile) {
                  weak_profile->GetPrefs()->SetBoolean(
                      prefs::kIOSGeminiLiveMicrophoneSetting, true);
                }
                if (completion) {
                  completion(YES);
                }
              }];

  UIAlertAction* denyAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                RecordLiveChromeMicPromptDenied();
                if (completion) {
                  completion(NO);
                }
              }];

  [alert addAction:acceptAction];
  [alert addAction:denyAction];

  [base_view_controller presentViewController:alert
                                     animated:YES
                                   completion:nil];
}

// Returns true if the page context is eligible to be listed in the tab picker.
// A context is eligible if its computation state is either success or pending
// (meaning it is not blocked or protected).
bool IsPageContextEligibleForTabPicker(GeminiPageContext* context) {
  auto computation_state = context.geminiPageContextComputationState;
  return computation_state ==
             ios::provider::GeminiPageContextComputationState::kSuccess ||
         computation_state ==
             ios::provider::GeminiPageContextComputationState::kPending;
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
  StartObserving(browser_);

  pref_change_registrar_.Init(browser_->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIOSBWGPageContentSetting,
      base::BindRepeating(&GeminiBrowserAgent::OnPageContentPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kIOSGeminiLiveMicrophoneSetting,
      base::BindRepeating(&GeminiBrowserAgent::OnMicrophonePrefChanged,
                          base::Unretained(this)));

  // Sets up observation of fullscreen state.
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
    if (IsChromeNextIaEnabled()) {
      tab_grid_state_observer_bridge_ =
          [[TabGridStateObserverBridge alloc] initWithObserver:this];
      [scene_state.tabGridState addObserver:tab_grid_state_observer_bridge_];
    }
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
  last_known_gemini_availability_ = IsGeminiAvailableForActiveWebState();

  if (IsAppSwitcherAISummarizationEnabled()) {
    GeminiCapabilitiesManager* capabilities_manager =
        GeminiCapabilitiesManagerFactory::GetForProfile(browser_->GetProfile());
    if (capabilities_manager) {
      capabilities_manager->UpdateCapabilities();
    }
  }

  link_opening_handler_ = [[GeminiLinkOpeningHandler alloc]
      initWithURLLoader:UrlLoadingBrowserAgent::FromBrowser(browser_)
             dispatcher:browser_->GetCommandDispatcher()];
  ConfigureGemini();

  if (IsIOSGeminiBottomSheetMigrationEnabled()) {
    return;
  }

  gemini_container_mediator_ =
      [[GeminiContainerMediator alloc] initWithBrowser:browser_
                                          eventHandler:this];

  // TODO(crbug.com/537761575): Move tab managment related work to into a
  // dedicated helper/service class GeminiTabSessionManager.
  if (gemini_container_mediator_.gateway) {
    base::WeakPtr<GeminiBrowserAgent> weak_this = weak_factory_.GetWeakPtr();
    GeminiSessionHandler* session_handler =
        gemini_container_mediator_.gatewayManager.sessionHandler;
    if (session_handler) {
      session_handler.tabDetachRequestCallback = ^(NSString* tabID) {
        if (weak_this) {
          weak_this->DetachTabWithID(tabID);
        }
      };
      session_handler.tabAttachedCallback = ^(NSString* tab_id) {
        if (weak_this) {
          weak_this->UpdateLocalTabAttachmentState(
              tab_id,
              ios::provider::GeminiPageContextAttachmentState::kAttached);
        }
      };
      session_handler.tabDetachedCallback = ^(NSString* tab_id) {
        if (weak_this) {
          weak_this->UpdateLocalTabAttachmentState(
              tab_id,
              ios::provider::GeminiPageContextAttachmentState::kDetached);
        }
      };
      session_handler.attachedTabsCountProvider = ^{
        if (weak_this) {
          return weak_this->AttachedTabsCount();
        }
        return (NSUInteger)0;
      };
      session_handler.isMultiTabUsedProvider = ^{
        if (weak_this) {
          return weak_this->GetSharedTabs().count > 0;
        }
        return NO;
      };
    }

    GeminiTabPickerHandler* tab_picker_handler =
        gemini_container_mediator_.gatewayManager.tabPickerHandler;
    if (tab_picker_handler) {
      tab_picker_handler.selectionCallback =
          ^(std::set<web::WebStateID> selected_tabs) {
            if (weak_this) {
              weak_this->OnTabPickerSelectionChanged(selected_tabs);
            }
          };
      tab_picker_handler.selectedTabsProvider = ^{
        if (weak_this) {
          std::set<web::WebStateID> eligible_tabs;
          for (const auto& [tab_id, context] : weak_this->attached_tabs_) {
            if (IsPageContextEligibleForTabPicker(context) &&
                context.geminiPageContextAttachmentState ==
                    ios::provider::GeminiPageContextAttachmentState::
                        kAttached) {
              eligible_tabs.insert(tab_id);
            }
          }
          return eligible_tabs;
        }
        return std::set<web::WebStateID>();
      };
    }
  }
}

GeminiBrowserAgent::~GeminiBrowserAgent() {
  LogLiveSessionMetrics(/*floaty_dismissed=*/true);
  [link_opening_handler_ disconnect];
  link_opening_handler_ = nil;

  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }

  if (browser_) {
    browser_->RemoveObserver(this);
  }

  if (!IsIOSGeminiBottomSheetMigrationEnabled()) {
    [gemini_container_mediator_ disconnect];
    gemini_container_mediator_ = nil;
  }

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

  if (tab_grid_state_observer_bridge_ && browser_) {
    SceneState* scene_state = browser_->GetSceneState();
    [scene_state.tabGridState removeObserver:tab_grid_state_observer_bridge_];
  }
  tab_grid_state_observer_bridge_ = nil;

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

  StopObserving();
}

void GeminiBrowserAgent::BrowserDestroyed(Browser* browser) {
  [link_opening_handler_ disconnect];
  link_opening_handler_ = nil;

  if (!IsIOSGeminiBottomSheetMigrationEnabled()) {
    [gemini_container_mediator_ disconnect];
    gemini_container_mediator_ = nil;
  }

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

bool GeminiBrowserAgent::IsFloatyVisible() const {
  if (!is_floaty_invoked_) {
    return false;
  }

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (!active_web_state) {
    return false;
  }

  bool is_web_state_visible = active_web_state->IsVisible();
  if (!is_web_state_visible && !IsChromeNextIaEnabled()) {
    return false;
  }

  ProfileIOS* profile = browser_->GetProfile();
  return gemini::IsGeminiAvailable(gemini::EntryPoint::Unknown, profile,
                                   active_web_state)
      .enabled;
}

bool GeminiBrowserAgent::IsInGeminiLiveMode() const {
  return is_floaty_invoked_ &&
         gemini::IsFeatureAvailable(gemini::Feature::kLive,
                                    browser_->GetProfile()) &&
         ios::provider::GetCurrentMode() ==
             ios::provider::GeminiViewMode::kLive;
}

gemini::EntryPoint GeminiBrowserAgent::GetEntryPoint() const {
  return entry_point_;
}

void GeminiBrowserAgent::ConfigureGemini() {
  ProfileIOS* profile = browser_->GetProfile();
  if (!profile) {
    return;
  }
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!auth_service || !auth_service->HasPrimaryIdentity()) {
    return;
  }

  GeminiStartupConfiguration* config =
      [[GeminiStartupConfiguration alloc] init];
  config.authService = auth_service;
  config.linkOpeningHandler = link_opening_handler_;
  config.imageRemixEnabled =
      gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, profile);
  config.geminiLiveEnabled =
      gemini::IsFeatureAvailable(gemini::Feature::kLive, profile);

  ios::provider::ConfigureWithStartupConfiguration(config);
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

  if (event_type != signin::PrimaryAccountChangeEvent::Type::kNone) {
    browser_->GetProfile()->GetPrefs()->ClearPref(prefs::kGeminiConversationId);

    ForceDismissFloaty();
  }
}

void GeminiBrowserAgent::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
    ForceDismissFloaty();
  }
}

// Called when account capabilities are updated in the background. This ensures
// we re-configure Gemini once the user's capabilities (like age eligibility)
// finish syncing from the server after sign-in.
void GeminiBrowserAgent::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id == account_info.GetAccountId()) {
    ConfigureGemini();
    UpdateGeminiAvailability();
    UpdateGeminiLiveIconVisibility();
  }
}

void GeminiBrowserAgent::OnKeyboardStateChanged(bool is_visible) {
  if (is_visible == is_keyboard_visible_) {
    return;
  }

  is_keyboard_visible_ = is_visible;
  if (gemini::IsFeatureAvailable(gemini::Feature::kLive,
                                 browser_->GetProfile())) {
    ios::provider::SetLiveCaptionsNumberOfLines(is_visible ? 1 : -1);
  }

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

  if (is_hidden_by_keyboard_) {
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
      SwitchToChatModeOrDismiss(/*animated=*/false);
    }
  }
  UpdateGeminiLiveIconVisibility(/*animated=*/false);
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

void GeminiBrowserAgent::ShowLiveSessionDormantSnackbar(int message_id) {
  id<SnackbarCommands> snackbar_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SnackbarCommands);

  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(message_id)];

  base::WeakPtr<GeminiBrowserAgent> weak_self = weak_factory_.GetWeakPtr();
  message.completionHandler = ^(BOOL completed) {
    if (weak_self) {
      weak_self->SetIsShowingLiveSessionDormantSnackbar(false);
    }
  };

  CGFloat floaty_offset = GetFullyExpandedFloatyOffset();
  CGFloat offset_from_floaty = IsChromeNextIaEnabled()
                                   ? kDormantSnackbarOffsetFromFloatyNext
                                   : kDormantSnackbarOffsetFromFloatyLegacy;
  CGFloat snackbar_offset = floaty_offset + offset_from_floaty;

  if (is_floaty_invoked_) {
    PrepareFloatyToBeShown();
  }

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
  entry_point_ = entry_point;
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
  startup_state.isFirstSession = will_show_first_run && !skip_consent;

  if (!startup_state.isFirstSession) {
    PresentFloaty(base_view_controller, startup_state);
    return;
  }

  id<GeminiCommands> gemini_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), GeminiCommands);

  auto present_floaty_closure = base::BindRepeating(
      &GeminiBrowserAgent::PresentFloaty, weak_factory_.GetWeakPtr(),
      base_view_controller, startup_state);

  [gemini_handler
      startGeminiFirstRunWithCompletion:BlockRunningClosureIfSuccess(
                                            std::move(present_floaty_closure))
                         fromEntryPoint:entry_point];
}

void GeminiBrowserAgent::ShowGeminiLiveMicrophoneAlert(
    UIViewController* base_view_controller,
    void (^completion)(BOOL granted)) {
  AVAuthorizationStatus status =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
  switch (status) {
    case AVAuthorizationStatusNotDetermined: {
      RecordLiveOSMicPromptShown();
      [AVCaptureDevice
          requestAccessForMediaType:AVMediaTypeAudio
                  completionHandler:^(BOOL granted) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                      if (granted) {
                        RecordLiveOSMicPromptAllowed();
                        if (!browser_->GetProfile()->GetPrefs()->GetBoolean(
                                prefs::kIOSGeminiLiveMicrophoneSetting)) {
                          ShowGeminiMicrophonePermissionAlert(
                              base_view_controller,
                              browser_->GetProfile()->AsWeakPtr(), completion);
                        } else {
                          if (completion) {
                            completion(YES);
                          }
                        }
                      } else {
                        RecordLiveOSMicPromptDenied();
                        // If user reject mic permission on the
                        // native iOS alert, we call completion to
                        // reset state.
                        if (completion) {
                          completion(NO);
                        }
                      }
                    });
                  }];
      break;
    }
    case AVAuthorizationStatusAuthorized:
      if (!browser_->GetProfile()->GetPrefs()->GetBoolean(
              prefs::kIOSGeminiLiveMicrophoneSetting)) {
        ShowGeminiMicrophonePermissionAlert(base_view_controller,
                                            browser_->GetProfile()->AsWeakPtr(),
                                            completion);
        break;
      }
      if (completion) {
        completion(YES);
      }
      break;
    case AVAuthorizationStatusDenied:
    case AVAuthorizationStatusRestricted:
      ShowMicrophoneSettingsAlert(base_view_controller, completion);
      break;
  }
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

void GeminiBrowserAgent::UpdateGeminiLiveIconVisibility(bool animated) {
  if (!IsGeminiLiveEnabled()) {
    return;
  }

  CustomLeadingViewType type = IsInGeminiLiveMode()
                                   ? CustomLeadingViewType::kGeminiLive
                                   : CustomLeadingViewType::kNone;
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  if (IsChromeNextIaEnabled()) {
    id<LocationBarBadgeCommands> location_bar_badge_handler =
        HandlerForProtocol(dispatcher, LocationBarBadgeCommands);
    [location_bar_badge_handler setBadgeCustomLeadingViewType:type];
  } else {
    id<OmniboxCommands> omnibox_handler =
        HandlerForProtocol(dispatcher, OmniboxCommands);
    [omnibox_handler setCustomLeadingViewType:type];
  }
}

CGFloat GeminiBrowserAgent::GetFloatyOffset() {
  CHECK(IsFullscreenInitialized());

  CGFloat max_bottom_inset = 0;

  SceneState* scene_state = browser_->GetSceneState();
  if (IsChromeNextIaEnabled() && IsTabGridVisible()) {
    if (scene_state.layoutState.appBarPosition == AppBarPosition::kBottom) {
      max_bottom_inset = AppBarHeightPortrait();
    } else {
      max_bottom_inset = 0;
    }
  } else {
    max_bottom_inset =
        IsFullscreenRefactoringEnabled()
            ? FullscreenBrowserAgent::FromBrowser(browser_)->max_insets().bottom
            : fullscreen_controller_->GetMaxViewportInsets().bottom;
  }

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
      CGFloat portrait_height =
          (is_floaty_invoked_ && IsAppBarHiddenInFullscreen())
              ? kAppBarHeightFullscreen
              : AppBarHeightPortrait();
      max_bottom_inset += portrait_height;
    }
  }

  if (scene_state && scene_state.window && IsLandscape(scene_state.window)) {
    max_bottom_inset += scene_state.window.safeAreaInsets.bottom;
  }

  CGFloat bottomMargin;
  if (IsChromeNextIaEnabled()) {
    CGFloat safeAreaBottom = scene_state.window.safeAreaInsets.bottom;
    bottomMargin = safeAreaBottom - kFloatingBottomMargin;

  } else {
    bottomMargin =
        kFloatyIntrinsicPaddingCorrection - kLegacyFloatingBottomMargin;
  }

  CGFloat offset = (max_bottom_inset * GetFloatyProgress()) - bottomMargin;

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
      CGFloat portrait_height =
          (is_floaty_invoked_ && IsAppBarHiddenInFullscreen())
              ? kAppBarHeightFullscreen
              : AppBarHeightPortrait();
      max_bottom_inset += portrait_height;
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
  PrepareFloatyToBeShown();
  ios::provider::StartGeminiOverlay(config);
  last_shown_view_state_ = ios::provider::GetCurrentGeminiViewState();
  is_floaty_invoked_ = true;
  floaty_tab_switch_count_ = 0;
  if (IsChromeNextIaEnabled()) {
    ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                  GetFloatyProgress());
  }
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
                                       GeminiStartupState* startup_state) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state) {
    return;
  }

  UpdateAttachedTabsForActiveWebState(web_state);

  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper) {
    return;
  }

  // Fetch zero-state suggestions while the floaty is being presented.
  if (IsZeroStateSuggestionsEnabled()) {
    gemini_tab_helper->FetchZeroStateSuggestions(base::DoNothing());
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

  if (is_floaty_invoked_) {
    if (image_attachment) {
      ios::provider::AttachImage(image_attachment);
    }
    PropagatePageContextToProvider(initial_page_context);
    if (prepopulated_prompt) {
      ios::provider::UpdatePromptAction(entry_point, prepopulated_prompt);
    }
    CHECK(gemini_container_mediator_, base::NotFatalUntil::M155);
    bool should_show_suggestion_chips = [gemini_container_mediator_
        shouldShowSuggestionChipsForEntryPoint:entry_point];
    ios::provider::SetShouldShowSuggestionChips(should_show_suggestion_chips);
    bool block_query_submission = [gemini_container_mediator_
        shouldBlockQuerySubmissionWhileLoadingForEntryPoint:entry_point];
    ios::provider::SetBlockQuerySubmissionWhileLoading(block_query_submission);
    bool show_page_loading_snackbar = [gemini_container_mediator_
        shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
            entry_point];
    ios::provider::SetShowPageLoadingSnackbarOnOpeningInvocation(
        show_page_loading_snackbar);
    if (IsChromeNextIaEnabled() && IsFullscreenRefactoringEnabled()) {
      [HandlerForProtocol(browser_->GetCommandDispatcher(), FullscreenCommands)
          exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::
                                        kUserInitiatedFinishedByCode
                           animated:YES];
    }
    ForceShowFloatyIfInvoked();
    ios::provider::UpdateGeminiViewState(
        ios::provider::GeminiViewState::kExpanded, /*animated=*/true);
    if (IsAppSwitcherAISummarizationEnabled() &&
        startup_state.isMismatchedAccount) {
      ios::provider::ShowAccountSnackbar();
    }
  } else {
    SetSessionCommandHandlers();

    CHECK(gemini_container_mediator_, base::NotFatalUntil::M155);
    GeminiConfiguration* config = [gemini_container_mediator_
        createGeminiConfigurationForActiveWebState:startup_state
                                baseViewController:base_view_controller];
    config.initialBottomOffset = GetFloatyOffset();
    config.hostWindowScene = browser_->GetSceneState().scene;

    DismissGeminiFromOtherWindows(base::BindOnce(
        &GeminiBrowserAgent::InvokeFloaty, weak_factory_.GetWeakPtr(), config));
  }

  base::UmaHistogramLongTimes(startup_state.isFirstSession
                                  ? kStartupTimeWithFirstRunHistogram
                                  : kStartupTimeNoFirstRunHistogram,
                              base::TimeTicks::Now() - start_time);

  // Request full page context generation, which will update the floaty once
  // it's available.
  gemini_tab_helper->GeneratePageContext(base::BindRepeating(
      &GeminiBrowserAgent::OnPageContextGenerated, weak_factory_.GetWeakPtr()));
}

void GeminiBrowserAgent::HandleDormantStatus(
    ios::provider::GeminiDormantReason dormant_reason) {
  if (IsGeminiLiveDormantReasonsEnabled()) {
    switch (dormant_reason) {
      case ios::provider::GeminiDormantReason::kLowVolumeInBackground:
      case ios::provider::GeminiDormantReason::kLowVolumeInForeground:
      case ios::provider::GeminiDormantReason::kInterruptedByExternalAudio:
      case ios::provider::GeminiDormantReason::kUserStop:
      case ios::provider::GeminiDormantReason::kUserPause:
        SwitchToChatModeOrDismiss(/*animated=*/true,
                                  ios::provider::GeminiViewState::kExpanded);
        break;
      case ios::provider::GeminiDormantReason::kInactivityTimeout:
        SwitchToChatModeOrDismiss(/*animated=*/true,
                                  ios::provider::GeminiViewState::kCollapsed);
        is_showing_live_session_dormant_snackbar_ = true;
        ShowLiveSessionDormantSnackbar(
            IDS_IOS_GEMINI_LIVE_CONTINUE_SESSION_SNACKBAR);
        break;
      case ios::provider::GeminiDormantReason::kLongInteractionTimeout:
      case ios::provider::GeminiDormantReason::kServerPause:
        SwitchToChatModeOrDismiss(/*animated=*/true,
                                  ios::provider::GeminiViewState::kCollapsed);
        is_showing_live_session_dormant_snackbar_ = true;
        ShowLiveSessionDormantSnackbar(
            IDS_IOS_GEMINI_LIVE_SERVER_PAUSE_SNACKBAR);
        break;
      default:
        SwitchToChatModeOrDismiss(/*animated=*/true,
                                  ios::provider::GeminiViewState::kCollapsed);
        is_showing_live_session_dormant_snackbar_ = true;
        ShowLiveSessionDormantSnackbar(
            IDS_IOS_GEMINI_LIVE_GENERAL_DORMANT_SNACKBAR);
        break;
    }
  } else {
    SwitchToChatModeOrDismiss(/*animated=*/true,
                              ios::provider::GeminiViewState::kCollapsed);
    is_showing_live_session_dormant_snackbar_ = true;
    ShowLiveSessionDormantSnackbar(
        IDS_IOS_GEMINI_LIVE_GENERAL_DORMANT_SNACKBAR);
  }
}

void GeminiBrowserAgent::LogLiveStatusTransition(
    ios::provider::GeminiClientMode old_status,
    ios::provider::GeminiClientMode new_status) {
  if (old_status == ios::provider::GeminiClientMode::kResponding &&
      new_status != ios::provider::GeminiClientMode::kResponding) {
    if (!live_response_start_time_.is_null()) {
      base::TimeDelta duration =
          base::TimeTicks::Now() - live_response_start_time_;
      RecordGeminiLiveResponseDuration(duration);
      live_response_start_time_ = base::TimeTicks();
    }
  }

  if (old_status == ios::provider::GeminiClientMode::kThinking &&
      new_status != ios::provider::GeminiClientMode::kResponding) {
    live_thinking_start_time_ = base::TimeTicks();
  }
}

void GeminiBrowserAgent::LogLiveSessionMetrics(bool floaty_dismissed) {
  if (!live_session_start_time_.is_null() &&
      (floaty_dismissed || !IsInGeminiLiveMode())) {
    live_session_accumulated_duration_ +=
        base::TimeTicks::Now() - live_session_start_time_;
    live_session_start_time_ = base::TimeTicks();

    RecordGeminiLiveTurnCount(live_turn_count_);
    live_turn_count_ = 0;
    live_response_start_time_ = base::TimeTicks();
    live_thinking_start_time_ = base::TimeTicks();
  }

  if (floaty_dismissed) {
    if (!live_session_accumulated_duration_.is_zero()) {
      RecordGeminiLiveAccumulatedDuration(live_session_accumulated_duration_);
      live_session_accumulated_duration_ = base::TimeDelta();
    }
  }
}

#pragma mark - GeminiContainerMediatorEventHandler

void GeminiBrowserAgent::OnViewStateChanged(
    ios::provider::GeminiViewState view_state) {
  UpdateLiveModeUI();

  if (view_state == ios::provider::GeminiViewState::kExpanded) {
    if (last_shown_view_state_ != ios::provider::GeminiViewState::kExpanded) {
      PrepareFloatyToBeShown();
    }
    if (is_floaty_temporarily_hidden_) {
      ForceShowFloatyIfInvoked();
      is_hidden_by_keyboard_ = false;
    }
    RequestPageContextGeneration();
  } else if (view_state == ios::provider::GeminiViewState::kCollapsed) {
    ResetFullscreenDisabler();
  } else if (view_state == ios::provider::GeminiViewState::kHidden) {
    ResetFullscreenDisabler();

    // TODO(crbug.com/517583120): Remove when the temporary actuation prototype
    // is cleaned up.
    if (IsGeminiActorEnabled()) {
      if (actor::ActorService* actor_service =
              actor::ActorServiceFactory::GetForProfile(
                  browser_->GetProfile())) {
        actor_service->StopAllTasks();
      }
    }
  }
}

void GeminiBrowserAgent::OnProcessingStatusChanged(
    ios::provider::GeminiClientMode processing_status,
    ios::provider::GeminiDormantReason dormant_reason) {
  UpdateGeminiLiveIconVisibility();
  if (!IsInGeminiLiveMode()) {
    return;
  }

  LogLiveStatusTransition(processing_status_, processing_status);

  processing_status_ = processing_status;
  switch (processing_status) {
    case ios::provider::GeminiClientMode::kTranscribing:
      RequestPageContextGeneration();
      break;
    case ios::provider::GeminiClientMode::kThinking:
      live_thinking_start_time_ = base::TimeTicks::Now();
      break;
    case ios::provider::GeminiClientMode::kResponding: {
      live_turn_count_++;
      live_response_start_time_ = base::TimeTicks::Now();
      if (!live_thinking_start_time_.is_null()) {
        base::TimeDelta latency =
            live_response_start_time_ - live_thinking_start_time_;
        RecordGeminiLiveResponseLatency(latency);
        live_thinking_start_time_ = base::TimeTicks();
      }
      // Update partial page context (i.e., live sharing context label) when
      // transitioning out of the transcribing (i.e., speaking) state.
      UpdateFloatyWithPartialPageContext();
      break;
    }
    case ios::provider::GeminiClientMode::kDormant:
      RecordGeminiLiveDormantReason(dormant_reason);
      HandleDormantStatus(dormant_reason);
      LogLiveSessionMetrics();
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
    RecordFloatyCollapsedToExpanded();
    RecordFloatyMinimizedTime(elapsed_minimized_floaty_time_);
    elapsed_minimized_floaty_time_ = base::TimeTicks();
  } else if (view_state == ios::provider::GeminiViewState::kCollapsed) {
    RecordFloatyExpandedToCollapsed();
    elapsed_minimized_floaty_time_ = base::TimeTicks::Now();
  }
  last_shown_view_state_ = view_state;
}

void GeminiBrowserAgent::OnLiveButtonTapped() {
  RecordLiveButtonTapped();
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile());
  if (tracker) {
    tracker->NotifyEvent(feature_engagement::events::kIOSGeminiLiveUsed);
  }
}

void GeminiBrowserAgent::OnGeminiLiveUserDidPressStopButton() {
  // TODO(crbug.com/513271981): Record metrics when the user presses the Live
  // stop button.
}

void GeminiBrowserAgent::OnGeminiLiveUserDidBargeIn() {
  processing_status_ = ios::provider::GeminiClientMode::kTranscribing;
  RequestPageContextGeneration();
}

void GeminiBrowserAgent::OnModeChanged(ios::provider::GeminiViewMode mode) {
  if (IsFullscreenInitialized()) {
    if (IsFullscreenRefactoringEnabled()) {
      [HandlerForProtocol(browser_->GetCommandDispatcher(), FullscreenCommands)
          exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::
                                        kUserInitiatedFinishedByCode
                           animated:YES];
    } else {
      fullscreen_controller_->ExitFullscreen();
    }
  }

  if (mode == ios::provider::GeminiViewMode::kLive) {
    RecordLiveSessionStarted();
    if (live_session_start_time_.is_null()) {
      live_session_start_time_ = base::TimeTicks::Now();
      live_turn_count_ = 0;
    }
    if (last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded) {
      ResetFullscreenDisabler();
    }
  } else {
    LogLiveSessionMetrics();
  }
  UpdateGeminiLiveIconVisibility();
}

void GeminiBrowserAgent::OnGeminiUIDidAppear() {
  ResetFullscreenDisabler();
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
    id<GeminiCommands> gemini_handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), GeminiCommands);
    [gemini_handler
        dismissGeminiFlowWithCompletion:base::CallbackToBlock(barrier)];
  }
}

void GeminiBrowserAgent::DismissFloaty() {
  // If the floaty is temporarily hidden i.e. as part of a view controller being
  // shown underneath the Gemini floaty, don't clean up and reset internal
  // Gemini properties. Clean up should occur if a user taps the floaty to
  // dismiss it or the browser agent destructing.
  if (is_floaty_temporarily_hidden_) {
    return;
  }

  LogLiveSessionMetrics(/*force=*/true);

  CHECK(gemini_container_mediator_, base::NotFatalUntil::M155);
  [gemini_container_mediator_ onFloatyDismiss];

  // TODO(crbug.com/517583120): Remove when the temporary actuation prototype is
  // cleaned up.
  if (IsGeminiActorEnabled()) {
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

  // Record and reset tab switch metrics for the ending Floaty session.
  if (is_floaty_invoked_) {
    RecordSessionTabSwitchCount(floaty_tab_switch_count_);
    floaty_tab_switch_count_ = 0;
  }

  is_floaty_invoked_ = false;
  for (auto& observer : observers_) {
    observer.OnFloatyInvokedChanged(is_floaty_invoked_);
  }
  is_hidden_by_keyboard_ = false;
  processing_status_ = ios::provider::GeminiClientMode::kUnknown;
  elapsed_minimized_floaty_time_ = base::TimeTicks();
  entry_point_ = gemini::EntryPoint::Unknown;
  UpdateGeminiLiveIconVisibility();
  ios::provider::ResetGemini();
  ResetFullscreenDisabler();
}

void GeminiBrowserAgent::ForceDismissFloaty() {
  if (!is_floaty_invoked_) {
    return;
  }
  is_floaty_temporarily_hidden_ = false;
  DismissFloaty();
}

NSUInteger GeminiBrowserAgent::AttachedTabsCount() const {
  NSUInteger count = 0;
  for (const auto& [tab_id, context] : attached_tabs_) {
    if (context.geminiPageContextAttachmentState ==
        ios::provider::GeminiPageContextAttachmentState::kAttached) {
      count++;
    }
  }
  return count;
}

void GeminiBrowserAgent::OnTabPickerSelectionChanged(
    std::set<web::WebStateID> selected_tabs) {
  web::WebStateID active_web_state_id = GetActiveWebStateID();

  // Create `new_attached_tabs` which will replace `attached_tabs_`.
  AttachedTabsList new_attached_tabs;
  std::vector<web::WebStateID> tabs_to_fetch;

  // Add existing attached tabs to `new_attached_tabs` in insertion order,
  // ensuring their attachment state reflects whether they were selected.
  for (const auto& [tab_id, context] : attached_tabs_) {
    if (tab_id == active_web_state_id) {
      if (IsPageContextEligibleForTabPicker(context)) {
        // Update the active tab's selection state if it was shown in the Tab
        // Picker.
        ios::provider::GeminiPageContextAttachmentState new_state =
            selected_tabs.contains(active_web_state_id)
                ? ios::provider::GeminiPageContextAttachmentState::kAttached
                : ios::provider::GeminiPageContextAttachmentState::kDetached;
        context.geminiPageContextAttachmentState = new_state;
      }
      new_attached_tabs.emplace_back(tab_id, context);
    } else if (selected_tabs.contains(tab_id)) {
      // Keep existing page context for a tab that is still selected.
      new_attached_tabs.emplace_back(tab_id, context);
    }
  }

  // Identify newly selected tabs and queue them for page context fetch.
  for (web::WebStateID selected_tab : selected_tabs) {
    if (selected_tab == active_web_state_id) {
      // We already processed the active tab above, so we can ignore it here.
      continue;
    }
    if (!GetAttachedPageContext(selected_tab)) {
      // Tab is newly selected and we will need to fetch its page context.
      tabs_to_fetch.push_back(selected_tab);
    }
  }

  attached_tabs_ = std::move(new_attached_tabs);

  UpdateAttachedTabContexts(tabs_to_fetch);

  GeminiPageContext* active_page_context =
      GetAttachedPageContext(active_web_state_id);
  CHECK(active_page_context);

  ios::provider::UpdateActivePageContext(active_page_context, GetSharedTabs());
}

void GeminiBrowserAgent::UpdateAttachedTabContexts(
    const std::vector<web::WebStateID>& tabs_to_fetch) {
  if (tabs_to_fetch.empty()) {
    return;
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  std::vector<std::string> tabs_to_fetch_str;

  // Generate partial page contexts for each tab.
  for (web::WebStateID tab_to_fetch : tabs_to_fetch) {
    web::WebState* web_state = GetWebState(
        web_state_list, WebStateSearchCriteria{.identifier = tab_to_fetch});
    if (!web_state) {
      continue;
    }

    GeminiPageContext* partial_context = CreatePartialPageContext(web_state);
    CHECK(partial_context);

    partial_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kAttached;
    SetAttachedPageContext(tab_to_fetch, partial_context);

    tabs_to_fetch_str.push_back(
        base::NumberToString(tab_to_fetch.identifier()));
  }

  // Trigger full page context fetching, which will call
  // `OnPersistTabContextLookupComplete` for each tab when completed.
  PersistTabContextBrowserAgent* persist_agent =
      PersistTabContextBrowserAgent::FromBrowser(browser_);
  CHECK(persist_agent);

  persist_agent->GetMultipleContextsAsync(
      tabs_to_fetch_str,
      base::BindOnce(&GeminiBrowserAgent::OnPersistTabContextLookupComplete,
                     weak_factory_.GetWeakPtr()));
}

void GeminiBrowserAgent::SwitchToChatModeOrDismiss(
    bool animated,
    ios::provider::GeminiViewState target_state) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (tab_helper && !tab_helper->IsGeminiChatAvailableForWebState()) {
    DismissFloaty();
  } else {
    ios::provider::SwitchToMode(ios::provider::GeminiViewMode::kFloaty,
                                target_state, animated);
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
  bool is_context_menu = source == gemini::FloatyUpdateSource::ContextMenu;

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  bool should_block =
      gemini_tab_helper && gemini_tab_helper->ShouldBlockFloatyFromShowing();
  if ((!is_web_navigation && !is_context_menu && triggered_during_transition) ||
      should_block) {
    return;
  }

  RecordGeminiViewStateHiddenToShown(last_shown_view_state_);
  RecordFloatyShownFromSource(source);
  is_floaty_temporarily_hidden_ = false;

  // Exit fullscreen to prepare floaty for incoming response stream.
  if (source == gemini::FloatyUpdateSource::ForcedFromQueryResponse) {
    PrepareFloatyToBeShown();
  }

  // Animate the floaty back into view and release the fullscreen disabler
  // once the animation completes.
  base::WeakPtr<GeminiBrowserAgent> weak_self = weak_factory_.GetWeakPtr();
  [UIView animateWithDuration:kFloatyAnimationDuration
                   animations:base::CallbackToBlock(base::BindRepeating(
                                  &GeminiBrowserAgent::ForceShowFloatyIfInvoked,
                                  weak_self))
                   completion:^(BOOL finished) {
                     if (weak_self) {
                       weak_self->ResetFullscreenDisabler();
                     }
                   }];
}

#pragma mark - TabsDependencyInstaller

void GeminiBrowserAgent::OnWebStateInserted(web::WebState* web_state) {}

void GeminiBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  if (!IsGeminiMultiTabContextEnabled()) {
    return;
  }
  web::WebStateID removed_web_state_id = web_state->GetUniqueIdentifier();
  RemoveAttachedPageContext(removed_web_state_id);
}

void GeminiBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  if (!IsGeminiMultiTabContextEnabled()) {
    return;
  }
  web::WebStateID deleted_web_state_id = web_state->GetUniqueIdentifier();
  RemoveAttachedPageContext(deleted_web_state_id);
}

void GeminiBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                                 web::WebState* new_active) {
  // Track tab switches during an active Floaty session for session metrics and
  // user actions.
  if (is_floaty_invoked_ && old_active && new_active &&
      old_active != new_active) {
    floaty_tab_switch_count_++;
    base::RecordAction(
        base::UserMetricsAction("MobileGeminiFloatyTabSwitched"));
  }

  if (old_active) {
    GeminiTabHelper* old_tab_helper = GeminiTabHelper::FromWebState(old_active);
    if (old_tab_helper) {
      old_tab_helper->RemoveObserver(this);
    }
    [old_active->GetWebViewProxy().scrollViewProxy
        removeObserver:scroll_observer_];

    web::WebStateID old_active_id = old_active->GetUniqueIdentifier();
    if (GeminiPageContext* old_context =
            GetAttachedPageContext(old_active_id)) {
      if (old_context.geminiPageContextAttachmentState !=
          ios::provider::GeminiPageContextAttachmentState::kAttached) {
        RemoveAttachedPageContext(old_active_id);
      } else if (HasSharedTabs()) {
        // We are switching tabs and there is more than one tab attached to the
        // conversation. Refetch the old active tab's page context to ensure it
        // reflects its most recent state (instead of the state when the Floaty
        // was last invoked).
        UpdateAttachedTabContexts({old_active_id});
      }
    }
  }

  if (new_active) {
    if (is_floaty_invoked_) {
      UpdateAttachedTabsForActiveWebState(new_active);
    }
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
  ResetFullscreenDisabler();
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
    id<GeminiCommands> gemini_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), GeminiCommands);
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
  if (!tab_helper || !is_floaty_invoked_) {
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

bool GeminiBrowserAgent::IsOmniboxFocused() const {
  OmniboxFocusBrowserAgent* omnibox_agent =
      OmniboxFocusBrowserAgent::FromBrowser(browser_);
  return omnibox_agent && omnibox_agent->IsOmniboxFocused();
}

bool GeminiBrowserAgent::IsTabGridVisible() const {
  SceneState* scene_state = browser_->GetSceneState();
  return scene_state && scene_state.tabGridState.tabGridVisible;
}

bool GeminiBrowserAgent::ShouldIgnoreKeyboardUpdate() const {
  bool is_expanded_not_thinking =
      last_shown_view_state_ == ios::provider::GeminiViewState::kExpanded &&
      ios::provider::GetCurrentClientMode() !=
          ios::provider::GeminiClientMode::kThinking;
  return !IsOmniboxFocused() &&
         (is_expanded_not_thinking || is_floaty_temporarily_hidden_);
}

void GeminiBrowserAgent::UpdateLiveModeUI() {
  if (!IsInGeminiLiveMode()) {
    return;
  }
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  bool is_eligible =
      tab_helper && tab_helper->IsGeminiChatAvailableForWebState();
  ios::provider::SetLiveStopButtonHidden(!is_eligible);
}

bool GeminiBrowserAgent::UpdateLiveModeUIAndMaybeContext() {
  UpdateLiveModeUI();
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (tab_helper && tab_helper->IsGeminiChatAvailableForWebState()) {
    // If the user is speaking (i.e., transcribing), we block page context
    // updates, to maintain the full context of the page that the user was on
    // when they started wording their query.
    if (processing_status_ == ios::provider::GeminiClientMode::kTranscribing) {
      return true;
    }
    UpdateFloatyWithPartialPageContext();
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
      agent->keyboard_obscured_inset() > 0) {
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
      agent->keyboard_obscured_inset() > 0) {
    return;
  }

  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                GetFloatyProgress());
}

void GeminiBrowserAgent::WillShutDown(FullscreenBrowserAgent* agent) {
  fullscreen_observation_.Reset();
}

#pragma mark - TabGridStateObserver

void GeminiBrowserAgent::WillEnterTabGrid() {
  if (IsFullscreenInitialized()) {
    ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(), 1.0);
  }
}

void GeminiBrowserAgent::WillExitTabGrid() {
  if (IsFullscreenInitialized()) {
    ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(),
                                                  GetFloatyProgress());
  }
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

void GeminiBrowserAgent::UpdateAttachedTabsForActiveWebState(
    web::WebState* active_web_state) {
  if (!IsGeminiMultiTabContextEnabled()) {
    return;
  }

  if (!active_web_state) {
    attached_tabs_.clear();
    return;
  }

  web::WebStateID new_active_id = active_web_state->GetUniqueIdentifier();
  GeminiPageContext* active_context = GetAttachedPageContext(new_active_id);
  if (!active_context ||
      active_context.geminiPageContextAttachmentState !=
          ios::provider::GeminiPageContextAttachmentState::kAttached) {
    attached_tabs_.clear();
  }
}

void GeminiBrowserAgent::PropagatePageContextToProvider(
    GeminiPageContext* active_page_context) {
  if (!is_floaty_invoked_) {
    return;
  }

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  GeminiTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  bool is_eligible =
      tab_helper && tab_helper->IsGeminiChatAvailableForWebState();

  // Handle programmatic blocking/detachment for ineligible or hidden pages.
  if (!is_eligible) {
    active_page_context.geminiPageContextComputationState =
        ios::provider::GeminiPageContextComputationState::kBlocked;
    active_page_context.geminiPageContextAttachmentState =
        ios::provider::GetCurrentPageContextAttachmentState();
    active_page_context.uniquePageContext = nullptr;
  } else {
    // Apply user settings.
    ApplyUserPrefsToPageContext(active_page_context);

    // Persists manual detachment across navigations. If the user explicitly
    // detached the context via the paperclip UI, respect that choice over the
    // default attached state.
    if (active_page_context.geminiPageContextAttachmentState ==
            ios::provider::GeminiPageContextAttachmentState::kAttached &&
        ios::provider::GetCurrentPageContextAttachmentState() ==
            ios::provider::GeminiPageContextAttachmentState::kDetached) {
      active_page_context.geminiPageContextAttachmentState =
          ios::provider::GeminiPageContextAttachmentState::kDetached;
    }
  }

  // Save the active page context to `attached_tabs`. If we are on the tab
  // grid, the active page context will be saved as `kBlocked` unless we have
  // other tabs attached. This prevents the current tab from being erroneously
  // showed as `kBlocked` when we open the Floaty on a different attached tab.
  bool should_save_active_context = !IsTabGridVisible() || !HasSharedTabs();
  if (IsGeminiMultiTabContextEnabled() && active_web_state &&
      should_save_active_context) {
    SetAttachedPageContext(active_web_state->GetUniqueIdentifier(),
                           active_page_context);
  }

  ios::provider::UpdateActivePageContext(active_page_context, GetSharedTabs());
}

NSArray<GeminiPageContext*>* GeminiBrowserAgent::GetSharedTabs() const {
  NSMutableArray<GeminiPageContext*>* shared_tabs = [NSMutableArray array];
  web::WebStateID active_web_state_id = GetActiveWebStateID();

  for (const auto& [tab_id, context] : attached_tabs_) {
    if (tab_id != active_web_state_id) {
      [shared_tabs addObject:context];
    }
  }
  return shared_tabs;
}

bool GeminiBrowserAgent::HasSharedTabs() const {
  if (attached_tabs_.size() > 1) {
    return true;
  }
  return !attached_tabs_.empty() &&
         attached_tabs_.begin()->first != GetActiveWebStateID();
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

void GeminiBrowserAgent::PrepareFloatyToBeShown() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!IsFullscreenInitialized() || !web_state) {
    return;
  }

  CRWWebViewScrollViewProxy* scroll_view_proxy =
      web_state->GetWebViewProxy().scrollViewProxy;
  CGPoint current_offset = scroll_view_proxy.contentOffset;
  [scroll_view_proxy setContentOffset:current_offset animated:NO];

  if (IsFullscreenRefactoringEnabled()) {
    [HandlerForProtocol(browser_->GetCommandDispatcher(), FullscreenCommands)
        exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::
                                      kUserInitiatedFinishedByCode
                         animated:YES];
    fullscreen_disabler_ =
        std::make_unique<ScopedFullscreenDisabler>(HandlerForProtocol(
            browser_->GetCommandDispatcher(), FullscreenCommands));
  } else {
    fullscreen_controller_->ExitFullscreen();
    fullscreen_disabler_ =
        std::make_unique<ScopedFullscreenDisabler>(fullscreen_controller_);
  }
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

  fullscreen_disabler_.reset();
}

GeminiPageContext* GeminiBrowserAgent::CreatePartialPageContext(
    web::WebState* web_state) {
  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  if (tab_helper) {
    // If web state is realized, get partial page context via tab helper
    // to perform page context extraction eligibility checks.
    return tab_helper->GetPartialPageContext(/*forced=*/true);
  }

  // If the web state is unrealized, the tab has cached APC and we can bypass
  // page context extraction eligibility checks.
  return gemini::CreatePartialPageContextForWebState(web_state,
                                                     /*is_eligible=*/true);
}

void GeminiBrowserAgent::ApplyUserPrefsToPageContext(
    GeminiPageContext* gemini_page_context) {
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kUserDisabled;
  } else {
    // If page context is not disabled by the user, page context is always
    // available and should be attached. Note page context is only partially
    // available (e.g. title, url, favicon) while
    // `GeminiPageContextComputationState` is pending.
    gemini_page_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kAttached;
  }
}

void GeminiBrowserAgent::OnPageContentPrefChanged() {
  if (!browser_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kIOSBWGPageContentSetting)) {
    attached_tabs_.clear();
  }

  if (IsInGeminiLiveMode() &&
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

void GeminiBrowserAgent::OnMicrophonePrefChanged() {
  if (!browser_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kIOSGeminiLiveMicrophoneSetting) &&
      IsInGeminiLiveMode()) {
    SwitchToChatModeOrDismiss(/*animated=*/true);
  }
}

void GeminiBrowserAgent::SetSessionCommandHandlers() {
  CHECK(gemini_container_mediator_, base::NotFatalUntil::M155);
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  gemini_container_mediator_.gatewayManager.sessionHandler.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  gemini_container_mediator_.gatewayManager.sessionHandler.geminiHandler =
      HandlerForProtocol(dispatcher, GeminiCommands);
}

void GeminiBrowserAgent::OnPageContextGenerated(
    GeminiPageContext* gemini_page_context) {
  PropagatePageContextToProvider(gemini_page_context);
}

web::WebStateID GeminiBrowserAgent::GetActiveWebStateID() const {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  return active_web_state ? active_web_state->GetUniqueIdentifier()
                          : web::WebStateID();
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

void GeminiBrowserAgent::OnPersistTabContextLookupComplete(
    PersistTabContextBrowserAgent::PageContextMap contexts_map) {
  for (auto& [tab_id_str, proto_context] : contexts_map) {
    int32_t identifier;
    if (!base::StringToInt(tab_id_str, &identifier)) {
      continue;
    }
    web::WebStateID selected_tab =
        web::WebStateID::FromSerializedValue(identifier);

    if (proto_context.has_value()) {
      RetrieveCachedPageContextForTab(selected_tab,
                                      std::move(proto_context.value()));
    } else {
      GenerateFullPageContextForTab(selected_tab);
    }
  }
}

void GeminiBrowserAgent::RetrieveCachedPageContextForTab(
    web::WebStateID selected_tab,
    std::unique_ptr<optimization_guide::proto::PageContext> proto_context) {
  GeminiPageContext* partial_context = GetAttachedPageContext(selected_tab);
  if (!partial_context) {
    return;
  }

  // Create and populate full page context.
  GeminiPageContext* full_context = [[GeminiPageContext alloc] init];
  full_context.favicon = partial_context.favicon;
  full_context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kSuccess;
  full_context.uniquePageContext = std::move(proto_context);

  SnapshotBrowserAgent* snapshot_browser_agent =
      SnapshotBrowserAgent::FromBrowser(browser_);

  if (!snapshot_browser_agent) {
    OnFullPageContextAvailableForSharedTab(selected_tab, full_context);
    return;
  }

  // Retrieve a snapshot of the web state and attach it to the page context.
  // TODO(crbug.com/534752184): Move screenshot retrieval to PageContextWrapper.
  base::WeakPtr<GeminiBrowserAgent> weak_this = weak_factory_.GetWeakPtr();
  snapshot_browser_agent->RetrieveSnapshotWithID(
      SnapshotID(selected_tab), SnapshotKindColor, ^(UIImage* snapshot) {
        if (!snapshot || !full_context.uniquePageContext) {
          if (weak_this) {
            weak_this->OnFullPageContextAvailableForSharedTab(selected_tab,
                                                              full_context);
          }
          return;
        }

        std::unique_ptr<optimization_guide::proto::PageContext>
            extracted_proto = full_context.uniquePageContext;
        full_context.uniquePageContext = nullptr;

        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            // Background task performs heavy image encoding work off the
            // main thread.
            base::BindOnce(
                ^(std::unique_ptr<optimization_guide::proto::PageContext>
                      proto) {
                  NSData* image_data = UIImagePNGRepresentation(snapshot);
                  if (!image_data) {
                    return proto;
                  }

                  NSString* base64_string =
                      [image_data base64EncodedStringWithOptions:0];
                  proto->set_tab_screenshot(
                      base::SysNSStringToUTF8(base64_string));
                  return proto;
                },
                std::move(extracted_proto)),
            // Reply task updates page context and notifies the provider on
            // the main thread once image encoding is complete.
            base::BindOnce(
                [](base::WeakPtr<GeminiBrowserAgent> weak_this,
                   web::WebStateID selected_tab,
                   GeminiPageContext* full_context,
                   std::unique_ptr<optimization_guide::proto::PageContext>
                       proto) {
                  full_context.uniquePageContext = std::move(proto);
                  if (!weak_this) {
                    return;
                  }
                  weak_this->OnFullPageContextAvailableForSharedTab(
                      selected_tab, full_context);
                },
                weak_this, selected_tab, full_context));
      });
}

void GeminiBrowserAgent::GenerateFullPageContextForTab(
    web::WebStateID selected_tab) {
  web::WebState* web_state =
      GetWebState(browser_->GetWebStateList(),
                  WebStateSearchCriteria{.identifier = selected_tab});
  if (!web_state) {
    return;
  }

  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }

  tab_helper->GeneratePageContext(
      base::BindRepeating(
          &GeminiBrowserAgent::OnFullPageContextAvailableForSharedTab,
          weak_factory_.GetWeakPtr(), selected_tab),
      /*is_background_tab=*/true);
}

void GeminiBrowserAgent::OnFullPageContextAvailableForSharedTab(
    web::WebStateID web_state_id,
    GeminiPageContext* full_page_context) {
  GeminiPageContext* existing_context = GetAttachedPageContext(web_state_id);
  // The tab was un-shared or closed before full page context became available.
  if (!existing_context) {
    return;
  }

  full_page_context.geminiPageContextAttachmentState =
      existing_context.geminiPageContextAttachmentState;

  SetAttachedPageContext(web_state_id, full_page_context);

  // Re-evaluate and push the updated state to the provider.
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (active_web_state) {
    GeminiPageContext* active_page_context =
        GetAttachedPageContext(active_web_state->GetUniqueIdentifier());
    ios::provider::UpdateActivePageContext(active_page_context,
                                           GetSharedTabs());
  }
}

void GeminiBrowserAgent::DetachTabWithID(NSString* tab_id) {
  if (!IsGeminiMultiTabContextEnabled()) {
    return;
  }

  int32_t identifier_value;
  if (!base::StringToInt(base::SysNSStringToUTF8(tab_id), &identifier_value)) {
    return;
  }

  web::WebStateID detached_tab_id =
      web::WebStateID::FromSerializedValue(identifier_value);

  web::WebStateID active_web_state_id = GetActiveWebStateID();
  CHECK(detached_tab_id != active_web_state_id);

  RemoveAttachedPageContext(detached_tab_id);
  RecordGeminiTabDetached();

  GeminiPageContext* active_page_context =
      GetAttachedPageContext(active_web_state_id);
  ios::provider::UpdateActivePageContext(active_page_context, GetSharedTabs());
}

void GeminiBrowserAgent::UpdateLocalTabAttachmentState(
    NSString* tab_id,
    ios::provider::GeminiPageContextAttachmentState new_state) {
  int32_t identifier_value;
  if (!base::StringToInt(base::SysNSStringToUTF8(tab_id), &identifier_value)) {
    return;
  }

  web::WebStateID attached_tab_id =
      web::WebStateID::FromSerializedValue(identifier_value);

  if (GeminiPageContext* page_context =
          GetAttachedPageContext(attached_tab_id)) {
    page_context.geminiPageContextAttachmentState = new_state;
    if (new_state ==
        ios::provider::GeminiPageContextAttachmentState::kAttached) {
      RecordGeminiActiveTabAttached();
    } else if (new_state ==
               ios::provider::GeminiPageContextAttachmentState::kDetached) {
      RecordGeminiActiveTabDetached();
    }
  }
}

GeminiPageContext* GeminiBrowserAgent::GetAttachedPageContext(
    web::WebStateID tab_id) const {
  for (const auto& [id, context] : attached_tabs_) {
    if (id == tab_id) {
      return context;
    }
  }
  return nil;
}

void GeminiBrowserAgent::SetAttachedPageContext(
    web::WebStateID tab_id,
    GeminiPageContext* page_context) {
  for (auto& [id, existing_context] : attached_tabs_) {
    if (id == tab_id) {
      existing_context = page_context;
      return;
    }
  }
  attached_tabs_.emplace_back(tab_id, page_context);
}

void GeminiBrowserAgent::RemoveAttachedPageContext(web::WebStateID tab_id) {
  std::erase_if(attached_tabs_,
                [tab_id](const auto& pair) { return pair.first == tab_id; });
}
