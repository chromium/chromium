// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/scoped_observation.h"
#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_evaluator.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

class ProfileIOS;

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace contextual_cueing {

class ContextualCueingCapTrackerService;

// Tab helper that orchestrates contextual cueing classification for a WebState.
// It requests page classification from OnDevicePageClassificationService and
// evaluates page eligibility and category confidence against frequency limits
// to request and present contextual cues via Model Execution Service.
class ContextualCueingTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualCueingTabHelper> {
 public:
  struct BackgroundTabContext {
    GURL url;
    std::string title;
  };

  // Delegate interface to provide surrounding context (such as background tabs)
  // without coupling ContextualCueingTabHelper directly to Browser or
  // WebStateList UI container objects.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns a list of background tab contexts eligible to be included in
    // contextual cue requests.
    virtual std::vector<BackgroundTabContext> GetEligibleBackgroundTabs(
        web::WebState* active_web_state,
        size_t max_tabs) = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPageClassificationCompleted(
        ContextualCueingTabHelper* tab_helper,
        const std::optional<std::vector<page_content_annotations::Category>>&
            categories) {}
    virtual void OnContextualCueReceived(
        ContextualCueingTabHelper* tab_helper,
        const std::optional<optimization_guide::proto::ContextualCue>& cue) {}
    virtual void OnContextualCueInvalidated(
        ContextualCueingTabHelper* tab_helper) {}
  };

  ~ContextualCueingTabHelper() override;

  ContextualCueingTabHelper(const ContextualCueingTabHelper&) = delete;
  ContextualCueingTabHelper& operator=(const ContextualCueingTabHelper&) =
      delete;

  // Sets the delegate for providing surrounding context.
  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the latest categories for the current committed page, or
  // std::nullopt if classification has not completed.
  const std::optional<std::vector<page_content_annotations::Category>>&
  GetCategories() const;

  // Returns the contextual cue for the current committed page, or std::nullopt
  // if no cue is available.
  const std::optional<optimization_guide::proto::ContextualCue>&
  GetContextualCue() const;

  // Records that a contextual cue was shown to the user.
  void RecordCueShown();

  // Records that a contextual cue was explicitly dismissed by the user.
  void RecordCueDismissed();

  // Records that a contextual cue was clicked by the user.
  void RecordCueClicked();

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<ContextualCueingTabHelper>;
  friend class ContextualCueingTabHelperTest;

  explicit ContextualCueingTabHelper(web::WebState* web_state);

  // Initiates classification for the current page.
  void StartClassification();

  // Cancels any in-flight classification request.
  void CancelClassification();

  // Callback invoked when OnDevicePageClassificationService finishes.
  void OnPageClassified(
      const GURL& expected_url,
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories);

  // Initiates a request to the Model Execution Service for contextual cues.
  void InitiateModelExecutionRequest(const GURL& expected_url);

  // Callback invoked when the Model Execution Service returns a response.
  void OnModelExecutionResponseReceived(
      const GURL& expected_url,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Updates `cue_` and notifies observers.
  void NotifyContextualCueReceived(
      std::optional<optimization_guide::proto::ContextualCue> cue);

  // Checks if history sync is enabled.
  bool IsHistorySyncEnabled(ProfileIOS* profile);

  // Checks if the user is eligible for Gemini.
  bool IsUserEligibleForGemini(ProfileIOS* profile);

  // Returns the CapTrackerService for the associated profile, or nullptr.
  ContextualCueingCapTrackerService* GetCapTrackerService() const;

  raw_ptr<web::WebState> web_state_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;
  GURL current_url_;

  std::optional<std::vector<page_content_annotations::Category>> categories_;
  std::optional<optimization_guide::proto::ContextualCue> cue_;

  std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<ContextualCueingTabHelper> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_
