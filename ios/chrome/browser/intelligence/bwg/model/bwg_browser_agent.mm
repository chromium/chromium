// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
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
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ui/gfx/image/image.h"

namespace {

// Helper to convert PageContextWrapperError to BWGPageContextComputationState.
ios::provider::BWGPageContextComputationState
BWGPageContextComputationStateFromPageContextWrapperError(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kForceDetachError:
      return ios::provider::BWGPageContextComputationState::kProtected;
    default:
      return ios::provider::BWGPageContextComputationState::kError;
  }
}

// The offset for the Gemini overlay when fullscreen mode is enabled. Fullscreen
// mode is enabled when the fullscreen progress value is 0. A negative value
// will bring the overlay towards the bottom of the viewport while a positive
// value will do the reverse.
CGFloat kOverlayFullscreenOffset = -100;

// Used for forcing fullscreen progress value.
CGFloat kFullscreenEnabled = 0.0;

// Used for forcing non-fullscreen progress value.
CGFloat kFullscreenDisabled = 1.0;

// Used to check if floaty visibility updates are part of a UIView dismissal or
// presentation.
double kViewTransitionTime = 1.5;

}  // namespace

BwgBrowserAgent::BwgBrowserAgent(Browser* browser) : BrowserUserData(browser) {
  if (IsGeminiCopresenceEnabled()) {
    StartObserving(browser_, TabsDependencyInstaller::Policy::kOnlyRealized);

    pref_change_registrar_.Init(browser_->GetProfile()->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kIOSBWGPageContentSetting,
        base::BindRepeating(&BwgBrowserAgent::OnPageContentPrefChanged,
                            base::Unretained(this)));
  }

  bwg_gateway_ = ios::provider::CreateBWGGateway();

  if (bwg_gateway_) {
    bwg_link_opening_handler_ = [[BWGLinkOpeningHandler alloc]
        initWithURLLoader:UrlLoadingBrowserAgent::FromBrowser(browser_)];
    bwg_page_state_change_handler_ = [[BWGPageStateChangeHandler alloc]
        initWithPrefService:browser_->GetProfile()->GetPrefs()];
    bwg_gateway_.pageStateChangeHandler = bwg_page_state_change_handler_;

    bwg_session_handler_ = [[BWGSessionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    if (IsGeminiCopresenceEnabled()) {
      gemini_view_state_handler_ = [[GeminiViewStateChangeHandler alloc]
          initWithBrowserAgent:weak_factory_.GetWeakPtr()];
      bwg_session_handler_.geminiViewStateDelegate = gemini_view_state_handler_;
      bwg_link_opening_handler_.geminiViewStateDelegate =
          gemini_view_state_handler_;
    }
    bwg_gateway_.sessionHandler = bwg_session_handler_;
    bwg_gateway_.linkOpeningHandler = bwg_link_opening_handler_;

    gemini_suggestion_handler_ = [[GeminiSuggestionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    bwg_gateway_.suggestionHandler = gemini_suggestion_handler_;

    if (IsGeminiImageRemixToolEnabled()) {
      gemini_camera_handler_ = [[GeminiCameraHandler alloc]
          initWithPrefService:browser_->GetProfile()->GetPrefs()];
      bwg_gateway_.cameraHandler = gemini_camera_handler_;
    }
  }

  // Ensures a `FullscreenController` is created.
  if (IsGeminiCopresenceEnabled()) {
    FullscreenController::CreateForBrowser(browser_);
    fullscreen_controller_ = FullscreenController::FromBrowser(browser_);
    fullscreen_controller_->AddObserver(this);

    base::WeakPtr<BwgBrowserAgent> weak_ptr = weak_factory_.GetWeakPtr();
    keyboard_show_observer_ = [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardWillShowNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  if (weak_ptr) {
                    weak_ptr->OnKeyboardStateChanged(true);
                  }
                }];
    keyboard_hide_observer_ = [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardWillHideNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  if (weak_ptr) {
                    weak_ptr->OnKeyboardStateChanged(false);
                  }
                }];
  }
}

BwgBrowserAgent::~BwgBrowserAgent() {
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

  if (fullscreen_controller_) {
    fullscreen_controller_->RemoveObserver(this);
    fullscreen_controller_ = nullptr;
  }

  StopObserving();
}

void BwgBrowserAgent::OnKeyboardStateChanged(bool is_visible) {
  is_keyboard_visible_ = is_visible;
  if (fullscreen_controller_) {
    // Re-trigger the update with the current progress to apply opacity override
    // if needed.
    FullscreenProgressUpdated(fullscreen_controller_,
                              fullscreen_controller_->GetProgress());
  }
}

void BwgBrowserAgent::StartGeminiFlow(UIViewController* base_view_controller,
                                      UIImage* image_attachment,
                                      gemini::EntryPoint entry_point) {
  bool will_show_first_run = !HasCompletedFirstRun();
  RecordGeminiEntryPointClick(entry_point, will_show_first_run);

  // Check if the user has already consented or if the consent flow should be
  // skipped.
  bool skip_consent = BWGPromoConsentVariationsParam() ==
                      BWGPromoConsentVariations::kSkipConsent;

  if (!will_show_first_run || skip_consent) {
    PresentFloaty(base_view_controller, image_attachment, entry_point,
                  /*was_first_run_shown=*/false);
    return;
  }

  id<BWGCommands> gemini_commands_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);
  base::WeakPtr<BwgBrowserAgent> weak_ptr = weak_factory_.GetWeakPtr();
  [gemini_commands_handler
      startGeminiFREWithCompletion:^(BOOL success) {
        if (success) {
          if (weak_ptr) {
            weak_ptr->PresentFloaty(base_view_controller, image_attachment,
                                    entry_point,
                                    /*first_run_shown=*/true);
          }
        }
      }
                    fromEntryPoint:entry_point];
}

bool BwgBrowserAgent::HasCompletedFirstRun() {
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();

  // If we are forcing the FRE, reset the consent pref and return false.
  if (BWGPromoConsentVariationsParam() ==
      BWGPromoConsentVariations::kForceFRE) {
    pref_service->SetBoolean(prefs::kIOSBwgConsent, false);
    return false;
  }

  return pref_service->GetBoolean(prefs::kIOSBwgConsent);
}

CGFloat BwgBrowserAgent::GetFloatyOffsetFromFullscreenController(
    FullscreenController* controller) {
  CGFloat fullyExpandedBottomToolbarHeight =
      controller->GetMaxViewportInsets().bottom;
  CGFloat offset = fullyExpandedBottomToolbarHeight +
                   kOverlayFullscreenOffset * (1.0 - controller->GetProgress());
  return offset;
}

void BwgBrowserAgent::UpdateForTraitCollection(
    UITraitCollection* traitCollection) {
  // Update the offset for a device orientation update to landscape or portrait.
  CGFloat offset =
      GetFloatyOffsetFromFullscreenController(fullscreen_controller_);
  ios::provider::UpdateOverlayOffsetWithOpacity(offset, 1.0);
}

void BwgBrowserAgent::PresentFloaty(UIViewController* base_view_controller,
                                    UIImage* image_attachment,
                                    gemini::EntryPoint entry_point,
                                    bool first_run_shown) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // Trigger zero state suggestions.
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  if (!gemini_tab_helper) {
    return;
  }

  if (IsZeroStateSuggestionsAskGeminiEnabled()) {
    gemini_tab_helper->ExecuteZeroStateSuggestions(
        base::BindOnce(^(NSArray<NSString*>* suggestions){
            // No-op.
        }));
  }

  // Configure the callback to be executed once the page context is ready.
  base::WeakPtr<BwgBrowserAgent> weak_ptr = weak_factory_.GetWeakPtr();
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback;

  if (IsGeminiImmediateOverlayEnabled()) {
    // Present the overlay immediately without page context.
    PresentFloatyWithPendingContext(base_view_controller, entry_point,
                                    image_attachment);

    page_context_completion_callback = base::BindOnce(
        [](base::WeakPtr<BwgBrowserAgent> weak_ptr,
           PageContextWrapperCallbackResponse response) {
          if (weak_ptr) {
            weak_ptr->UpdateFloatyPageContext(std::move(response));
          }
        },
        weak_ptr);

    base::UmaHistogramLongTimes(first_run_shown ? kStartupTimeWithFREHistogram
                                                : kStartupTimeNoFREHistogram,
                                base::TimeTicks::Now() - start_time);
  } else {
    page_context_completion_callback = base::BindOnce(
        &BwgBrowserAgent::OnPageContextReady, weak_factory_.GetWeakPtr(),
        base_view_controller, image_attachment, start_time, first_run_shown,
        entry_point);
  }

  gemini_tab_helper->GeneratePageContext(
      std::move(page_context_completion_callback),
      /*full_page_context=*/true);
}

