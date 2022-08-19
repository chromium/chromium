// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "ios/web/annotations/annotations_java_script_feature.h"
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
  explicit AnnotationsTabHelper(web::WebState* web_state);
  ~AnnotationsTabHelper() override;

  // Sets the BaseViewController from which to present UI.
  void SetBaseViewController(UIViewController* baseViewController);

  // WebStateUserData methods:
  static void CreateForWebState(web::WebState* web_state);

  // AnnotationsTextObserver methods:
  void OnTextExtracted(web::WebState* web_state,
                       const std::string& text) override;
  void OnDecorated(web::WebState* web_state,
                   int successes,
                   int annotations) override;
  void OnClick(web::WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data) override;

  // WebStateObserver methods:
  void WebFrameDidBecomeAvailable(web::WebState* web_state,
                                  web::WebFrame* web_frame) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  friend class WebStateUserData<AnnotationsTabHelper>;

  UIViewController* base_view_controller_;

  // Applies an text classifier to extract intents in the given text. Returns
  // a `base::Value::List` of annotations (see i/w/a/annotations_utils.h).
  absl::optional<base::Value> ApplyDataExtractor(const std::string& text);

  // Sends `deferred_processing_params_` to the `web::AnnotationstextManager`
  // for styling.
  void DecorateAnnotations();

  web::WebState* web_state_ = nullptr;

  // Processing may be deferred in cases where the main WebFrame isn't available
  // right away. In those cases, the params needed to complete processing are
  // cached here until a frame becomes available.
  absl::optional<base::Value> deferred_processing_params_;

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<AnnotationsTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_TAB_HELPER_H_
