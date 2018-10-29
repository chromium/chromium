// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_
#define CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;

// A FormSubmissionThrottle is responsible for enforcing the 'form-action' CSP
// directive, blocking requests which violate them.
// The form-action CSP is enforced here only for redirects. Blink is enforcing
// it for the initial URL.
// TODO(arthursonzogni): https://crbug.com/663512: Depending on specification
// clarification, we might be able to delete FormSubmissionThrottle altogether.
// It will be deleted if the final specification clarifies that form-action
// should NOT be enforced on redirects.
class CONTENT_EXPORT FormSubmissionThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  ~FormSubmissionThrottle() override;

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  explicit FormSubmissionThrottle(NavigationHandle* handle);
  NavigationThrottle::ThrottleCheckResult CheckContentSecurityPolicyFormAction(
      bool was_server_redirect);

  DISALLOW_COPY_AND_ASSIGN(FormSubmissionThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FORM_SUBMISSION_THROTTLE_H_
