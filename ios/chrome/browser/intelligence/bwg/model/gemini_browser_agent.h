// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <set>

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "base/types/expected.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

class Browser;
class FullscreenController;
class AppBarMediatorTest;

enum class PageContextWrapperError;

namespace gemini {
enum class FloatyUpdateSource;
}  // namespace gemini

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

class ScopedFullscreenDisabler;
@class GeminiLinkOpeningHandler;
@class GeminiPageStateChangeHandler;
@class GeminiSessionHandler;
@class GeminiCameraHandler;
@class GeminiPageContext;
@class GeminiViewStateChangeHandler;
@class GeminiScrollObserver;
@class GeminiSceneStateObserver;
@class GeminiSuggestionHandler;
@class GeminiActuationHandler;

@protocol BWGGatewayProtocol;
@protocol FullscreenCommands;

// A browser agent responsible for presenting the floaty and managing
// its protocol handlers.
class GeminiBrowserAgent : public BrowserUserData<GeminiBrowserAgent>,
                           public GeminiTabHelperObserver,
                           public FullscreenControllerObserver,
                           public FullscreenBrowserAgentObserver,
                           public TabsDependencyInstaller,
                           public BrowserObserver,
                           public signin::IdentityManager::Observer,
                           public GeminiViewStateChangeHandlerTarget {
 public:
  // Observer interface for GeminiBrowserAgent.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the floaty invocation state changes.
    virtual void OnFloatyInvokedChanged(bool is_invoked) {}

    // Called when Gemini availability for the active web state changes.
    virtual void OnGeminiAvailabilityChanged(bool available) {}
  };

  GeminiBrowserAgent(const GeminiBrowserAgent&) = delete;
  GeminiBrowserAgent& operator=(const GeminiBrowserAgent&) = delete;

  ~GeminiBrowserAgent() override;

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the floaty is currently invoked.
  bool is_floaty_invoked() const { return is_floaty_invoked_; }

  // Returns true if Gemini is available for the active web state.
  bool IsGeminiAvailableForActiveWebState() const;

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // GeminiTabHelperObserver:
  void OnPageContextUpdated(web::WebState* web_state) override;
  void OnGeminiTabHelperDestroyed(GeminiTabHelper* tab_helper) override;

  // Checks if the FRE needs to be shown and start the Gemini flow
  // accordingly.
  void StartGeminiFlow(UIViewController* base_view_controller,
                       GeminiStartupState* startup_state);

  // Updates the page context for the floaty.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead (and let this be handled internally within the
  // browser agent).
  void UpdateFloatyPageContext(
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

  // Updates the page context for the floaty after cancelling the timeout.
  // TODO(crbug.com/465535924): Deprecated, new callers should use
  // `StartGeminiFlow` instead (and let this be handled internally within the
  // browser agent).
  void CancelTimeoutAndUpdateFloatyPageContext(
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

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

  void OnViewStateChanged(ios::provider::GeminiViewState view_state) override;
  void OnProcessingStatusChanged(
      ios::provider::GeminiClientMode processing_status) override;
  void CollapseFloatyIfInvoked() override;
  void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) override;

  // Called when the scene activation level changes.
  void OnSceneActivationLevelChanged(SceneActivationLevel level);

  // Called when trait collection is updated.
  void UpdateForTraitCollection(UITraitCollection* traitCollection);

  // Dismisses Gemini from all other windows and executes the completion block.
  void DismissGeminiFromOtherWindows(base::OnceClosure completion);

 private:
  explicit GeminiBrowserAgent(Browser* browser);
  friend class BrowserUserData<GeminiBrowserAgent>;
  friend class GeminiBrowserAgentTest;
  friend class AppBarMediatorTest;

  // Fetches the full context of the active page and feeds it to Gemini.
  void UpdateGeminiPageContext();

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

  // Helper to get the GeminiTabHelper for the active web state if it matches
  // the provided web state.
  GeminiTabHelper* GetActiveTabHelper(web::WebState* web_state);

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

  // FullscreenBrowserAgentObserver:
  void WillUpdateState(FullscreenBrowserAgent* agent) override;
  void DidUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override;
  void WillShutDown(FullscreenBrowserAgent* agent) override;

  // Returns true if the user has completed the FRE.
  bool HasCompletedFirstRun();

  // Shows a snackbar message informing the user that sign-in is required.
  void ShowSignInRequiredSnackbar(gemini::EntryPoint entry_point);

  // Returns the floaty offset based on current fullscreen progress.
  CGFloat GetFloatyOffset();

  // Returns the floaty progress based on current fullscreen state.
  CGFloat GetFloatyProgress();

  // Invokes the floaty.
  void InvokeFloaty(GeminiConfiguration* config);

  // Forces the floaty to be shown if it is invoked. Can be used to set the
  // floaty opacity to 1.0 effectively re-showing the floaty. Useful to re-show
  // the floaty if a user is currently in fullscreen mode.
  void ForceShowFloatyIfInvoked();

  // Forces the floaty to be dismissed and cleaned up, ignoring if it is
  // temporarily hidden.
  void ForceDismissFloaty();

  // Whether to allow the floaty to be shown given a `source`. If not allowed,
  // the floaty state will be as if a floaty was never shown.
  bool ShouldShowFloatyForSource(gemini::FloatyUpdateSource source);

  // Prepares the floaty to be shown by exiting fullscreen and stopping scroll
  // animation. Not called every time the floaty is shown since there are
  // instances where scrolling should be allowed when a floaty is shown.
  void PrepareFloatyToBeShown();

  // Returns true if the active fullscreen implementation is initialized.
  bool IsFullscreenInitialized();

  // Resets the fullscreen disabler. Needs to be called each time
  // PrepareFloatyToBeShown() is called or the floaty may permanently disable
  // fullscreen mode. Called when the floaty is dismissed or collapsed.
  void ResetFullscreenDisabler();

  // Creates a `GeminiPageContext` for the current web state.
  GeminiPageContext* CreateGeminiPageContext(
      ios::provider::GeminiPageContextComputationState computation_state,
      std::unique_ptr<optimization_guide::proto::PageContext>
          page_context_proto);

  // Updates the presented source, if any, of the active tab helper.
  void UpdateActiveTabHelperWithPresentedSource(
      gemini::FloatyUpdateSource source,
      bool is_presented);

  // Returns true if the floaty has active hiding sources.
  bool DoesFloatyHaveActiveHidingSources() const;

  // Returns true if the floaty is only hidden by the keyboard.
  bool IsOnlyHiddenByKeyboard() const;

  // Returns true if the source expects the floaty to re-show after hiding it.
  // New sources must be added to the switch statement depending on if we
  // expect the source to re-show the floaty after hiding it.
  bool ShouldSourceReshowFloaty(gemini::FloatyUpdateSource source) const;

  // Called when keyboard state changes.
  void OnKeyboardStateChanged(bool is_visible);

  // Called for the fullscreen update animation.
  void FullscreenProgressUpdatedForAnimation();

  // Configures Gemini for the authenticated user.
  void ConfigureGemini();

  // Called when the page content sharing preference changes.
  void OnPageContentPrefChanged();

  // The gateway for bridging internal protocols.
  __strong id<BWGGatewayProtocol> bwg_gateway_ = nullptr;

  /// TODO(crbug.com/491093929): Rename the below classes to move away from the
  /// `-Handler` naming scheme used by Chromium Objective-C command protocols.
  // Handler for opening links from Gemini.
  __strong GeminiLinkOpeningHandler* gemini_link_opening_handler_ = nullptr;

  // Handler for PageState changes.
  __strong GeminiPageStateChangeHandler* gemini_page_state_change_handler_ =
      nullptr;

  // Handler for the Gemini sessions.
  __strong GeminiSessionHandler* bwg_session_handler_ = nullptr;

  // Handler for Gemini camera.
  __strong GeminiCameraHandler* gemini_camera_handler_ = nullptr;

  // Handler for Gemini suggestion chips.
  __strong GeminiSuggestionHandler* gemini_suggestion_handler_ = nullptr;

  // Handler for Gemini actor.
  __strong GeminiActuationHandler* gemini_actuation_handler_ = nullptr;

  // Delegate implementation for BWGSessionHandler.
  __strong GeminiViewStateChangeHandler* gemini_view_state_handler_ = nullptr;

  // Reference to fullscreen controller. Used to observe fullscreen progress
  // updates related to the Gemini overlay for the legacy fullscreen
  // implementation.
  raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;

  // IdentityManager associated with the Browser's profile.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Observers for keyboard events.
  id keyboard_show_observer_ = nil;
  id keyboard_hide_observer_ = nil;

  // Observer for scene state activation changes.
  __strong GeminiSceneStateObserver* scene_state_observer_ = nil;

  // Observer for scroll events.
  __strong GeminiScrollObserver* scroll_observer_ = nullptr;

  // Whether the keyboard is currently visible.
  bool is_keyboard_visible_ = false;

  // Set of sources currently hiding the floaty. If this set is not empty, the
  // floaty is considered temporarily hidden.
  std::set<gemini::FloatyUpdateSource> active_hiding_sources_;

  // Used to track the last shown view state of an invoked floaty. Used to show
  // a hidden floaty with the previous view state.
  ios::provider::GeminiViewState last_shown_view_state_ =
      ios::provider::GeminiViewState::kUnknown;

  // Whether the floaty is currently invoked.
  bool is_floaty_invoked_ = false;

  // Whether the floaty is temporarily hidden. Used to hide the floaty without
  // triggering logic related to ending floaty persistence.
  bool is_floaty_temporarily_hidden_ = false;

  // Records when the floaty was last hidden. Prevents the floaty from
  // reappearing too soon, particularly after a
  // `HideFloatyIfInvoked()` call during parent/child view
  // transitions.
  base::TimeTicks floaty_hidden_timestamp_;

  // Tracks the elapsed time a floaty is minimized until it's expanded. If the
  // floaty is expanded, the time is reset to null.
  base::TimeTicks elapsed_minimized_floaty_time_;

  // Registrar for pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  // Timer to force page context generation if page load takes too long.
  base::OneShotTimer page_context_timeout_timer_;

  // Scoped fullscreen disabler.
  std::unique_ptr<ScopedFullscreenDisabler> fullscreen_disabler_;

  // Scoped fullscreen observervation.
  base::ScopedObservation<FullscreenBrowserAgent,
                          FullscreenBrowserAgentObserver>
      fullscreen_observation_{this};

  // Timer to reset the fullscreen disabler. Re-enabling fullscreen should be
  // handled in floaty interaction logic such as the floaty being collapsed or
  // dismissed. For any reason, if an exit point doesn't re-enable fullscreen,
  // this timer will reset the fullscreen disabler after a short delay.
  base::OneShotTimer fullscreen_disabler_timer_;

  // Whether the floaty is hidden by the keyboard.
  bool is_hidden_by_keyboard_ = false;

  // The last known availability of Gemini for the active web state.
  bool last_known_gemini_availability_ = false;

  // Updates the Gemini availability and notifies observers if it changed.
  void UpdateGeminiAvailability();

  // Weak pointer factory.
  // Observers for GeminiBrowserAgent.
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<GeminiBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
