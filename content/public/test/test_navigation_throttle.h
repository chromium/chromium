// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class NavigationHandle;

// This class can be used to cancel navigations synchronously or asynchronously
// at specific times in the NavigationThrottle lifecycle.
//
// By default TestNavigationThrottle responds to every method synchronously with
// NavigationThrottle::PROCEED.
class TestNavigationThrottle : public NavigationThrottle {
 public:
  enum ThrottleMethod {
    WILL_START_REQUEST,
    WILL_REDIRECT_REQUEST,
    WILL_FAIL_REQUEST,
    WILL_PROCESS_RESPONSE,
    WILL_COMMIT_WITHOUT_URL_LOADER,
    NUM_THROTTLE_METHODS
  };

  enum ResultSynchrony {
    SYNCHRONOUS,
    ASYNCHRONOUS,
  };

  TestNavigationThrottle(NavigationHandle* handle);

  TestNavigationThrottle(const TestNavigationThrottle&) = delete;
  TestNavigationThrottle& operator=(const TestNavigationThrottle&) = delete;

  ~TestNavigationThrottle() override;

  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;
  NavigationThrottle::ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

  // Return how often the indicated |method| was called.
  int GetCallCount(ThrottleMethod method);

  // Sets the throttle to respond to the method indicated by |method| using
  // |result|, with the given |synchrony|. This overrides any behaviour
  // previously set for the same |method| using SetResult().
  //
  // If |synchrony| is ASYNCHRONOUS, |result|'s action must be one that that is
  // allowed for NavigationThrottle::CancelDeferredNavigation():
  //  - NavigationThrottle::CANCEL,
  //  - NavigationThrottle::CANCEL_AND_IGNORE, or
  //  - NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE.
  //
  // At the moment, it is not possible to specify that the throttle should defer
  // and then asynchronously call Resume().
  void SetResponse(ThrottleMethod method,
                   ResultSynchrony synchrony,
                   NavigationThrottle::ThrottleCheckResult result);

  // Calls SetResponse with the given values for every method.
  void SetResponseForAllMethods(ResultSynchrony synchrony,
                                NavigationThrottle::ThrottleCheckResult result);

  // Callback to be called when the given method is called.
  void SetCallback(ThrottleMethod method, base::RepeatingClosure callback);

 protected:
  // A method that subclasses can override to be called immediately before a
  // throttle responds, either by returning synchronously, or by calling
  // CancelDeferredNavigation() asynchronously.
  //
  // TODO(crbug.com/40542516): Support setting a callback instead, and use that
  // to get rid of the following classes:
  // - ResourceLoadingCancellingThrottle in
  //   ads_page_load_metrics_observer_unittest.cc
  // - DeletingNavigationThrottle in navigation_throttle_runner_unittest.cc
  void OnWillRespond();

 private:
  NavigationThrottle::ThrottleCheckResult ProcessMethod(ThrottleMethod method);
  void CancelAsynchronously(NavigationThrottle::ThrottleCheckResult result);

  struct MethodProperties {
   public:
    MethodProperties();
    ~MethodProperties();

    ResultSynchrony synchrony = SYNCHRONOUS;
    NavigationThrottle::ThrottleCheckResult result = {
        NavigationThrottle::PROCEED};
    base::RepeatingClosure callback;
    int call_count = 0;
  };
  MethodProperties method_properties_[NUM_THROTTLE_METHODS];

  base::WeakPtrFactory<TestNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_H_
