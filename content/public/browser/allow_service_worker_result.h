// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ALLOW_SERVICE_WORKER_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_ALLOW_SERVICE_WORKER_RESULT_H_

#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT AllowServiceWorkerResult {
 public:
  static AllowServiceWorkerResult Yes();
  static AllowServiceWorkerResult No();
  static AllowServiceWorkerResult FromPolicy(bool javascript_blocked_by_policy,
                                             bool cookies_blocked_by_policy);

  operator bool() { return allowed_; }

  bool javascript_blocked_by_policy() const {
    return javascript_blocked_by_policy_;
  }

  bool cookies_blocked_by_policy() const { return cookies_blocked_by_policy_; }

  bool operator==(const AllowServiceWorkerResult& other) const;

 private:
  AllowServiceWorkerResult(bool allowed,
                           bool javacript_blocked_by_policy,
                           bool cookies_blocked_by_policy);

  bool allowed_ = false;
  bool javascript_blocked_by_policy_ = false;
  bool cookies_blocked_by_policy_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ALLOW_SERVICE_WORKER_RESULT_H_