void BwgBrowserAgent::PresentFloatyWithPageContext(
    UIViewController* base_view_controller,
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context,
    gemini::EntryPoint entry_point) {
  if (expected_page_context.has_value()) {
    PresentFloatyWithState(
        base_view_controller, std::move(expected_page_context.value()),
        ios::provider::BWGPageContextComputationState::kSuccess, entry_point);
  } else {
    PresentFloatyWithState(
        base_view_controller,
        /*page_context_proto=*/nullptr,
        BWGPageContextComputationStateFromPageContextWrapperError(
            expected_page_context.error()),
        entry_point);
  }
}

void BwgBrowserAgent::PresentFloatyWithPendingContext(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context,
    gemini::EntryPoint entry_point) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper) {
    return;
  }
  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  PresentFloatyWithState(base_view_controller, std::move(page_context),
                         gemini_page_context.BWGPageContextComputationState,
                         entry_point);
}

void BwgBrowserAgent::PresentFloatyWithPendingContext(
    UIViewController* base_view_controller,
    gemini::EntryPoint entry_point,
    UIImage* image_attachment) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper) {
    return;
  }
  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();

  std::unique_ptr<optimization_guide::proto::PageContext> partial_page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  partial_page_context->set_url(active_web_state->GetVisibleURL().spec());
  partial_page_context->set_title(
      base::UTF16ToUTF8(active_web_state->GetTitle()));

  PresentFloatyWithState(base_view_controller, std::move(partial_page_context),
                         gemini_page_context.BWGPageContextComputationState,
                         entry_point, image_attachment);
}

