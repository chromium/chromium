// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import <optional>
#import <string>
#import <vector>

#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol GeminiCommands;
@class GeminiPageContext;
@protocol HelpCommands;
enum class IOSGeminiInvocationPageType;
@protocol LocationBarBadgeCommands;

@class ZeroStateSuggestion;

namespace gemini {
enum class FloatyUpdateSource;
}

namespace ai {
class ZeroStateSuggestionsService;
}

class GeminiSuggestionHandlerTest;

// Tab helper controlling Gemini and its current state for a given tab.
class GeminiTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<GeminiTabHelper> {
 public:
  GeminiTabHelper(const GeminiTabHelper&) = delete;
  GeminiTabHelper& operator=(const GeminiTabHelper&) = delete;

  ~GeminiTabHelper() override;

  // Represents the Gemini contextual eligibility state of the current URL. The
  // URL can either be ineligible, or eligible via a proactive fetch vs an
  // on-demand fetch. That depends on a few factors, notably incognito and MSBB
  // (Make Searches and Browsing Better - see
  // `//components/unified_consent/README.md` for more info).
  enum class ContextualEligibility {
    // The page has not been evaluated, or is ineligible for Gemini contextual
    // features.
    kIneligible,
    // The page is eligible, and the decision was fetched via the standard
    // proactive path (i.e. MSBB enabled).
    kEligibleViaProactiveFetch,
    // The page is eligible, and the decision was fetched via the on-demand
    // path (i.e. MSBB disabled).
    kEligibleViaOnDemandFetch,
  };

  // Forces the generation of page context immediately, bypassing any wait for
  // page load completion. Used when the page load timeout is exceeded.
  // This is no op if page has already finished loading.
  void ForcePageContextGeneration();

  // Cancels any ongoing page context generation.
  void CancelPageContextGeneration();

  // Fetches zero-state suggestions for the current WebState returning
  // ZeroStateSuggestion objects.
  void FetchZeroStateSuggestions(
      base::OnceCallback<void(NSArray<ZeroStateSuggestion*>* suggestions)>
          callback);

  // Fetches zero-state suggestions for the current WebState returning strings.
  void FetchZeroStateSuggestionsAsStrings(
      base::OnceCallback<void(NSArray<NSString*>* suggestions)> callback);

  // Deactivates the Gemini associated to this WebState.
  void DeactivateGeminiSession();

  // Returns true if the URL of last recorded interaction is not the same as the
  // current URL (ignoring URL fragments).
  bool IsLastInteractionUrlDifferent();

  // Whether Gemini should show the suggestion chips for the current Web State
  // and visible URL.
  bool ShouldShowSuggestionChips();

  // Whether Gemini is available for the current web state.
  bool IsGeminiAvailableForWebState();

  // Whether contextual entry points are allowed to be accessed for the current
  // web state.
  bool IsContextualEntryPointAllowed();

  // Returns the current type of page or WebState.
  IOSGeminiInvocationPageType GetCurrentPageType();

  // Whether Gemini Chat mode is available for the current web state.
  bool IsGeminiChatAvailableForWebState();

  // Gets the client ID for the Gemini session for the associated WebState.
  std::string GetClientId();

  // Set the Gemini commands handler, used to show/hide the Gemini UI.
  void SetGeminiHandler(id<GeminiCommands> handler);

  // Set help commands handler, for showing in-product help UI.
  void SetHelpCommandsHandler(id<HelpCommands> handler);

  // Set the location bar badge commands handler.
  void SetLocationBarBadgeCommandsHandler(id<LocationBarBadgeCommands> handler);

  // Returns whether to prevent contextual panel entrypoint based on Gemini
  // in-product help criteria.
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

  // Requests the latest page context. Resolves immediately if the page is
  // restricted to surface-level metadata, or asynchronously if deep extraction
  // is required. `is_background_tab` indicates whether to bypass eligibility
  // checks related to this tab's visibility, NextIA or Gemini Live.
  void GeneratePageContext(
      base::RepeatingCallback<void(GeminiPageContext*)> callback,
      bool is_background_tab = false);

  // Returns the partial PageContext for the current WebState, including URL,
  // Title, and Favicon. `is_background_tab` indicates whether to bypass
  // eligibility checks related to this tab's visibility, NextIA or Gemini Live.
  GeminiPageContext* GetPartialPageContext(bool is_background_tab = false);

