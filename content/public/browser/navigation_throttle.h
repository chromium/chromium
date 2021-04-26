// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_

#include "base/callback.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"

namespace content {
class NavigationHandle;

// A NavigationThrottle tracks and allows interaction with a navigation on the
// UI thread.
class CONTENT_EXPORT NavigationThrottle {
 public:
  // Represents what a NavigationThrottle can decide to do to a navigation. Note
  // that this enum is implicitly convertable to ThrottleCheckResult.
  enum ThrottleAction {
    FIRST = 0,

    // The action proceeds. This can either mean the navigation continues (e.g.
    // for WillStartRequest) or that the navigation fails (e.g. for
    // WillFailRequest).
    PROCEED = FIRST,

    // Defers the navigation until the NavigationThrottle calls
    // NavigationHandle::Resume or NavigationHandle::CancelDeferredRequest. If
    // the NavigationHandle is destroyed while the navigation is deferred, the
    // navigation will be canceled in the network stack.
    DEFER,

    // Cancels the navigation.
    CANCEL,

    // Cancels the navigation and makes the requester of the navigation act
    // like the request was never made.
    CANCEL_AND_IGNORE,

    // Blocks a navigation due to rules asserted before the request is made.
    // This can only be returned from WillStartRequest or WillRedirectRequest.
    // This will result in a default net_error code of
    // net::ERR_BLOCKED_BY_CLIENT being loaded in the frame that is navigated.
    BLOCK_REQUEST,

    // Blocks a navigation taking place in a subframe, and collapses the frame
    // owner element in the parent document (i.e. removes it from the layout).
    // This can only be returned from WillStartRequest or WillRedirectRequest.
    BLOCK_REQUEST_AND_COLLAPSE,

    // Blocks a navigation due to rules asserted by a response (for instance,
    // embedding restrictions like 'X-Frame-Options'). This result will only
    // be returned from WillProcessResponse.
    BLOCK_RESPONSE,

    LAST = BLOCK_RESPONSE,
  };

  // ThrottleCheckResult, the return value for NavigationThrottle decision
  // methods, is a ThrottleAction value with an attached net::Error and an
  // optional attached error page HTML string.
  //
  // ThrottleCheckResult is implicitly convertible from ThrottleAction, allowing
  // the following examples to work:
  //
  //   ThrottleCheckResult WillStartRequest() override {
  //      // Uses default error for PROCEED (net::OK).
  //      return PROCEED;
  //   }
  //
  //   ThrottleCheckResult WillStartRequest() override {
  //      // Uses default error for BLOCK_REQUEST (net::ERR_BLOCKED_BY_CLIENT).
  //      return BLOCK_REQUEST;
  //   }
  //
  //   ThrottleCheckResult WillStartRequest() override {
  //      // Identical to previous example (net::ERR_BLOCKED_BY_CLIENT)
  //      return {BLOCK_REQUEST};
  //   }
  //
  //   ThrottleCheckResult WillStartRequest() override {
  //      // Uses a custom error code of ERR_FILE_NOT_FOUND.
  //      return {BLOCK_REQUEST, net::ERR_FILE_NOT_FOUND};
  //   }
  //
  //   ThrottleCheckResult WillStartRequest() override {
  //      // Uses a custom error code of ERR_FILE_NOT_FOUND and an error page
  //      string.
  //      return {BLOCK_REQUEST,
  //              net::ERR_FILE_NOT_FOUND,
  //              std::string("<html><body>Could not find.</body></html>")};
  //   }
  class CONTENT_EXPORT ThrottleCheckResult {
   public:
    // Construct with just a ThrottleAction, using the default net::Error for
    // that action.
    ThrottleCheckResult(ThrottleAction action);

    // Construct with an action and error.
    ThrottleCheckResult(ThrottleAction action, net::Error net_error_code);

