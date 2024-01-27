// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_LIST_OBSERVER_H_

#include <set>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_observer.h"
#include "ios/web/public/web_state_observer.h"

class FullscreenController;
class FullscreenMediator;
class FullscreenModel;

// A WebStateListObserver that creates WebStateObservers that update a
// FullscreenModel for navigation events.
class FullscreenWebStateListObserver : public WebStateListObserver {
 public:
  // Constructor for an observer for `web_state_list` that updates `model`.
  // `controller` is used to create ScopedFullscreenDisablers for WebState
  // navigation events that require the toolbar to be visible.
  FullscreenWebStateListObserver(FullscreenController* controller,
                                 FullscreenModel* model,
                                 FullscreenMediator* mediator);
  ~FullscreenWebStateListObserver() override;

  // The WebStateList being observed.
  void SetWebStateList(WebStateList* web_state_list);
  const WebStateList* GetWebStateList() const;
  WebStateList* GetWebStateList();

  // Stops observing the the WebStateList.
  void Disconnect();

 private:
  // WebStateListObserver:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // Called when `web_state` is activated in `web_state_list_`.
  void WebStateWasActivated(web::WebState* web_state);

  // Called when `web_state` is removed from `web_state_list_`.
  void WebStateWasRemoved(web::WebState* web_state);

  // Whether `web_state` has been activated during the lifetime of this object.
  bool HasWebStateBeenActivated(web::WebState* web_state);

  // The controller passed on construction.
  raw_ptr<FullscreenController> controller_ = nullptr;
  // The model passed on construction.
  raw_ptr<FullscreenModel> model_ = nullptr;
  // The WebStateList passed on construction.
  raw_ptr<WebStateList> web_state_list_ = nullptr;
  // The observer for the active WebState.
  FullscreenWebStateObserver web_state_observer_;
  // The WebStates that have been activated in `web_state_list_`.
  std::set<web::WebState*> activated_web_states_;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_LIST_OBSERVER_H_
