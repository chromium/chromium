// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/mojom/zero_state_suggestions_service.mojom.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol BWGCommands;
@protocol HelpCommands;
@protocol LocationBarBadgeCommands;
@class GeminiPageContext;

namespace gemini {
enum class FloatyUpdateSource;
}

// Tab helper controlling the BWG feature and its current state for a given tab.
class BwgTabHelper : public web::WebStateObserver,
                     public web::WebStateUserData<BwgTabHelper> {
 public:
  BwgTabHelper(const BwgTabHelper&) = delete;
  BwgTabHelper& operator=(const BwgTabHelper&) = delete;

  ~BwgTabHelper() override;

  // Set up generation of page Context and the callback to be run when the page
  // context is ready.
  void SetupPageContextGeneration(
      base::RepeatingCallback<void(PageContextWrapperCallbackResponse)>
          callback);

  // Forces the generation of page context immediately, bypassing any wait for
  // page load completion. Used when the page load timeout is exceeded.
  // This is no op if page has already finished loading.
  void ForcePageContextGeneration();

  // Executes the zero-state suggestions flow.
  void ExecuteZeroStateSuggestions(
      base::OnceCallback<void(NSArray<NSString*>* suggestions)> callback);

  // Sets the state of `is_bwg_ui_showing_`.
  void SetBwgUiShowing(bool showing);

  // Gets the state of `is_bwg_session_active_in_background_`.
  bool GetIsBwgSessionActiveInBackground();

  // Deactivates the BWG associated to this WebState.
  void DeactivateBWGSession();

  // Returns true if the URL of last recorded interaction is not the same as the
  // current URL (ignoring URL fragments).
  bool IsLastInteractionUrlDifferent();

  // Whether BWG should show the suggestion chips for the current Web State and
  // visible URL.
  bool ShouldShowSuggestionChips();

  // Creates, or updates, a new BWG session in storage with the current
  // timestamp, server ID and URL for the associated WebState.
  void CreateOrUpdateBwgSessionInStorage(std::string server_id);

  // Removes the associated WebState's session from storage.
  void DeleteBwgSessionInStorage();

  // Whether BWG is available for the current web state.
  bool IsBwgAvailableForWebState();

  // Prepares the WebState for the BWG FRE (first run experience) backgrounding.
  // Takes a fullscreen screenshot and sets the session to active.
  void PrepareBwgFreBackgrounding();

  // Gets the client and server IDs for the BWG session for the associated
  // WebState. server ID is optional because it may not be found or is expired.
  std::string GetClientId();
  std::optional<std::string> GetServerId();

  // Set the BWG commands handler, used to show/hide the BWG UI.
  void SetBwgCommandsHandler(id<BWGCommands> handler);

  // Set help commands handler, for showing in-product help UI.
  void SetHelpCommandsHandler(id<HelpCommands> handler);

  // Set the location bar badge commands handler.
  void SetLocationBarBadgeCommandsHandler(id<LocationBarBadgeCommands> handler);

  // Sets the state of `is_first_run`.
  void SetIsFirstRun(bool is_first_run);

  // Gets the state of `is_first_run`.
  bool GetIsFirstRun();

  // Returns whether to prevent contextual panel entrypoint based on Gemini IPH
  // criteria.
  bool ShouldPreventContextualPanelEntryPoint();

  // Adds an observer.
  void AddObserver(GeminiTabHelperObserver* observer);

  // Removes an observer.
  void RemoveObserver(GeminiTabHelperObserver* observer);

  // Whether the observer exists in the observer list.
  bool HasObserver(GeminiTabHelperObserver* observer);

  // Setter for `prevent_contextual_panel_entry_point_`.
  void SetPreventContextualPanelEntryPoint(bool should_prevent);

  // Sets a callback to be run when the page has finished loading.
  void SetPageLoadedCallback(base::RepeatingClosure callback);

  // Returns the partial PageContext for the current WebState, including URL,
  // Title, and Favicon.
  GeminiPageContext* GetPartialPageContext();

  // Returns true if a show floaty trigger should be blocked resulting in an
  // early return and the floaty remaining hidden. Used when the floaty is
  // forced to be hidden such as an overlay, alert, or banner being presented
  bool ShouldBlockFloatyFromShowing();

  // Updates the state of a `source` that `is_presented`.
  void UpdatePresentedSource(gemini::FloatyUpdateSource source,
                             bool is_presented);

  // WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void TitleWasSet(web::WebState* web_state) override;
  void FaviconUrlUpdated(
      web::WebState* web_state,
      const std::vector<web::FaviconURL>& candidates) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  struct ZeroStateSuggestions;

  explicit BwgTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<BwgTabHelper>;

  // The PageContext wrapper used to provide context about a page.
  __strong PageContextWrapper* page_context_wrapper_ = nil;

  // Clears the zero-state suggestions and resets the service.
  void ClearZeroStateSuggestions();

  // Notifies observers of the web state that the page context changed.
  void NotifyPageContextUpdated(web::WebState* web_state);

  // Populates the page context fields if the wrapper exists.
  void PopulatePageContextFields();

  // Computes the actual Gemini eligibility based on the response from
  // `OnGeminiEligibilityDecision`.
  bool ComputeGeminiEligibility(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Callback for the OptimizationGuide with the result of whether the
  // zero-state suggestions should be shown for the current URL.
  // Shows IPH for image remix if the user has enabled metadata requests (MSBB).
  void OnGeminiEligibilityDecision(
      const GURL& url_without_ref,
      bool user_enabled_request_metadata,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Callback for the OptimizationGuide with the result to the on-demand call.
  void OnGeminiEligibilityOnDemandDecision(
      const GURL& url_without_ref,
      const base::flat_map<
          optimization_guide::proto::OptimizationType,
          optimization_guide::OptimizationGuideDecisionWithMetadata>&
          decisions);

  // Callback from OptimizationGuide metadata request.
  void OnCanApplyContextualCueingDecision(
      const GURL& main_frame_url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Adding BwgTabHelperTest as a friend to facilitate validation of behavior in
  // tests.
  friend class BwgTabHelperTest;

  // Creates a new BWG session in the prefs, or updates an existing one, with
  // the current timestamp.
  void CreateOrUpdateSessionInPrefs(std::string client_id,
                                    std::string server_id);

  // Removes the BWG session from the prefs.
  void CleanupSessionFromPrefs();

  // Updates the snapshot in storage for the associated Web State. If a snapshot
  // is cached (cropped fullscreen screenshot), use it to update the storage,
  // otherwise generate one normally for the content area.
  void UpdateWebStateSnapshotInStorage();


  // Parses the response of a zero state suggestions execution.
  void ParseSuggestionsResponse(
      base::OnceCallback<void(NSArray<NSString*>*)> callback,
      ai::mojom::ZeroStateSuggestionsResponseResultPtr result);

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Whether the BWG UI is currently showing.
  bool is_bwg_ui_showing_ = false;

  // The cached WebState snapshot. Written to disk when the WebState is hidden.
  // If non-nil, stores a cropped fullscreen snapshot which includes the BWG UI.
  __strong UIImage* cached_snapshot_;

  // Whether the BWG session is currently active in the "background", i.e. the
  // UI is not present since another  WebState is being shown, but the current
  // WebState has an active session.
  bool is_bwg_session_active_in_background_ = false;

  // Commands handler for BWG commands.
  __weak id<BWGCommands> bwg_commands_handler_ = nullptr;

  // Commands handler for help commands.
  __weak id<HelpCommands> help_commands_handler_ = nullptr;

  // Commands handler for location bar badge.
  __weak id<LocationBarBadgeCommands> location_bar_badge_commands_handler_ =
      nullptr;

  // The observation of the Web State.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Whether this is a first run experience.
  bool is_first_run_ = false;

  // The URL from the previous successful main frame navigation. This will be
  // empty if this is the first navigation for this tab or post-restart.
  GURL previous_main_frame_url_;

  // The contextual cueing metadata for the latest page loaded.
  std::optional<optimization_guide::proto::GlicContextualCueingMetadata>
      latest_load_contextual_cueing_metadata_;

  // The optimization guide decider for page metadata.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  // Whether to prevent contextual panel entry point.
  bool prevent_contextual_panel_entry_point_ = false;

  // The zero-state suggestions data and service for the current page.
  std::unique_ptr<ZeroStateSuggestions> zero_state_suggestions_;

  // Callback to be run when the page has finished loading.
  base::RepeatingClosure page_loaded_callback_;

  // List of observers.
  base::ObserverList<GeminiTabHelperObserver> observers_;

  // Tracking variables for semantic event checks.
  GURL current_url_;
  std::u16string current_title_;
  __strong UIImage* current_favicon_;

  // The callback to be run when the page context is ready.
  base::RepeatingCallback<void(PageContextWrapperCallbackResponse)>
      page_context_wrapper_response_ready_callback_;

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

  // Whether a snackbar is currently presented. Used to avoid showing the floaty
  // when view controllers are presented/dismissed while a snackbar is
  // presented.
  bool is_snackbar_presented_ = false;

  // Weak pointer factory.
  base::WeakPtrFactory<BwgTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