    // Construct with an action, error, and error page HTML.
    ThrottleCheckResult(ThrottleAction action,
                        net::Error net_error_code,
                        base::Optional<std::string> error_page_content);

    ThrottleCheckResult(const ThrottleCheckResult& other);

    ~ThrottleCheckResult();

    ThrottleAction action() const { return action_; }
    net::Error net_error_code() const { return net_error_code_; }
    const base::Optional<std::string>& error_page_content() {
      return error_page_content_;
    }

   private:
    ThrottleAction action_;
    net::Error net_error_code_;
    base::Optional<std::string> error_page_content_;
  };

  NavigationThrottle(NavigationHandle* navigation_handle);
  virtual ~NavigationThrottle();

  // Called when a network request is about to be made for this navigation.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE or DEFER and perform the destruction asynchronously.
  virtual ThrottleCheckResult WillStartRequest();

  // Called when a server redirect is received by the navigation.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE or DEFER and perform the destruction asynchronously.
  virtual ThrottleCheckResult WillRedirectRequest();

  // Called when a request will fail.
  //
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE or DEFER and perform the destruction asynchronously.
  virtual ThrottleCheckResult WillFailRequest();

  // Called when a response's metadata is available.
  //
  // For HTTP(S) responses, headers will be available.
  // The implementer is responsible for ensuring that the WebContents this
  // throttle is associated with remain alive during the duration of this
  // method. Failing to do so will result in use-after-free bugs. Should the
  // implementer need to destroy the WebContents, it should return CANCEL,
  // CANCEL_AND_IGNORE, or BLOCK_RESPONSE and perform the destruction
  // asynchronously.
  virtual ThrottleCheckResult WillProcessResponse();

  // Returns the name of the throttle for logging purposes. It must not return
  // nullptr.
  virtual const char* GetNameForLogging() = 0;

  // The NavigationHandle that is tracking the information related to this
  // navigation.
  NavigationHandle* navigation_handle() const { return navigation_handle_; }

  // Overrides the default Resume method and replaces it by |callback|. This
  // should only be used in tests.
  void set_resume_callback_for_testing(const base::RepeatingClosure& callback) {
    resume_callback_ = callback;
  }

  // Overrides the default CancelDeferredNavigation method and replaces it by
  // |callback|. This should only be used in tests.
  void set_cancel_deferred_navigation_callback_for_testing(
      const base::RepeatingCallback<void(ThrottleCheckResult)> callback) {
    cancel_deferred_navigation_callback_ = callback;
  }

 protected:
  // Resumes a navigation that was previously deferred by this
  // NavigationThrottle.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles, including this one.
  virtual void Resume();

  // Cancels a navigation that was previously deferred by this
  // NavigationThrottle. |result|'s action should be equal to either:
  //  - NavigationThrottle::CANCEL,
  //  - NavigationThrottle::CANCEL_AND_IGNORE, or
  //  - NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE.
  // Note: this may lead to the deletion of the NavigationHandle and its
  // associated NavigationThrottles, including this one.
  virtual void CancelDeferredNavigation(ThrottleCheckResult result);

 private:
  NavigationHandle* navigation_handle_;

  // Used in tests.
  base::RepeatingClosure resume_callback_;
  base::RepeatingCallback<void(ThrottleCheckResult)>
      cancel_deferred_navigation_callback_;
};

#if defined(UNIT_TEST)
// Test-only operator== to enable assertions like:
//   EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillProcessResponse())
inline bool operator==(NavigationThrottle::ThrottleAction lhs,
                       const NavigationThrottle::ThrottleCheckResult& rhs) {
  return lhs == rhs.action();
}
// Test-only operator!= to enable assertions like:
//   EXPECT_NE(NavigationThrottle::PROCEED, throttle->WillProcessResponse())
inline bool operator!=(NavigationThrottle::ThrottleAction lhs,
                       const NavigationThrottle::ThrottleCheckResult& rhs) {
  return lhs != rhs.action();
}
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_
