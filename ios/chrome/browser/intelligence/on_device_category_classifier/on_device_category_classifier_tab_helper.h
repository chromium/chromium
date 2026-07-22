// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/page_content_annotations/core/page_content_annotations_common.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class PageContextWrapper;

// Tab helper that orchestrates on-device category classification.
// This is the iOS counterpart to `PageContentAnnotationsWebContentsObserver`
// on Desktop and Android (which relies on `content::WebContents` and cannot be
// used on iOS).
class OnDeviceCategoryClassifierTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OnDeviceCategoryClassifierTabHelper> {
 public:
  ~OnDeviceCategoryClassifierTabHelper() override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<OnDeviceCategoryClassifierTabHelper>;

  explicit OnDeviceCategoryClassifierTabHelper(web::WebState* web_state);

  // Starts page context extraction for the current web state.
  void StartExtraction();

  // Invoked when PageContext extraction completes asynchronously.
  void OnPageContextResponse(PageContextWrapperCallbackResponse response);

  // Invoked when the page inner text has been extracted asynchronously.
  void OnPageContextExtracted(const std::string& page_content,
                              const std::string& title,
                              const GURL& url);

  // Invoked asynchronously when the category classification model has finished
  // executing and returns the scores.
  void OnCategoriesClassified(
      const std::vector<page_content_annotations::Category>& categories);

  raw_ptr<web::WebState> web_state_ = nullptr;
  PageContextWrapper* page_context_wrapper_ = nil;

  base::WeakPtrFactory<OnDeviceCategoryClassifierTabHelper> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ON_DEVICE_CATEGORY_CLASSIFIER_ON_DEVICE_CATEGORY_CLASSIFIER_TAB_HELPER_H_