void BwgBrowserAgent::UpdateFloatyPageContext(
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
  if (!tab_helper) {
    return;
  }

  GeminiPageContext* gemini_page_context = [[GeminiPageContext alloc] init];
  gemini_page_context.BWGPageContextComputationState =
      tab_helper->GetIsGeminiEligible().value_or(true)
          ? ios::provider::BWGPageContextComputationState::kSuccess
          : ios::provider::BWGPageContextComputationState::kBlocked;
  std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto =
      nullptr;
  if (expected_page_context.has_value()) {
    page_context_proto = std::move(expected_page_context.value());
  } else {
    gemini_page_context.BWGPageContextComputationState =
        BWGPageContextComputationStateFromPageContextWrapperError(
            expected_page_context.error());
  }
  gemini_page_context.uniquePageContext = std::move(page_context_proto);
  gemini_page_context.favicon = FetchPageFavicon();

  ApplyUserPrefsToPageContext(gemini_page_context);
  ios::provider::UpdatePageContext(gemini_page_context);
}

void BwgBrowserAgent::OnGeminiViewStateExpanded() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* tab_helper = GetActiveTabHelper(active_web_state);

  if (tab_helper) {
    tab_helper->SetBwgUiShowing(true);
    tab_helper->GeneratePageContext(
        base::BindOnce(&BwgBrowserAgent::UpdateFloatyPageContext,
                       weak_factory_.GetWeakPtr()),
        /*full_page_context=*/true);
  }
  // Show page attachment UI chip every time the floaty is expanded.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

