// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "base/scoped_observation.h"
#import "components/page_content_annotations/core/page_content_annotation_type.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_evaluator.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

class ProfileIOS;

namespace contextual_cueing {

class ContextualCueingCapTrackerService;

// Tab helper that orchestrates contextual cueing classification for a WebState.
// It requests page classification from OnDevicePageClassificationService and
// evaluates page eligibility and category confidence against frequency limits.
class ContextualCueingTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualCueingTabHelper> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPageClassificationCompleted(
        web::WebState* web_state,
        const std::optional<std::vector<page_content_annotations::Category>>&
            categories) {}
    virtual void OnContextualCueInvalidated(web::WebState* web_state) {}
  };

  ~ContextualCueingTabHelper() override;

  ContextualCueingTabHelper(const ContextualCueingTabHelper&) = delete;
  ContextualCueingTabHelper& operator=(const ContextualCueingTabHelper&) =
      delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the latest categories for the current committed page, or
  // std::nullopt if classification has not completed.
  const std::optional<std::vector<page_content_annotations::Category>>&
  GetCategories() const;

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

  // Checks if history sync is enabled.
  bool IsHistorySyncEnabled(ProfileIOS* profile);

  // Checks if the user is eligible for Gemini.
  bool IsUserEligibleForGemini(ProfileIOS* profile);

  // Returns the CapTrackerService for the associated profile, or nullptr.
  ContextualCueingCapTrackerService* GetCapTrackerService() const;

  raw_ptr<web::WebState> web_state_ = nullptr;
  GURL current_url_;

  std::optional<std::vector<page_content_annotations::Category>> categories_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<ContextualCueingTabHelper> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_
