// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_

#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class WebFrame;
class WebState;
}

// A Tab Helper that inherits from the WebStateObserver in order to
// notify the WebPerformanceMetricsJavaScriptFeature that a web page
// navigation event has occurred and signals for the feature to log
// the metrics it has caputered into UMA.
class WebPerformanceMetricsTabHelper
    : public web::WebStateObserver,
      public web::WebFramesManager::Observer,
      public web::WebStateUserData<WebPerformanceMetricsTabHelper> {
 public:
  WebPerformanceMetricsTabHelper(const WebPerformanceMetricsTabHelper&) =
      delete;
  WebPerformanceMetricsTabHelper& operator=(
      const WebPerformanceMetricsTabHelper&) = delete;

  ~WebPerformanceMetricsTabHelper() override;

  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when the aggregate absolute First Contentful Paint time is
    // updated for the WebState. `absolute_first_contentful_paint` is measured
    // in milliseconds since the Unix epoch.
    virtual void OnFirstContentfulPaint(
        WebPerformanceMetricsTabHelper* tab_helper,
        double absolute_first_contentful_paint) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the absolute first contentful paint time aggregated across iframes.
  double GetAggregateAbsoluteFirstContentfulPaint() const;

  // Sets the absolute first contentful paint time.
  void SetAggregateAbsoluteFirstContentfulPaint(
      double absolute_first_contentful_paint);

  // If the web page has logged its First Input Delay, the function
  // returns `true` otherwise it returns `false`
  bool GetFirstInputDelayLoggingStatus() const;

  // Returns whether the WebState has been hidden at any point since the start
  // of the most recent navigation.
  bool HasBeenHiddenSinceNavigationStarted() const;

  // Sets the boolean variable that indicates whether the First Input Delay
  // has been logged in UMA for the current web page.
  void SetFirstInputDelayLoggingStatus(bool first_input_delay_logging_status);

  // Sets Interaction to Next Paint timing data reported for a frame.
  void SetFrameInteractionData(
      const std::string& frame_id,
      const std::vector<base::TimeDelta>& longest_durations,
      int interaction_count,
      bool is_main_frame);

  // Flushes the Interaction to Next Paint metrics to UMA for all pages that
  // have pending data. Must only be called when no more messages can arrive
  // for this WebState.
  void FlushInteractionToNextPaintMetrics();

 private:
  friend class web::WebStateUserData<WebPerformanceMetricsTabHelper>;

  // Interaction data reported by each frame of a page, keyed by frame id.
  using PageInteractions =
      std::map<std::string, web_performance_metrics::FrameInteractionData>;

  explicit WebPerformanceMetricsTabHelper(web::WebState* web_state);

  // WebStateObserver::
  void WebStateDestroyed(web::WebState* web_state) override;

  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WasHidden(web::WebState* web_state) override;

  // WebFramesManager::Observer::
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // Logs the Interaction to Next Paint metrics of a single page to UMA.
  void LogPageInteractionToNextPaint(const PageInteractions& page_interactions);

  // Assigns `frame_id` to the current page.
  void AddFrameToCurrentPage(std::string frame_id);

  // Manages the tab helper's connection to the WebState
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Observes frames in the feature's content world so frame IDs match those
  // reported by its script.
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      web_frames_manager_observation_{this};

  // Stores the earliest absolute First Contentful Paint across a web page's
  // main and subframes.
  double aggregate_absolute_first_contentful_paint_ =
      std::numeric_limits<double>::max();

  // Stores whether the First Input Delay has been logged to UMA for the
  // current web page
  bool first_input_delay_has_been_logged_ = false;

  // Stores whether the WebState has been hidden at any point since the most
  // recent navigation started.
  bool has_been_hidden_since_navigation_started_ = false;

  base::ObserverList<Observer> observers_;

  // Buffers interaction data for the current and previous pages to attribute
  // late messages arriving after cross-site navigations commit.
  PageInteractions current_page_interactions_;
  PageInteractions previous_page_interactions_;

  // Frame IDs for the current and previous pages. Messages from other frames
  // are dropped.
  std::set<std::string> current_frame_ids_;
  std::set<std::string> previous_frame_ids_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_
