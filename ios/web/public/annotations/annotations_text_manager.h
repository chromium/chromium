// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_
#define IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_

#import <UIKit/UIKit.h>

#import "base/observer_list.h"
#import "base/values.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class AnnotationsTextObserver;
class WebState;

/**
 * Class in charge of annotations in text.
 */
class AnnotationsTextManager : public WebStateUserData<AnnotationsTextManager> {
 public:
  // Overload as the default implementation to create impl.
  static void CreateForWebState(WebState* web_state);

  // Returns the content world associated with the JavaScript called by this
  // feature.
  static ContentWorld GetFeatureContentWorld();

  AnnotationsTextManager() = default;

  AnnotationsTextManager(const AnnotationsTextManager&) = delete;
  AnnotationsTextManager& operator=(const AnnotationsTextManager&) = delete;

  // Observers registered after web page is loaded will miss some notifications.
  virtual void AddObserver(AnnotationsTextObserver* observer) = 0;
  virtual void RemoveObserver(AnnotationsTextObserver* observer) = 0;

  // Triggers the JS decoration code with given `annotations`. JS will async
  // calls `OnDecorated` when done and `OnClick` when an annotation is tapped
  // on. `seq_id` must be the one passed to an observer `OnTextExtracted`.
  virtual void DecorateAnnotations(WebState* web_state,
                                   base::Value& annotations,
                                   int seq_id) = 0;

  // Removes all decorations added to the page. Call only if it needs to be done
  // before the page is navigated away from (i.e. at user's request).
  virtual void RemoveDecorations() = 0;

  // Removes all decorations of one type added to the page. Call only if it
  // needs to be done before the page is navigated away from (i.e. at user's
  // request).
  virtual void RemoveDecorationsWithType(const std::string& type) = 0;

  // Removes any highlight added by a tap.
  virtual void RemoveHighlight() = 0;

  // Sets the supported typed for the annotation extraction.
  virtual void SetSupportedTypes(NSTextCheckingType supported_types) = 0;

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  ~AnnotationsTextManager() override = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_H_