void BwgBrowserAgent::CollapseFloatyIfInvoked() {
  if (!is_floaty_invoked_) {
    return;
  }

  ios::provider::UpdateGeminiViewState(
      ios::provider::GeminiViewState::kCollapsed, /*animated=*/true);
}

void BwgBrowserAgent::SetLastShownViewState(
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

void BwgBrowserAgent::DismissGeminiFromOtherWindows(
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

void BwgBrowserAgent::DismissFloaty() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* gemini_tab_helper = GetActiveTabHelper(active_web_state);
  if (gemini_tab_helper) {
    gemini_tab_helper->SetBwgUiShowing(false);
  }

  // If the floaty is temporarily hidden i.e. as part of a view controller being
  // shown underneath the Gemini floaty, don't clean up and reset internal
  // Gemini properties. Clean up should occur if a user taps the floaty to
  // dismiss it or the browser agent destructing.
  if (is_floaty_temporarily_hidden_) {
    return;
  }

  if (last_shown_view_state_ == ios::provider::GeminiViewState::kCollapsed) {
    RecordFloatyDismissedWhileCollapsed();
  }

  is_floaty_invoked_ = false;
  ios::provider::ResetGemini();
}

void BwgBrowserAgent::HideFloatyIfInvoked(bool animated) {
  if (!is_floaty_invoked_) {
    return;
  }

  is_floaty_temporarily_hidden_ = true;
  floaty_hidden_timestamp_ = base::TimeTicks::Now();
  ios::provider::GeminiViewState current_view_state =
      ios::provider::GetCurrentGeminiViewState();
  SetLastShownViewState(current_view_state);

  ios::provider::UpdateGeminiViewState(ios::provider::GeminiViewState::kHidden,
                                       animated);
}

void BwgBrowserAgent::ShowFloatyIfInvoked(bool animated) {
  if (!is_floaty_invoked_ || !is_floaty_temporarily_hidden_) {
    return;
  }

  // `HideFloatyIfInvoked()` may be called when a view controller
  // dismisses. If a view controller dismisses as part of presenting another
  // view controller, the floaty should not show.
  base::TimeDelta time_since_last_hidden =
      base::TimeTicks::Now() - floaty_hidden_timestamp_;
  if (time_since_last_hidden <= base::Seconds(kViewTransitionTime)) {
    return;
  }

  RecordGeminiViewStateHiddenToShown(last_shown_view_state_);
  is_floaty_temporarily_hidden_ = false;
  ios::provider::UpdateGeminiViewState(last_shown_view_state_, animated);
}

#pragma mark - TabsDependencyInstaller

void BwgBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  // No-op. We only observe the active WebState, handled in
  // OnActiveWebStateChanged.
}

void BwgBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  // No-op. If the active WebState is removed, OnActiveWebStateChanged will
  // handle the detachment.
}

void BwgBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {
  // No-op, handled by OnWebStateRemoved or OnBwgTabHelperDestroyed.
}

void BwgBrowserAgent::OnActiveWebStateChanged(web::WebState* old_active,
                                              web::WebState* new_active) {
  if (old_active) {
    BwgTabHelper* old_tab_helper = BwgTabHelper::FromWebState(old_active);
    if (old_tab_helper) {
      old_tab_helper->RemoveObserver(this);
    }
  }

  if (new_active) {
    BwgTabHelper* new_tab_helper = GetActiveTabHelper(new_active);
    if (new_tab_helper) {
      new_tab_helper->AddObserver(this);
      // Propagate the context of the new active tab.
      OnPageContextUpdated(new_active);
    }
  }
}

#pragma mark - GeminiTabHelperObserver

