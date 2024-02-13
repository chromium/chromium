// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {

class WebState;
class FakeWebState;

// Decides the navigation policy for a web state.
class WebStatePolicyDecider : public base::CheckedObserver {
 public:
  // Specifies a navigation decision. Used as a return value by
  // WebStatePolicyDecider::ShouldAllowRequest(), and used by
  // WebStatePolicyDecider::ShouldAllowResponse() when sending its decision to
  // its callback.
  struct PolicyDecision {
    // A policy decision which allows the navigation.
    static PolicyDecision Allow();

    // A policy decision which cancels the navigation.
    static PolicyDecision Cancel();

    // A policy decision which cancels the navigation and displays `error`.
    // NOTE: The `error` will only be displayed if the associated navigation is
    // being loaded in the main frame.
    static PolicyDecision CancelAndDisplayError(NSError* error);

    // Whether or not the navigation will continue.
    bool ShouldAllowNavigation() const;

    // Whether or not the navigation will be cancelled.
    bool ShouldCancelNavigation() const;

    // Whether or not an error should be displayed. Always returns false if
    // `ShouldAllowNavigation` is true.
    // NOTE: Will return true when the receiver is created with
    // `CancelAndDisplayError` even though an error will only end up being
    // displayed if the associated navigation is occurring in the main frame.
    bool ShouldDisplayError() const;

    // The error to display when `ShouldDisplayError` is true.
    NSError* GetDisplayError() const;

   private:
    // The decisions which can be taken for a given navigation.
    enum class Decision {
      // Allow the navigation to proceed.
      kAllow,

      // Cancel the navigation.
      kCancel,

      // Cancel the navigation and display an error.
      kCancelAndDisplayError,
    };

    PolicyDecision(Decision decision, NSError* error)
        : decision(decision), error(error) {}

    // The decision to be taken for a given navigation.
    Decision decision = Decision::kAllow;

    // An error associated with the navigation. This error will be displayed if
    // `decision` is `kCancelAndDisplayError`.
    NSError* error = nil;
  };

  // Callback used to provide asynchronous policy decisions.
  typedef base::OnceCallback<void(PolicyDecision)> PolicyDecisionCallback;

  // Data Transfer Object for the additional information about navigation
  // request passed to WebStatePolicyDecider::ShouldAllowRequest().
  struct RequestInfo {
    RequestInfo(ui::PageTransition transition_type,
                bool target_frame_is_main,
                bool target_frame_is_cross_origin,
                bool target_window_is_cross_origin,
                bool is_user_initiated,
                bool user_tapped_recently)
        : transition_type(transition_type),
          target_frame_is_main(target_frame_is_main),
          target_frame_is_cross_origin(target_frame_is_cross_origin),
          target_window_is_cross_origin(target_window_is_cross_origin),
          is_user_initiated(is_user_initiated),
          user_tapped_recently(user_tapped_recently) {}
    // The navigation page transition type.
    ui::PageTransition transition_type =
        ui::PageTransition::PAGE_TRANSITION_FIRST;
    // Indicates whether the navigation target frame is the main frame.
    bool target_frame_is_main = false;
    // Indicates whether the navigation target frame is cross-origin with
    // respect to the the navigation source frame.
    bool target_frame_is_cross_origin = false;
    // Indicates whether the navigation target frame is in another window and is
    // cross-origin with respect to the the navigation source frame.
    bool target_window_is_cross_origin = false;
    // Indicates if the request is user initiated (to the best of our
    // knowledge).
    bool is_user_initiated = false;
    // Indicates if there was a recent user interaction with the web view (not
    // necessarily on the page).
    bool user_tapped_recently = false;
  };

  // Data Transfer Object for the additional information about response
  // request passed to WebStatePolicyDecider::ShouldAllowResponse().
  struct ResponseInfo {
    explicit ResponseInfo(bool for_main_frame)
        : for_main_frame(for_main_frame) {}
    // Indicates whether the response target frame is the main frame.
    bool for_main_frame = false;
  };

  WebStatePolicyDecider(const WebStatePolicyDecider&) = delete;
  WebStatePolicyDecider& operator=(const WebStatePolicyDecider&) = delete;

  // Removes self as a policy decider of `web_state_`.
  ~WebStatePolicyDecider() override;

  // Asks the decider whether the navigation corresponding to `request` should
  // be allowed to continue. Defaults to PolicyDecision::Allow() if not
  // overridden. Called before WebStateObserver::DidStartNavigation. Calls
  // `callback` with the decision. Never called in the following cases:
  //  - same-document back-forward and state change navigations
  virtual void ShouldAllowRequest(NSURLRequest* request,
                                  RequestInfo request_info,
                                  PolicyDecisionCallback callback);

  // Asks the decider whether the navigation corresponding to `response` should
  // be allowed to continue. Defaults to PolicyDecision::Allow() if not
  // overridden. Called before WebStateObserver::DidFinishNavigation. Calls
  // `callback` with the decision.
  // Never called in the following cases:
  //  - same-document navigations (unless initiated via LoadURLWithParams)
  //  - going back after form submission navigation
  //  - user-initiated POST navigation on iOS 10
  virtual void ShouldAllowResponse(NSURLResponse* response,
                                   ResponseInfo response_info,
                                   PolicyDecisionCallback callback);

  // Notifies the policy decider that the web state is being destroyed.
  // Gives subclasses a chance to cleanup.
  // The policy decider must not be destroyed while in this call, as removing
  // while iterating is not supported.
  virtual void WebStateDestroyed() {}

  WebState* web_state() const { return web_state_; }

 protected:
  // Designated constructor. Subscribes to `web_state`.
  explicit WebStatePolicyDecider(WebState* web_state);

 private:
  friend class ContentWebState;
  friend class FakeWebState;
  friend class WebStateImpl;

  // Resets the current web state.
  void ResetWebState();

  // The web state to decide navigation policy for.
  raw_ptr<WebState> web_state_;
};
}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_WEB_STATE_POLICY_DECIDER_H_
