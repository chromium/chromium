// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_

#include "base/macros.h"

#include "base/scoped_observer.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

// Interface for getting notified when WebStates get added/removed to/from an
// incognito browser state. For example, implementations can invoke
// TabModelList::IsOffTheRecordSessionActive() in the body of the observer
// methods to learn if incognito session is currently active (i.e., at least one
// incognito tab is open).
class IncognitoWebStateObserver : public TabModelListObserver,
                                  public WebStateListObserver {
 public:
  IncognitoWebStateObserver();
  ~IncognitoWebStateObserver() override;

  // TabModelListObserver:
  void TabModelRegisteredWithBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;
  void TabModelUnregisteredFromBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

 protected:
  virtual void OnIncognitoWebStateAdded() = 0;
  virtual void OnIncognitoWebStateRemoved() = 0;

 private:
  ScopedObserver<WebStateList, WebStateListObserver> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(IncognitoWebStateObserver);
};

#endif  // IOS_CHROME_BROWSER_METRICS_INCOGNITO_WEB_STATE_OBSERVER_H_
