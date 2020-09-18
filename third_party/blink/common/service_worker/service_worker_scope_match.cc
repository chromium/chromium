// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"

#include "base/strings/string_util.h"

namespace blink {

bool ServiceWorkerScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

ServiceWorkerLongestScopeMatcher::ServiceWorkerLongestScopeMatcher(
    const GURL& url)
    : url_(url) {}

ServiceWorkerLongestScopeMatcher::~ServiceWorkerLongestScopeMatcher() = default;

bool ServiceWorkerLongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerScopeMatches(scope, url_))
    return false;
  if (match_.is_empty() || match_.spec().size() < scope.spec().size()) {
    match_ = scope;
    return true;
  }
  return false;
}

}  // namespace blink
