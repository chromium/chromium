// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_TAB_HELPER_H_

#include <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class JavaScriptFindInPageController;
@class FindInPageModel;
@protocol FindInPageResponseDelegate;

// Adds support for the "Find in page" feature.
class JavaScriptFindTabHelper final
    : public AbstractFindTabHelper,
      public web::WebStateObserver,
      public web::WebStateUserData<JavaScriptFindTabHelper> {
 public:
  JavaScriptFindTabHelper(const JavaScriptFindTabHelper&) = delete;
  JavaScriptFindTabHelper& operator=(const JavaScriptFindTabHelper&) = delete;

  ~JavaScriptFindTabHelper() final;

  // AbstractFindTabHelper implementation
  void SetResponseDelegate(
      id<FindInPageResponseDelegate> response_delegate) final;
  void StartFinding(NSString* search_string) final;
  void ContinueFinding(FindDirection direction) final;
  void StopFinding() final;
  FindInPageModel* GetFindResult() const final;
  bool CurrentPageSupportsFindInPage() const final;
  bool IsFindUIActive() const final;
  void SetFindUIActive(bool active) final;
  void PersistSearchTerm() final;
  void RestoreSearchTerm() final;

 private:
  friend class web::WebStateUserData<JavaScriptFindTabHelper>;

  // Private constructor used by CreateForWebState().
  JavaScriptFindTabHelper(web::WebState* web_state);

  // Create the JavaScriptFindInPageController for `web_state`. Only called
  // if/when the WebState is realized.
  void CreateFindInPageController(web::WebState* web_state);

  // web::WebStateObserver.
  void WebStateRealized(web::WebState* web_state) final;
  void WebStateDestroyed(web::WebState* web_state) final;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) final;

  // The ObjC find in page controller (nil if the WebState is not realized).
  JavaScriptFindInPageController* controller_ = nil;

  // The delegate to register with JavaScriptFindInPageController when it is
  // created.
  __weak id<FindInPageResponseDelegate> response_delegate_ = nil;

  // Manage the registration of this instance as a WebStateObserver.
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_JAVA_SCRIPT_FIND_TAB_HELPER_H_
