// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {

class WebState;
class TestWebState;

// Decides the navigation policy for a web state.
class WebStatePolicyDecider {
 public:
  // Data Transfer Object for the additional information about navigation
  // request passed to WebStatePolicyDecider::ShouldAllowRequest().
  struct RequestInfo {
    RequestInfo(ui::PageTransition transition_type,
                bool target_frame_is_main,
                bool has_user_gesture)
        : transition_type(transition_type),
          target_frame_is_main(target_frame_is_main),
          has_user_gesture(has_user_gesture) {}
    // The navigation page transition type.
    ui::PageTransition transition_type =
        ui::PageTransition::PAGE_TRANSITION_FIRST;
    // Indicates whether the navigation target frame is the main frame.
    bool target_frame_is_main = false;
    // Indicates if there was a recent user interaction with the request frame.
    bool has_user_gesture = false;
  };

  // Removes self as a policy decider of |web_state_|.
  virtual ~WebStatePolicyDecider();

  // Asks the decider whether the navigation corresponding to |request| should
  // be allowed to continue. Defaults to true if not overriden.
  // Called before WebStateObserver::DidStartNavigation.
  // Never called in the following cases:
  //  - same-document back-forward and state change navigations
  //  - CRWNativeContent navigations
  virtual bool ShouldAllowRequest(NSURLRequest* request,
                                  const RequestInfo& request_info);

  // Asks the decider whether the navigation corresponding to |response| should
  // be allowed to continue. Defaults to true if not overriden.
  // |for_main_frame| indicates whether the frame being navigated is the main
  // frame. Called before WebStateObserver::DidFinishNavigation.
  // Never called in the following cases:
  //  - same-document navigations (unless ititiated via LoadURLWithParams)
  //  - CRWNativeContent navigations
  //  - going back after form submission navigation
  //  - user-initiated POST navigation on iOS 10
  virtual bool ShouldAllowResponse(NSURLResponse* response,
                                   bool for_main_frame);

  // Notifies the policy decider that the web state is being destroyed.
  // Gives subclasses a chance to cleanup.
  // The policy decider must not be destroyed while in this call, as removing
  // while iterating is not supported.
  virtual void WebStateDestroyed() {}

  WebState* web_state() const { return web_state_; }

 protected:
  // Designated constructor. Subscribes to |web_state|.
  explicit WebStatePolicyDecider(WebState* web_state);

 private:
  friend class WebStateImpl;
  friend class TestWebState;

  // Resets the current web state.
  void ResetWebState();

  // The web state to decide navigation policy for.
  WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(WebStatePolicyDecider);
};
}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
