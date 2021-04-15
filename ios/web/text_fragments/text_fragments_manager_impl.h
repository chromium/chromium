// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_
#define IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "services/metrics/public/cpp/ukm_source_id.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class WebState;
class NavigationContext;
struct Referrer;

// Class in charge of highlighting text fragments when they are present in
// WebStates' loaded URLs.
class TextFragmentsManagerImpl : public TextFragmentsManager,
                                 public WebStateObserver {
 public:
  explicit TextFragmentsManagerImpl(WebState* web_state);
  ~TextFragmentsManagerImpl() override;

  // WebStateUserData methods:
  static void CreateForWebState(WebState* web_state);
  static TextFragmentsManagerImpl* FromWebState(WebState* web_state);

  // WebStateObserver methods:
  void WebStateDestroyed(WebState* web_state) override;

  // Checks the WebState's destination URL for Text Fragments. If found,
  // searches the DOM for matching text, highlights the text, and scrolls the
  // first into view. Uses the |context| and |referrer| to analyze the current
  // navigation scenario.
  void ProcessTextFragments(const web::NavigationContext* context,
                            const web::Referrer& referrer);

 private:
  friend class web::WebStateUserData<TextFragmentsManagerImpl>;

  bool AreTextFragmentsAllowed(const web::NavigationContext* context);

  void DidReceiveJavaScriptResponse(const base::DictionaryValue& response);

  web::WebState* web_state_ = nullptr;
  base::CallbackListSubscription subscription_;

  // Cached value of the source ID representing the last navigation to have text
  // fragments.
  ukm::SourceId latest_source_id_;

  // Cached value of the latest referrer's URL to have triggered a navigation
  // with text fragments.
  GURL latest_referrer_url_;
};

}  // namespace web

#endif  // IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_IMPL_H_