void BwgBrowserAgent::OnPageContextUpdated(web::WebState* web_state) {
  BwgTabHelper* tab_helper = GetActiveTabHelper(web_state);
  if (!tab_helper) {
    return;
  }

  GeminiPageContext* gemini_page_context = tab_helper->GetPartialPageContext();
  ApplyUserPrefsToPageContext(gemini_page_context);

  // This update is from the active tab. Propagate it to the provider.
  ios::provider::UpdatePageContext(gemini_page_context);
}

void BwgBrowserAgent::OnGeminiTabHelperDestroyed(BwgTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
}

#pragma mark - FullscreenControllerObserver

void BwgBrowserAgent::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  // Catch-all in case the floaty is still in a temporarily hidden state. A
  // fullscreen update implies a user is interacting with the web page,
  // therefore we should force-show the floaty if invoked. Uses the command
  // handler to do eligibility checks outside of this browser agent before
  // showing the floaty.
  if (is_floaty_temporarily_hidden_) {
    id<BWGCommands> gemini_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);
    [gemini_handler showFloatyIfInvokedAnimated:NO];
  }

  CGFloat offset = GetFloatyOffsetFromFullscreenController(controller);

  // When fullscreen mode is disabled (progress == 1), the offset will be a
  // positive value equal to the `fullyExpandedBottomToolbarHeight`. When
  // fullscreen mode is enabled (progress == 0), the offset will be a negative
  // value, `kOverlayFullscreenOffset`.
  if (is_keyboard_visible_) {
    // When the keyboard is visible, force the opacity to 1.0 (fully opaque) to
    // prevent the floaty from disappearing, even if the fullscreen progress is
    // 0 (enabled).
    ios::provider::UpdateOverlayOffsetWithOpacity(offset, 1.0);
  } else {
    ios::provider::UpdateOverlayOffsetWithOpacity(offset, progress);
  }
}

void BwgBrowserAgent::FullscreenWillAnimate(FullscreenController* controller,
                                            FullscreenAnimator* animator) {
  base::WeakPtr<BwgBrowserAgent> weak_ptr = weak_factory_.GetWeakPtr();
  [animator addAnimations:^{
    if (weak_ptr) {
      weak_ptr->FullscreenProgressUpdated(
          controller, controller->GetProgress() < 0.5 ? kFullscreenEnabled
                                                      : kFullscreenDisabled);
    }
  }];
}

void BwgBrowserAgent::FullscreenDidAnimate(FullscreenController* controller,
                                           FullscreenAnimatorStyle style) {
  if (style == FullscreenAnimatorStyle::ENTER_FULLSCREEN) {
    FullscreenProgressUpdated(controller, kFullscreenEnabled);
  } else {
    FullscreenProgressUpdated(controller, kFullscreenDisabled);
  }
}

void BwgBrowserAgent::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  controller->RemoveObserver(this);
  fullscreen_controller_ = nullptr;
}

void BwgBrowserAgent::FullscreenViewportInsetRangeChanged(
    FullscreenController* controller,
    UIEdgeInsets min_viewport_insets,
    UIEdgeInsets max_viewport_insets) {
  FullscreenProgressUpdated(controller, controller->GetProgress());
}

#pragma mark - Private