  // Returns true if a show floaty trigger should be blocked resulting in an
  // early return and the floaty remaining hidden. Used when the floaty is
  // forced to be hidden such as an overlay, alert, or banner being presented
  bool ShouldBlockFloatyFromShowing();

  // Updates the state of a `source` that `is_presented`.
  void UpdatePresentedSource(gemini::FloatyUpdateSource source,
                             bool is_presented);

  // Notifies observers of the web state that the page context changed.
  void NotifyPageContextUpdated(web::WebState* web_state);

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
  explicit GeminiTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<GeminiTabHelper>;

  // Adding GeminiTabHelperTest as a friend to facilitate validation of behavior
  // in tests.
  friend class GeminiTabHelperTest;

  // TODO(crbug.com/502249229): Refactor GeminiSuggestionHandler to use a
  // protocol or delegate to avoid needing to be a friend of GeminiTabHelper.
  friend class GeminiSuggestionHandlerTest;

  // The PageContext wrapper used to provide context about a page.
  __strong PageContextWrapper* page_context_wrapper_ = nil;

  // Populates the page context fields if the wrapper exists.
  void PopulatePageContextFields();

  // Computes the actual Gemini eligibility based on the response from
  // `OnGeminiEligibilityDecision`.
  ContextualEligibility ComputeGeminiEligibility(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata,
      const bool was_proactive_fetch_used);

  // Callback for the OptimizationGuide with the result of whether the
  // zero-state suggestions should be shown for the current URL.
  // Attempts to show the in-product help tooltip for Image Remix.
  //
  // `was_proactive_fetch_used` is true if the proactive fetch from
  // OptimizationGuide was permitted and used (i.e. MSBB is enabled).
  void OnGeminiEligibilityDecision(
      const GURL& url_without_ref,
      const bool was_proactive_fetch_used,
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

  // Removes the Gemini session from the prefs.
  void CleanupSessionFromPrefs();

  // Whether Gemini can extract the current web state's page context.
  // `is_background_tab` indicates whether to bypass eligibility checks related
  // to this tab's visibility, NextIA or Gemini Live.
  bool CanExtractPageContextForGemini(bool is_background_tab = false);

  // TODO(crbug.com/516531773): Find solution for repetitive helper methods.
  // Whether Gemini Live mode is currently active.
  bool IsInGeminiLiveMode() const;

  // Whether standard NextIA or Live mode is active.
  bool IsNextIaOrLiveMode() const;

  // Fetches the cached favicon or generates a default fallback.
  UIImage* GetFavicon();

  // Handles the asynchronous result from PageContextWrapper.
  void OnPageContextWrapperResponse(
      PageContextWrapperCallbackResponse expected_page_context);

  // Presents the in-product help tooltip bubble for Image Remix if the WebState
  // is visible and not loading, and the page and user are eligible.
  void MaybePresentImageRemixTooltip();

  // Presents the in-product help tooltip bubble for Image Remix.
  void PresentImageRemixTooltip();

  // Tracks the best-effort extraction timeout.
  base::OneShotTimer page_context_timeout_timer_;

  // Stores the consumer callback waiting for the final context object.
  base::RepeatingCallback<void(GeminiPageContext*)>
      page_context_consumer_callback_;

  // WebState this tab helper is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Commands handler for Gemini commands.
  __weak id<GeminiCommands> gemini_handler_ = nullptr;

  // Commands handler for help commands.
  __weak id<HelpCommands> help_commands_handler_ = nullptr;

  // Commands handler for location bar badge.
  __weak id<LocationBarBadgeCommands> location_bar_badge_commands_handler_ =
      nullptr;

  // The observation of the Web State.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

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

  // The service managing zero-state suggestions.
  std::unique_ptr<ai::ZeroStateSuggestionsService>
      zero_state_suggestions_service_;

  // Whether Gemini contextual features (such as zero-state suggestions and
  // Image Remix) are eligible to be shown on the current page, based on the
  // optimization guide decision.
  ContextualEligibility gemini_contextual_eligibility_ =
      ContextualEligibility::kIneligible;

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
  base::WeakPtrFactory<GeminiTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_H_
