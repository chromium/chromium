// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/scoped_observation.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol BWGCommands;
@protocol SnackbarCommands;

// Tab helper controlling the BWG feature and its current state for a given tab.
class BwgTabHelper : public web::WebStateObserver,
                     public web::WebStateUserData<BwgTabHelper> {
 public:
  BwgTabHelper(const BwgTabHelper&) = delete;
  BwgTabHelper& operator=(const BwgTabHelper&) = delete;

  ~BwgTabHelper() override;

  // Sets the state of `is_bwg_ui_showing_`.
  void SetBwgUiShowing(bool showing);

  // Gets the state of `is_bwg_session_active_in_background_`.
  bool GetIsBwgSessionActiveInBackground();

  // Deactivates the BWG associated to this WebState.
  void DeactivateBWGSession();

  // Whether BWG should show the zero-state input box UI for the current Web
  // State and visible URL.
  bool ShouldShowZeroState();

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

  // Set the snackbar commands handler for presenting snackbars.
  void SetSnackbarCommandsHandler(id<SnackbarCommands> handler);

  // Sets the state of `is_first_run`.
  void SetIsFirstRun(bool is_first_run);

  // Gets the state of `is_first_run`.
  bool GetIsFirstRun();

  // Returns whether to prevent contextual panel entrypoint based on Gemini IPH
  // criteria.
  bool ShouldPreventContextualPanelEntryPoint();

  // Setter for `prevent_contextual_panel_entry_point_`.
  void SetPreventContextualPanelEntryPoint(bool should_prevent);

  // WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit BwgTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<BwgTabHelper>;

  // Callback from OptimizationGuide metadata request.
  void OnOptimizationGuideDecision(
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
  void CleanupSessionFromPrefs(std::string session_id);

  // Updates the snapshot in storage for the associated Web State. If a snapshot
  // is cached (cropped fullscreen screenshot), use it to update the storage,
  // otherwise generate one normally for the content area.
  void UpdateWebStateSnapshotInStorage();

  // Gets the associated WebState's visible URL during the last interaction, if
  // present and not expired, from storage.
  std::optional<std::string> GetURLOnLastInteraction();

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
  __weak id<BWGCommands> bwg_commands_handler_ = nil;

  // Commands handler for snackbars.
  __weak id<SnackbarCommands> snackbar_commands_handler_ = nil;

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

  base::WeakPtrFactory<BwgTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_TAB_HELPER_H_