void BwgBrowserAgent::PresentFloatyWithState(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto,
    ios::provider::BWGPageContextComputationState computation_state,
    gemini::EntryPoint entry_point,
    UIImage* image_attachment) {
  SetSessionCommandHandlers();
  [bwg_page_state_change_handler_ setBaseViewController:base_view_controller];

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
  BwgTabHelper* gemini_tab_helper = GetActiveTabHelper(web_state);
  config.clientID = base::SysUTF8ToNSString(gemini_tab_helper->GetClientId());
  std::optional<std::string> maybe_server_id = gemini_tab_helper->GetServerId();
  config.serverID =
      maybe_server_id ? base::SysUTF8ToNSString(*maybe_server_id) : nil;
  config.shouldAnimatePresentation =
      !gemini_tab_helper->GetIsBwgSessionActiveInBackground();
  config.lastInteractionURLDifferent =
      gemini_tab_helper->IsLastInteractionUrlDifferent();
  config.shouldShowSuggestionChips =
      gemini_tab_helper->ShouldShowSuggestionChips();
  config.contextualCueChipLabel = gemini_tab_helper->GetContextualCueLabel();
  config.imageRemixIPHShouldShow =
      entry_point == gemini::EntryPoint::ImageRemixIPH;

  // Set the location permission state.
  // TODO(crbug.com/426207968): Populate with actual value.
  config.BWGLocationPermissionState =
      ios::provider::BWGLocationPermissionState::kUnknown;

  // Set the page context itself and page context computation/attachment state
  // for the current web state.
  config.pageContext = [[GeminiPageContext alloc] init];
  config.pageContext.BWGPageContextComputationState = computation_state;
  config.pageContext.uniquePageContext = std::move(page_context_proto);
  config.pageContext.favicon = FetchPageFavicon();
  ApplyUserPrefsToPageContext(config.pageContext);
  if (IsGeminiCopresenceEnabled()) {
    config.initialBottomOffset =
        GetFloatyOffsetFromFullscreenController(fullscreen_controller_);
  }

  // Start the overlay and update the tab helper to reflect this.
  ios::provider::StartBwgOverlay(config);
  gemini_tab_helper->SetBwgUiShowing(true);
  if (IsGeminiCopresenceEnabled()) {
    last_shown_view_state_ = ios::provider::GetCurrentGeminiViewState();
    is_floaty_invoked_ = true;
  }
}

UIImage* BwgBrowserAgent::FetchPageFavicon() {
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

void BwgBrowserAgent::ApplyUserPrefsToPageContext(
    GeminiPageContext* gemini_page_context) {
  // Disable the page context attachment state based on user prefs.
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    gemini_page_context.BWGPageContextAttachmentState =
        ios::provider::BWGPageContextAttachmentState::kUserDisabled;
  } else {
    // If page context is not disabled by the user, page context is always
    // available and should be attached. Note page context is only partially
    // available (e.g. title, url, favicon) while
    // `BWGPageContextComputationState` is pending.
    gemini_page_context.BWGPageContextAttachmentState =
        ios::provider::BWGPageContextAttachmentState::kAttached;
  }
}

void BwgBrowserAgent::OnPageContextReady(
    UIViewController* base_view_controller,
    UIImage* image_attachment,
    base::TimeTicks start_time,
    bool first_run_shown,
    gemini::EntryPoint entry_point,
    PageContextWrapperCallbackResponse response) {
  if (response.has_value()) {
    PresentFloatyWithState(
        base_view_controller, std::move(response.value()),
        ios::provider::BWGPageContextComputationState::kSuccess, entry_point,
        image_attachment);
  } else {
    PresentFloatyWithState(
        base_view_controller, nullptr,
        BWGPageContextComputationStateFromPageContextWrapperError(
            response.error()),
        entry_point, image_attachment);
  }

  base::UmaHistogramLongTimes(first_run_shown ? kStartupTimeWithFREHistogram
                                              : kStartupTimeNoFREHistogram,
                              base::TimeTicks::Now() - start_time);
}

void BwgBrowserAgent::SetSessionCommandHandlers() {
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
  id<BWGCommands> bwg_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);

  bwg_session_handler_.settingsHandler = settings_handler;
  bwg_session_handler_.BWGHandler = bwg_handler;
}

void BwgBrowserAgent::OnPageContentPrefChanged() {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  BwgTabHelper* tab_helper = GetActiveTabHelper(active_web_state);
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

BwgTabHelper* BwgBrowserAgent::GetActiveTabHelper(web::WebState* web_state) {
  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (active_web_state && active_web_state == web_state) {
    BwgTabHelper* tab_helper = BwgTabHelper::FromWebState(web_state);
    if (tab_helper) {
      return tab_helper;
    }
  }
  return nullptr;
}
