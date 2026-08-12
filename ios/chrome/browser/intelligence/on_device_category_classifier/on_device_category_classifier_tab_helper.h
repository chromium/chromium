// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/observer_list_types.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "url/gurl.h"

namespace actor {
class PageStabilityMonitor;
}  // namespace actor

@class PageContextWrapper;

// Tab helper that orchestrates on-device category classification.
// This is the iOS counterpart to `PageContentAnnotationsWebContentsObserver`
// on Desktop and Android (which relies on `content::WebContents` and cannot be
// used on iOS).
class OnDeviceCategoryClassifierTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OnDeviceCategoryClassifierTabHelper> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked when category classification has completed for the given
    // `web_state`.
    virtual void OnCategoriesClassified(
        web::WebState* web_state,
        const std::vector<page_content_annotations::Category>& categories) {}
  };

  ~OnDeviceCategoryClassifierTabHelper() override;

  // Registers / unregisters an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the latest categories for the current committed page, or
  // std::nullopt if classification has not completed.
  const std::optional<std::vector<page_content_annotations::Category>>&
  GetCategories() const;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  // Invoked when the tab is hidden (e.g. user switched tabs). Used to cancel
  // any pending classifications.
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<OnDeviceCategoryClassifierTabHelper>;
  friend class OnDeviceCategoryClassifierTabHelperTest;

  explicit OnDeviceCategoryClassifierTabHelper(web::WebState* web_state);

  // Starts category classification for the current web state.
  void StartClassification();

  // Extracts page context and executes classification after page stability is
  // reached.
  void ExtractPageContextAndClassify();

  // Invoked when PageContext extraction completes asynchronously.
  void OnPageContextResponse(PageContextWrapperCallbackResponse response);

  // Invoked when the page inner text has been extracted asynchronously.
  void OnPageContextExtracted(const std::string& page_content,
                              const std::string& title,
                              const GURL& url);

  // Invoked asynchronously when the category classification model has finished
  // executing and returns the scores.
  void OnCategoriesClassified(
      ukm::SourceId source_id,
      const std::vector<page_content_annotations::Category>& categories);

  raw_ptr<web::WebState> web_state_ = nullptr;
  PageContextWrapper* page_context_wrapper_ = nil;
  std::unique_ptr<actor::PageStabilityMonitor> page_stability_monitor_;

  // The URL of the current page, used to ignore same-document anchor
  // navigations.
  GURL current_url_;

  // Cached categories for the current committed page, or std::nullopt if
  // classification has not completed.
  std::optional<std::vector<page_content_annotations::Category>> categories_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<OnDeviceCategoryClassifierTabHelper> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_
