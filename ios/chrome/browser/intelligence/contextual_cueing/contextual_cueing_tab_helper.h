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
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

namespace contextual_cueing {

// Tab helper that orchestrates contextual cueing classification for a WebState.
// It stores page categories and word count for downstream contextual cue
// evaluations and notifies observers when classification completes.
class ContextualCueingTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<ContextualCueingTabHelper> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPageClassificationCompleted(
        web::WebState* web_state,
        const std::optional<std::vector<page_content_annotations::Category>>&
            categories,
        size_t word_count) {}
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

  // Returns the word count of the extracted page text for the current page.
  size_t GetExtractedWordCount() const;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<ContextualCueingTabHelper>;
  friend class ContextualCueingTabHelperTest;

  explicit ContextualCueingTabHelper(web::WebState* web_state);

  // Initiates classification for the current page.
  void StartClassification();

  // Callback invoked when page classification finishes.
  void OnPageClassified(
      const GURL& expected_url,
      const std::optional<std::vector<page_content_annotations::Category>>&
          categories,
      size_t word_count);

  raw_ptr<web::WebState> web_state_ = nullptr;
  GURL current_url_;

  std::optional<std::vector<page_content_annotations::Category>> categories_;
  size_t word_count_ = 0;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<ContextualCueingTabHelper> weak_ptr_factory_{this};
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_TAB_HELPER_H_
