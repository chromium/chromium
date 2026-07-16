// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import <map>
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
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/web_state_id.h"

class Browser;
class FullscreenController;
class AppBarMediatorTest;
class LocationBarBadgeMediatorTest;

namespace gemini {
enum class FloatyUpdateSource;
}  // namespace gemini

class ScopedFullscreenDisabler;
@class GeminiLinkOpeningHandler;
@class GeminiPageStateChangeHandler;
@class GeminiSessionHandler;
@class GeminiCameraHandler;
@class GeminiTabPickerHandler;
@class GeminiConsentProviderHandler;
@class GeminiPageContext;
@class GeminiViewStateChangeHandler;
@class GeminiScrollObserver;
@class GeminiSceneStateObserver;
@class GeminiSuggestionHandler;
@class GeminiActuationHandler;
@class TabGridStateObserverBridge;

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
                           public TabGridStateObserver,
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

  // Returns true if Gemini Live mode is currently active.
  bool IsInGeminiLiveMode() const;

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
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;

  // GeminiTabHelperObserver:
  void OnPageContextUpdated(web::WebState* web_state) override;
  void OnGeminiTabHelperDestroyed(GeminiTabHelper* tab_helper) override;

  // Checks if the FRE needs to be shown and start the Gemini flow
  // accordingly.
  void StartGeminiFlow(UIViewController* base_view_controller,
                       GeminiStartupState* startup_state);

  // Creates and returns the GeminiConfiguration for the active web state.
  GeminiConfiguration* CreateGeminiConfigurationForActiveWebState(
      UIViewController* base_view_controller,
      GeminiStartupState* startup_state);

  // Presents a Gemini Live microphone authorization alert or Settings prompt.
  void ShowGeminiLiveMicrophoneAlert(UIViewController* base_view_controller,
                                     void (^completion)(BOOL granted));

  // Dismisses the floaty and resets the Gemini flow.
  void DismissFloaty();

  // Called when the tab picker selection changes.
  void OnTabPickerSelectionChanged(std::set<web::WebStateID> selected_tabs);

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
      ios::provider::GeminiClientMode processing_status,
      ios::provider::GeminiDormantReason dormant_reason) override;
  void CollapseFloatyIfInvoked() override;
  void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) override;
  void OnLiveButtonTapped() override;
  void OnGeminiLiveUserDidBargeIn() override;
  void OnGeminiLiveUserDidPressStopButton() override;
  void OnModeChanged(ios::provider::GeminiViewMode mode) override;
  void OnGeminiUIDidAppear() override;

  // Called when the scene activation level changes.
  void OnSceneActivationLevelChanged(SceneActivationLevel level);

  // Called when the scene is about to enter Incognito mode.
  void OnWillEnterIncognito();

  // Called when trait collection is updated.
  void UpdateForTraitCollection(UITraitCollection* traitCollection);

  // Dismisses Gemini from all other windows and executes the completion block.
  void DismissGeminiFromOtherWindows(base::OnceClosure completion);

  // Returns the entry point that triggered the current Gemini flow.
  gemini::EntryPoint GetEntryPoint() const;

 private:
  explicit GeminiBrowserAgent(Browser* browser);
  friend class BrowserUserData<GeminiBrowserAgent>;
  friend class GeminiBrowserAgentTest;
  friend class AppBarMediatorTest;
  friend class LocationBarBadgeMediatorTest;

  // Fetches the full context of the active page and feeds it to Gemini.
  void RequestPageContextGeneration();

  // Updates the active page context and passes it to the Gemini provider, along
  // with any shared tabs.
  void PropagatePageContextToProvider(GeminiPageContext* active_page_context);

  // Updates the floaty with partial page context synchronously if the tab
  // helper is available.
  void UpdateFloatyWithPartialPageContext();

  // Returns the array of page contexts for all currently attached
  // shared tabs.
  NSArray<GeminiPageContext*>* GetSharedTabs() const;

  // Starts the Gemini session (prepares context and shows overlay).
  void PresentFloaty(UIViewController* base_view_controller,
                     GeminiStartupState* startup_state);

  // Creates the configuration for the Gemini overlay.
  GeminiConfiguration* CreateGeminiConfiguration(
      UIViewController* base_view_controller,
      GeminiStartupState* startup_state,
      web::WebState* web_state,
      GeminiPageContext* page_context);

  // Adjusts the configuration around the Gemini page context based on user
  // prefs.
  void ApplyUserPrefsToPageContext(GeminiPageContext* gemini_page_context);

  // Records the page type when Gemini is invoked.
  void RecordInvocationPageType();

  // Sets the UI command handlers on the session handler. This cannot be called
  // in the constructor because some objects fail the protocol conformance test
  // at that time.
  void SetSessionCommandHandlers();

  // Helper to get the GeminiTabHelper for the active web state if it matches
  // the provided web state.
  GeminiTabHelper* GetActiveTabHelper(web::WebState* web_state) const;

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

  // TabGridStateObserver:
  void WillEnterTabGrid() override;
  void WillExitTabGrid() override;

  // Returns true if the user has completed the FRE.
  bool HasCompletedFirstRun();

  // Shows a snackbar message informing the user that sign-in is required.
  void ShowSignInRequiredSnackbar(gemini::EntryPoint entry_point);

  // Shows a snackbar message with the given message ID.
  void ShowLiveSessionDormantSnackbar(int message_id);

  // Sets whether the dormant snackbar is showing.
  void SetIsShowingLiveSessionDormantSnackbar(bool showing);

  // Updates the Gemini Live leading icon visibility in the location bar.
  void UpdateGeminiLiveIconVisibility(bool animated = true);

  // Returns the floaty offset based on current fullscreen progress.
  CGFloat GetFloatyOffset();

  // Returns the floaty offset assuming the toolbars are fully expanded.
  CGFloat GetFullyExpandedFloatyOffset();

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

  // Switches the view mode to Floaty (i.e., chat) mode if the current page is
  // eligible, or dismisses the floaty if ineligible.
  void SwitchToChatModeOrDismiss(bool animated);

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

  // Updates the presented source, if any, of the active tab helper.
  void UpdateActiveTabHelperWithPresentedSource(
      gemini::FloatyUpdateSource source,
      bool is_presented);

  // Returns true if the omnibox is focused.
  bool IsOmniboxFocused() const;

  // Returns true if the keyboard update should be ignored.
  bool ShouldIgnoreKeyboardUpdate() const;

  // Recalculates and updates the Gemini Live mode UI elements.
  void UpdateLiveModeUI();

  // Updates the Gemini Live mode UI and page context. Returns true if page
  // context update was performed.
  bool UpdateLiveModeUIAndMaybeContext();

  // Returns true if the update from `source` should be ignored because the Live
  // session dormant snackbar is active.
  bool ShouldIgnoreUpdateForDormantSnackbar(
      gemini::FloatyUpdateSource source) const;

  // Called when keyboard state changes.
  void OnKeyboardStateChanged(bool is_visible);

  // Handles an generated page context by updating the floaty.
  void OnPageContextGenerated(GeminiPageContext* gemini_page_context);

  // Called for the fullscreen update animation.
  void FullscreenProgressUpdatedForAnimation();

  // Configures Gemini for the authenticated user.
  void ConfigureGemini();

  // Called when the page content sharing preference changes.
  void OnPageContentPrefChanged();

  // Called when the microphone preference changes.
  void OnMicrophonePrefChanged();

  // Clears the set of attached tabs if it doesn't include the active web
  // state.
  void UpdateAttachedTabsForActiveWebState(web::WebState* active_web_state);

  // Creates a partial page context synchronously for a web state.
  GeminiPageContext* CreatePartialPageContext(web::WebState* web_state);

  // Removes a tab from selected tabs and propagates attached tabs to Gemini.
  void DetachTabWithID(NSString* tab_id);

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

  // Handler for Gemini tab picker.
  __strong GeminiTabPickerHandler* gemini_tab_picker_handler_ = nullptr;

  // Handler for Gemini consent provider.
  __strong GeminiConsentProviderHandler* gemini_consent_provider_handler_ =
      nullptr;

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

  // Bridge to observe TabGridState.
  __strong TabGridStateObserverBridge* tab_grid_state_observer_bridge_ = nil;

  // Observer for scroll events.
  __strong GeminiScrollObserver* scroll_observer_ = nullptr;

  // Whether the keyboard is currently visible.
  bool is_keyboard_visible_ = false;

  // The active and shared tabs currently attached to the floaty, represented by
  // a mapping of the tab's WebStateID to its page context.
  std::map<web::WebStateID, __strong GeminiPageContext*> attached_tabs_;

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

  // Scoped fullscreen disabler.
  std::unique_ptr<ScopedFullscreenDisabler> fullscreen_disabler_;

  // Scoped fullscreen observervation.
  base::ScopedObservation<FullscreenBrowserAgent,
                          FullscreenBrowserAgentObserver>
      fullscreen_observation_{this};

  // Whether the floaty is hidden by the keyboard.
  bool is_hidden_by_keyboard_ = false;

  // Start time of the current Gemini Live response. Used for barge-in latency
  // and response duration.
  base::TimeTicks live_response_start_time_;

  // Start time of the Gemini Live thinking state. Used for response latency.
  base::TimeTicks live_thinking_start_time_;

  // The number of turns in the current Gemini Live session.
  int live_turn_count_ = 0;

  // The start time of the current Gemini Live session segment.
  base::TimeTicks live_session_start_time_;

  // The accumulated duration of all Gemini Live segments within a single
  // overall interaction.
  base::TimeDelta live_session_accumulated_duration_;

  // Logs Gemini live related metrics and resets values if needed.
  void LogLiveSessionMetrics(bool floaty_dismissed = false);

  // The current processing status of the Gemini client.
  ios::provider::GeminiClientMode processing_status_ =
      ios::provider::GeminiClientMode::kUnknown;

  // The last known availability of Gemini for the active web state.
  bool last_known_gemini_availability_ = false;

  // Updates the Gemini availability and notifies observers if it changed.
  void UpdateGeminiAvailability();

  // Handles the client transitioning to a dormant status.
  void HandleDormantStatus(ios::provider::GeminiDormantReason dormant_reason);

  // Logs state transition events for Gemini Live metrics.
  void LogLiveStatusTransition(ios::provider::GeminiClientMode old_status,
                               ios::provider::GeminiClientMode new_status);

  // Whether we are currently displaying the Live session dormant snackbar.
  bool is_showing_live_session_dormant_snackbar_ = false;

  // Track if we have triggered feature engagement for Gemini Live IPH or New
  // Badge.
  bool has_triggered_gemini_live_iph_ = false;
  bool has_triggered_gemini_live_new_badge_ = false;

  // The entry point that triggered the current Gemini flow.
  gemini::EntryPoint entry_point_ = gemini::EntryPoint::Unknown;

  // Weak pointer factory.
  // Observers for GeminiBrowserAgent.
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<GeminiBrowserAgent> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_AGENT_H_
