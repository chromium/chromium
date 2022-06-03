// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/allow_service_worker_result.h"

namespace content {

AllowServiceWorkerResult AllowServiceWorkerResult::Yes() {
  return AllowServiceWorkerResult(true, false, false);
}

AllowServiceWorkerResult AllowServiceWorkerResult::No() {
  return AllowServiceWorkerResult(false, false, false);
}

AllowServiceWorkerResult AllowServiceWorkerResult::FromPolicy(
    bool javascript_blocked_by_policy,
    bool cookies_blocked_by_policy) {
  return AllowServiceWorkerResult(
      !javascript_blocked_by_policy && !cookies_blocked_by_policy,
      javascript_blocked_by_policy, cookies_blocked_by_policy);
}

AllowServiceWorkerResult::AllowServiceWorkerResult(
    bool allowed,
    bool javascript_blocked_by_policy,
    bool cookies_blocked_by_policy)
    : allowed_(allowed),
      javascript_blocked_by_policy_(javascript_blocked_by_policy),
      cookies_blocked_by_policy_(cookies_blocked_by_policy) {}

bool AllowServiceWorkerResult::operator==(
    const AllowServiceWorkerResult& other) const {
  return allowed_ == other.allowed_ &&
         javascript_blocked_by_policy_ == other.javascript_blocked_by_policy_ &&
         cookies_blocked_by_policy_ == other.cookies_blocked_by_policy_;
}

}  // namespace content
