// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_
#define IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/observer_list.h"
#import "base/values.h"
#import "ios/web/annotations/annotations_java_script_feature.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class AnnotationsTextObserver;
class WebState;

/**
 * Class in charge of annotations in text.
 */
class AnnotationsTextManager : public WebStateUserData<AnnotationsTextManager>,
                               public WebStateObserver {
 public:
  ~AnnotationsTextManager() override;

  // Observers registered after web page is loaded will miss some notifications.
  void AddObserver(AnnotationsTextObserver* observer);
  void RemoveObserver(AnnotationsTextObserver* observer);

  // Triggers the JS decoration code with given `annotations`. JS will async
  // calls `OnDecorated` when done and `OnClick` when an annotation is tapped
  // on.
  void DecorateAnnotations(WebState* web_state, base::Value& annotations);

  // Removes all decorations added to the page. Call only if it needs to be done
  // before the page is navigated away from (i.e. at user's request).
  void RemoveDecorations();

  // Removes any highlight added by a tap.
  void RemoveHighlight();

  // JS callback methods.
  void OnTextExtracted(WebState* web_state, const std::string& text);
  void OnDecorated(WebState* web_state, int successes, int annotations);
  void OnClick(WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data);

  // WebStateObserver methods:
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(WebState* web_state) override;

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  friend class WebStateUserData<AnnotationsTextManager>;

  explicit AnnotationsTextManager(WebState* web_state);

  void StartExtractingText();

  // A list of observers. Weak references.
  base::ObserverList<AnnotationsTextObserver, true> observers_;

  WebState* web_state_ = nullptr;
};

}  // namespace web

#endif  // IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_
