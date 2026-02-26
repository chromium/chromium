// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <set>

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/types/expected.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

class Browser;

enum class PageContextWrapperError;

namespace gemini {
enum class FloatyUpdateSource;
}  // namespace gemini

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

@class BWGLinkOpeningHandler;
@class GeminiPageStateChangeHandler;
@class BWGSessionHandler;
@class GeminiCameraHandler;
@class GeminiPageContext;
@class GeminiViewStateChangeHandler;
@class GeminiScrollObserver;
@class GeminiSuggestionHandler;

@protocol BWGGatewayProtocol;

// A browser agent responsible for presenting the floaty and managing
// its protocol handlers.
class GeminiBrowserAgent : public BrowserUserData<GeminiBrowserAgent>,
                           public GeminiTabHelperObserver,
                           public FullscreenControllerObserver,
                           public TabsDependencyInstaller {
 public:
  GeminiBrowserAgent(const GeminiBrowserAgent&) = delete;
  GeminiBrowserAgent& operator=(const GeminiBrowserAgent&) = delete;

  ~GeminiBrowserAgent() override;

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // GeminiTabHelperObserver:
  void OnPageContextUpdated(web::WebState* web_state) override;
  void OnGeminiTabHelperDestroyed(BwgTabHelper* tab_helper) override;

  // Checks if the FRE needs to be shown and start the Gemini flow
  // accordingly.
  void StartGeminiFlow(UIViewController* base_view_controller,
                       GeminiStartupState* startup_state);

  // Presents the floaty on a given view controller in a pending state
  // with a partial PageContext.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead.
  void PresentFloatyWithPendingContext(
      UIViewController* base_view_controller,
      std::unique_ptr<optimization_guide::proto::PageContext> page_context,
      GeminiStartupState* startup_state);

  // Updates the page context for the floaty.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead (and let this be handled internally within the
  // browser agent).
  void UpdateFloatyPageContext(
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

  // Called when the Gemini view state expands.
  void OnGeminiViewStateExpanded();

  // Dismisses the floaty and resets the Gemini flow.
  void DismissFloaty();

  // Hide Gemini floaty with `animated` flag. When in a hidden state, the floaty
  // view is dismissed but still persists in memory and needs to be properly
  // cleaned up. Properly cleaning up the floaty can be done by resetting the
  // Gemini instance. Passes what `source` triggered the floaty to be hidden.
  void HideFloatyIfInvoked(bool animated, gemini::FloatyUpdateSource source);

  // TODO(crbug.com/483848831): Rename to a more accurate method name.
  // Show Gemini floaty with `animated` flag. Used to re-show an invoked Gemini
  // floaty with the `last_view_state_`. Passes what `source` triggered the
  // floaty to be shown.
  void ShowFloatyIfInvoked(bool animated, gemini::FloatyUpdateSource source);

  // Collapses floaty if invoked.
  void CollapseFloatyIfInvoked();

  // Setter for `last_shown_view_state_`.
  void SetLastShownViewState(ios::provider::GeminiViewState view_state);

  // Called when trait collection is updated.
  void UpdateForTraitCollection(UITraitCollection* traitCollection);

  // Dismisses Gemini from all other windows and executes the completion block.
  void DismissGeminiFromOtherWindows(base::OnceClosure completion);

 private:
  explicit GeminiBrowserAgent(Browser* browser);
  friend class BrowserUserData<GeminiBrowserAgent>;
  friend class GeminiBrowserAgentTest;

  // Starts the Gemini session (prepares context and shows overlay).
  void PresentFloaty(UIViewController* base_view_controller,
                     GeminiStartupState* startup_state,
                     bool first_run_shown);

  // Presents the floaty on a given view controller in a pending state
  // with partial PageContext and optional image attachment.
  void PresentFloatyWithPendingContext(UIViewController* base_view_controller,
                                       GeminiStartupState* startup_state);

  // Presents the floaty on a given view controller with page context,
  // given specific computation state and optional image attachment (can be
  // nil).
  void PresentFloatyWithState(
      UIViewController* base_view_controller,
      std::unique_ptr<optimization_guide::proto::PageContext>
          page_context_proto,
      ios::provider::GeminiPageContextComputationState computation_state,
      GeminiStartupState* startup_state);

  // Fetches the favicon for the page or a default favicon if not available.
  UIImage* FetchPageFavicon();

  // Adjusts the configuration around the Gemini page context based on user
  // prefs.
  void ApplyUserPrefsToPageContext(GeminiPageContext* gemini_page_context);

  // Callback for when the page load takes too long, triggers best effort page
  // context generation.
  void TriggerBestEffortPageContextGeneration();

  // Sets the UI command handlers on the session handler. This cannot be called
  // in the constructor because some objects fail the protocol conformance test
  // at that time.
  void SetSessionCommandHandlers();

  // Helper to get the BwgTabHelper for the active web state if it matches the
  // provided web state.
  BwgTabHelper* GetActiveTabHelper(web::WebState* web_state);

  // Callback for scroll events.
  void OnScrollEvent();

  // FullscreenControllerObserver:
  void FullscreenProgressUpdated(FullscreenController* controller,
                                 CGFloat progress) override;
  void FullscreenWillAnimate(FullscreenController* controller,
                             FullscreenAnimator* animator) override;
  void FullscreenDidAnimate(FullscreenController* controller,
                            FullscreenAnimatorStyle style) override;
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;
  void FullscreenViewportInsetRangeChanged(
      FullscreenController* controller,
      UIEdgeInsets min_viewport_insets,
      UIEdgeInsets max_viewport_insets) override;

  // Returns true if the user has completed the FRE.
  bool HasCompletedFirstRun();

  // Returns the floaty offset from a FullscreenController.
  CGFloat GetFloatyOffsetFromFullscreenController(
      FullscreenController* controller);

  // Invokes the floaty.
  void InvokeFloaty(GeminiConfiguration* config);

  // Forces the floaty to be shown if it is invoked. Can be used to set the
  // floaty opacity to 1.0 effectively re-showing the floaty. Useful to re-show
  // the floaty if a user is currently in fullscreen mode.
  void ForceShowFloatyIfInvoked();

  // Whether to allow the floaty to be shown given a `source`. If not allowed,
  // the floaty state will be as if a floaty was never shown.
  bool ShouldShowFloatyForSource(gemini::FloatyUpdateSource source);

  // Creates a `GeminiPageContext` for the current web state.
  GeminiPageContext* CreateGeminiPageContext(
      ios::provider::GeminiPageContextComputationState computation_state,
      std::unique_ptr<optimization_guide::proto::PageContext>
          page_context_proto);

  // Updates the presented source, if any, of the active tab helper.
  void UpdateActiveTabHelperWithPresentedSource(
      gemini::FloatyUpdateSource source,
      bool is_presented);

  // Returns true if the floaty is temporarily hidden.
  bool IsFloatyTemporarilyHidden() const;

  // Returns true if the floaty is only hidden by the keyboard.
  bool IsOnlyHiddenByKeyboard() const;

  // The gateway for bridging internal protocols.
  __strong id<BWGGatewayProtocol> bwg_gateway_ = nullptr;

  // Handler for opening links from BWG.
  __strong BWGLinkOpeningHandler* bwg_link_opening_handler_ = nullptr;

  // Handler for PageState changes.
  __strong GeminiPageStateChangeHandler* gemini_page_state_change_handler_ =
      nullptr;

  // Handler for the BWG sessions.
  __strong BWGSessionHandler* bwg_session_handler_ = nullptr;

  // Handler for Gemini camera.
  __strong GeminiCameraHandler* gemini_camera_handler_ = nullptr;

  // Handler for Gemini suggestion chips.
  __strong GeminiSuggestionHandler* gemini_suggestion_handler_ = nullptr;

  // Delegate implementation for BWGSessionHandler.
  __strong GeminiViewStateChangeHandler* gemini_view_state_handler_ = nullptr;

  // Reference to fullscreen controller. Used to observe fullscreen progress
  // updates related to the Gemini overlay.
  raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;

  // Observers for keyboard events.
  id keyboard_show_observer_ = nil;
  id keyboard_hide_observer_ = nil;

  // Observer for scroll events.
  __strong GeminiScrollObserver* scroll_observer_ = nullptr;

  // Whether the keyboard is currently visible.
  bool is_keyboard_visible_ = false;

  // Set of sources currently hiding the floaty. If this set is not empty, the
  // floaty is considered temporarily hidden.
  std::set<gemini::FloatyUpdateSource> active_hiding_sources_;

  // Called when keyboard state changes.
  void OnKeyboardStateChanged(bool is_visible);

  // Used to track the last shown view state of an invoked floaty. Used to show
  // a hidden floaty with the previous view state.
  ios::provider::GeminiViewState last_shown_view_state_ =
      ios::provider::GeminiViewState::kUnknown;

  // Whether the floaty is currently invoked.
  bool is_floaty_invoked_ = false;

  // Records when the floaty was last hidden. Prevents the floaty from
  // reappearing too soon, particularly after a
  // `HideFloatyIfInvoked()` call during parent/child view
  // transitions.
  base::TimeTicks floaty_hidden_timestamp_;

  // Tracks the elapsed time a floaty is minimized until it's expanded. If the
  // floaty is expanded, the time is reset to null.
  base::TimeTicks elapsed_minimized_floaty_time_;

  // Whether an external overlay is currently presented e.g. Lens Overlay. Used
  // to avoid showing the floaty when view controllers are presented/dismissed
  // while an overlay is presented.
  bool is_external_overlay_presented_ = false;

  // Whether an alert is currently presented. Used to avoid showing the floaty
  // when view controllers are presented/dismissed while an alert is presented.
  bool is_alert_presented_ = false;

  // Whether a banner is currently presented. Used to avoid showing the floaty
  // when view controllers are presented/dismissed while a banner is presented.
  bool is_banner_presented_ = false;

  // Registrar for pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Called when the page content sharing preference changes.
  void OnPageContentPrefChanged();

  // Timer to force page context generation if page load takes too long.
  base::OneShotTimer page_context_timeout_timer_;

  // Weak pointer factory.
  base::WeakPtrFactory<GeminiBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
