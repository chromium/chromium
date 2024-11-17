// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_THROTTLE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"

namespace content {
class NavigationHandle;

// A NavigationThrottle tracks and allows interaction with a navigation on the
// UI thread. NavigationThrottles may not be run for some kinds of navigations
// (e.g. same-document navigations, about:blank, activations into the primary
// frame tree like prerendering and back-forward cache, etc.). Content-internal
// code that just wishes to defer a commit, including activations to the
// primary frame tree, should instead use a CommitDeferringCondition.
class CONTENT_EXPORT NavigationThrottle {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

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
    // Note: since this slows page load it should be avoided unless there's no
    // other option. An example necessary case would be locked down users where
    // a server check needs to be done before starting the navigation. For other
    // cases, please consider alternatives like sending data to the renderer
    // asynchronously, showing interstitials later when possible etc. It's good
    // practice to add histograms to know how long the delay takes.
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
                        std::optional<std::string> error_page_content);

    ThrottleCheckResult(const ThrottleCheckResult& other);

    ~ThrottleCheckResult();

    ThrottleAction action() const { return action_; }
    net::Error net_error_code() const { return net_error_code_; }
    const std::optional<std::string>& error_page_content() {
      return error_page_content_;
    }

   private:
    ThrottleAction action_;
    net::Error net_error_code_;
    std::optional<std::string> error_page_content_;
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

  // Called when a navigation is about to immediately commit because there's no
  // need for a url loader. This includes browser-initiated same-document
  // navigations, same-document history navigations, about:blank, about:srcdoc,
  // any other empty document scheme, and MHTML subframes.
  // Renderer-initiated non-history same-document navigations do NOT go through
  // this path, because they are handled synchronously in the renderer and the
  // browser process is only notified after the fact.
  // BFCache and prerender activation also do NOT go through this path, because
  // they are considered already loaded when they are activated.
  // In order to get this event, a NavigationThrottle must register itself with
  // RegisterNavigationThrottlesForCommitWithoutUrlLoader().
  // This event is mutually exclusive with WillStartRequest,
  // WillRedirectRequest, and WillProcessResponse. Only WillFailRequest can
  // be called after WillCommitWithoutUrlLoader.
  // Only PROCEED, DEFER, and CANCEL_AND_IGNORE results are supported at this
  // time.
  virtual ThrottleCheckResult WillCommitWithoutUrlLoader();

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
  const raw_ptr<NavigationHandle> navigation_handle_;

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
