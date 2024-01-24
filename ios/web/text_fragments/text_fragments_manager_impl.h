// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_
#define IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/text_fragments/text_fragments_java_script_feature.h"
#import "services/metrics/public/cpp/ukm_source_id.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class WebState;
struct Referrer;

// Class in charge of highlighting text fragments when they are present in
// WebStates' loaded URLs.
class TextFragmentsManagerImpl : public TextFragmentsManager,
                                 public WebFramesManager::Observer,
                                 public WebStateObserver {
 public:
  explicit TextFragmentsManagerImpl(WebState* web_state);
  ~TextFragmentsManagerImpl() override;

  // Need to overload TextFragmentsManager::CreateForWebState() as the default
  // implementation inherited from WebStateUserData<TextFragmentsManager> would
  // create a TextFragmentsManager which is a pure abstract class.
  static void CreateForWebState(WebState* web_state);

  static TextFragmentsManagerImpl* FromWebState(WebState* web_state);

  // TextFragmentsManager methods:
  void RemoveHighlights() override;
  void RegisterDelegate(id<TextFragmentsDelegate> delegate) override;

  // Invokes post-processing hooks such as metrics logging. `fragment_count`
  // is the number of text fragments that were searched for in the page text;
  // `success_count` is the number of these that were actually found and
  // highlighted.
  void OnProcessingComplete(int success_count, int fragment_count);

  // Event propagated when the user clicks anywhere on the page.
  void OnClick();

  // Event propagated when the user clicks on a highlighted text fragment.
  // CGRect indicates the coordinates of the text fragment sending the event.
  void OnClickWithSender(
      CGRect rect,
      NSString* text,
      std::vector<shared_highlighting::TextFragment> fragments);

  // WebFramesManager::Observer
  void WebFrameBecameAvailable(WebFramesManager* web_frames_manager,
                               WebFrame* web_frame) override;

  // WebStateObserver methods:
  void DidFinishNavigation(WebState* web_state,
                           NavigationContext* navigation_context) override;
  void WebStateDestroyed(WebState* web_state) override;

  void SetJSFeatureForTesting(TextFragmentsJavaScriptFeature* feature);

 private:
  friend class web::WebStateUserData<TextFragmentsManagerImpl>;

  // Stores the params obtained by `ProcessTextFragments` for later execution,
  // in case a main WebFrame is not immediately available.
  struct TextFragmentProcessingParams {
    base::Value parsed_fragments;
    std::string bg_color;
    std::string fg_color;
  };

  // Checks the WebState's destination URL for Text Fragments. Uses the
  // `context` and `referrer` to analyze the current navigation scenario.
  // If the URL and navigation state indicate that a highlight should occur,
  // returns the needed params to complete highlighting. Otherwise, returns
  // empty.
  std::optional<TextFragmentProcessingParams> ProcessTextFragments(
      const web::NavigationContext* context,
      const web::Referrer& referrer);

  // Uses the cached processing params to search the DOM for matching text,
  // highlight the text, and scroll the first into view.
  void DoHighlight();

  bool AreTextFragmentsAllowed(const web::NavigationContext* context);

  TextFragmentsJavaScriptFeature* GetJSFeature();

  raw_ptr<web::WebState> web_state_ = nullptr;
  raw_ptr<TextFragmentsJavaScriptFeature> js_feature_for_testing_ = nullptr;

  // Cached value of the source ID representing the last navigation to have text
  // fragments.
  ukm::SourceId latest_source_id_;

  // Cached value of the latest referrer's URL to have triggered a navigation
  // with text fragments.
  GURL latest_referrer_url_;

  // Processing may be deferred in cases where the main WebFrame isn't available
  // right away. In those cases, the params needed to complete processing are
  // cached here until a frame becomes available.
  std::optional<TextFragmentProcessingParams> deferred_processing_params_;

  __weak id<TextFragmentsDelegate> delegate_;
};

}  // namespace web

#endif  // IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_
