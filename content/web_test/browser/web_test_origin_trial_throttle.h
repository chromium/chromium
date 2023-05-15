// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_ORIGIN_TRIAL_THROTTLE_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_ORIGIN_TRIAL_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class OriginTrialsControllerDelegate;

// NavigationThrottle that sets a header for testing with the names of all
// enabled persistent origin trials.
// This exists to support the tests in
// third_party/blink/web_tests/http/tests/persistent-origin-trial
// by providing an observable effect of setting a persistent origin trial.
class WebTestOriginTrialThrottle : public NavigationThrottle {
 public:
  WebTestOriginTrialThrottle(NavigationHandle* navigation_handle,
                             OriginTrialsControllerDelegate* delegate);
  ~WebTestOriginTrialThrottle() override = default;

  // NavigationThrottle implementation
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Helper function that set the X-Web-Test-Enabled-Origin-Trials header
  void SetHeaderForRequest();

  raw_ptr<OriginTrialsControllerDelegate> origin_trials_controller_delegate_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_ORIGIN_TRIAL_THROTTLE_H_
