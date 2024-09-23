// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

enum class ContextualPanelItemType;
class ContextualPanelModel;
struct ContextualPanelItemConfiguration;
class ContextualPanelTabHelperObserver;
@protocol ContextualSheetCommands;

// Tab helper controlling the Contextual Panel feature for a given tab.
class ContextualPanelTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualPanelTabHelper> {
 public:
  ContextualPanelTabHelper(const ContextualPanelTabHelper&) = delete;
  ContextualPanelTabHelper& operator=(const ContextualPanelTabHelper&) = delete;

  ~ContextualPanelTabHelper() override;

  // Helper class to hold all entrypoint metric data.
  struct EntrypointMetricsData {
    ContextualPanelItemType entrypoint_item_type;
    std::optional<base::Time> appearance_time = base::Time::Now();
    base::TimeDelta time_visible;
    bool largeEntrypointWasShown = false;
    bool iphWasShown = false;
    bool entrypoint_tap_metrics_fired = false;
    bool entrypoint_regular_display_metrics_fired = false;
    bool entrypoint_loud_display_metrics_fired = false;
  };

  // Adds and removes observers for contextual panel actions. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  virtual void AddObserver(ContextualPanelTabHelperObserver* observer);
  virtual void RemoveObserver(ContextualPanelTabHelperObserver* observer);

  // Whether there exists at least one finalized Contextual Panel model config
  // currently available in the cached list of sorted configs. This will be
  // false before all the models have returned a response or timed out.
  bool HasCachedConfigsAvailable();

  // Returns a list of the finalized Contextual Panel model configs
  // currently available in the cached list of sorted configs.
  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
  GetCurrentCachedConfigurations();

  // Gets the first config in the cached list of sorted Contextual Panel model
  // configs.
  virtual base::WeakPtr<ContextualPanelItemConfiguration>
  GetFirstCachedConfig();

  // Set the contextual sheet handler, used to display the contextual sheet UI.
  void SetContextualSheetHandler(id<ContextualSheetCommands> handler);

  // Getter for is_contextual_panel_currently_opened_.
  bool IsContextualPanelCurrentlyOpened();

  void OpenContextualPanel();
  void CloseContextualPanel();

  // Getter and setter for
  // loud_moment_entrypoint_shown_for_curent_page_navigation_.
  bool WasLoudMomentEntrypointShown();
  void SetLoudMomentEntrypointShown(bool shown);

  // Getter and setter for metrics_data_;
  std::optional<EntrypointMetricsData>& GetMetricsData();
  void SetMetricsData(EntrypointMetricsData data);

  // Returns whether the given navigation should cause the panel's data to be
  // updated.
  bool ShouldRefreshData(web::WebState* web_state,
                         web::NavigationContext* navigation_context);

  // WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;

 protected:
  // Protected to allow test overriding.
  ContextualPanelTabHelper(
      web::WebState* web_state,
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models);

 private:
  friend class web::WebStateUserData<ContextualPanelTabHelper>;

  // Helper struct to store responses received from individual models.
  struct ModelResponse {
    bool completed = false;
    std::unique_ptr<ContextualPanelItemConfiguration> configuration = nullptr;

    // Constructs a non-complete response.
    ModelResponse();

    ModelResponse(const ModelResponse&) = delete;
    ModelResponse& operator=(const ModelResponse&) = delete;
    ModelResponse& operator=(ModelResponse&& other) = default;

    // Constructs a completed response with the provided configuration
    explicit ModelResponse(
        std::unique_ptr<ContextualPanelItemConfiguration>&& configuration);
    ~ModelResponse();
  };

  // Callback for when the given model has finished fetching its data.
  void ModelCallbackReceived(
      ContextualPanelItemType item_type,
      std::unique_ptr<ContextualPanelItemConfiguration> configuration);

  // Query all the individual models for their data.
  void QueryModels();

  // Do any necessary work after all requests are completed or time out.
  void AllRequestsFinished();

  // Fire any metrics that should fire when all requests are finished.
  void FireRequestsFinishedMetrics();

  WEB_STATE_USER_DATA_KEY_DECL();

  // Whether the Contextual Panel is currently opened for the current tab.
  bool is_contextual_panel_currently_opened_ = false;

  // Whether a loud moment (large entrypoint or IPH) for the Contextual Panel
  // entrypoint has been shown for the current navigation.
  bool loud_moment_entrypoint_shown_for_curent_page_navigation_ = false;

  // Stores the previous URL to help decide whether this navigation is to
  // a new page.
  GURL previous_url_;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Stores metric data for the entrypoint. The data is stored here because
  // there is one tab helper per tab, while the entrypoint classes are one per
  // browser. The data stored here is specific to a given tab.
  std::optional<EntrypointMetricsData> metrics_data_ = std::nullopt;

  // Map of the models this tab helper should query for possible panels.
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models_;

  // The time the current request began.
  base::Time request_start_time_;

  // Holds the responses currently being returned.
  std::map<ContextualPanelItemType, ModelResponse> responses_;

  // Holds the current finalized and sorted list of configurations passed to
  // observers when all requests have completed. Not the source of truth of
  // panel model responses, simply a cached list of their configs.
  std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>
      sorted_weak_configurations_;

  // Command handler for contextual sheet commands.
  __weak id<ContextualSheetCommands> contextual_sheet_handler_ = nil;

  // List of observers to be notified when the Contextual Panel gets new data.
  base::ObserverList<ContextualPanelTabHelperObserver, true> observers_;

  // Scoped observation for WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<ContextualPanelTabHelper> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_TAB_HELPER_H_
