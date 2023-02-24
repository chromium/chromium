// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/values.h"
#import "ios/web/public/annotations/annotations_text_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

@protocol CRWWebViewHandlerDelegate;
@class UIViewController;

namespace web {
class WebState;
}  // namespace web

/**
 * Class in charge of annotations in text.
 */
class AnnotationsTabHelper : public web::AnnotationsTextObserver,
                             public web::WebStateUserData<AnnotationsTabHelper>,
                             public web::WebStateObserver {
 public:
  ~AnnotationsTabHelper() override;

  // Sets the BaseViewController from which to present UI.
  void SetBaseViewController(UIViewController* baseViewController);

  // AnnotationsTextObserver methods:
  void OnTextExtracted(web::WebState* web_state,
                       const std::string& text,
                       int seq_id) override;
  void OnDecorated(web::WebState* web_state,
                   int successes,
                   int annotations) override;
  void OnClick(web::WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data) override;

  // WebStateObserver methods:
  void WebStateDestroyed(web::WebState* web_state) override;

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  friend class WebStateUserData<AnnotationsTabHelper>;

  explicit AnnotationsTabHelper(web::WebState* web_state);

  // Receiver for text classifier extracted intents. Must run on main thread.
  // `seq_id` comes from `OnTextExtracted` and is meant to be passed on to
  // `AnnotationsTextManager::DecorateAnnotations` to validate decorations
  // against the text extracted.
  void ApplyDeferredProcessing(int seq_id,
                               absl::optional<base::Value> deferred);

  UIViewController* base_view_controller_ = nil;

  web::WebState* web_state_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<AnnotationsTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
