// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"

#include "base/containers/contains.h"
#include "base/strings/string_util.h"

namespace blink {

namespace {

bool PathContainsDisallowedCharacter(const GURL& url) {
  std::string path = url.path();
  DCHECK(base::IsStringUTF8(path));

  // We should avoid these escaped characters in the path component because
  // these can be handled differently depending on server implementation.
  if (base::Contains(path, "%2f") || base::Contains(path, "%2F")) {
    return true;
  }
  if (base::Contains(path, "%5c") || base::Contains(path, "%5C")) {
    return true;
  }
  return false;
}

}  // namespace

bool ServiceWorkerScopeOrScriptUrlContainsDisallowedCharacter(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  if (PathContainsDisallowedCharacter(scope) ||
      PathContainsDisallowedCharacter(script_url)) {
    *error_message = "The provided scope ('";
    error_message->append(scope.spec());
    error_message->append("') or scriptURL ('");
    error_message->append(script_url.spec());
    error_message->append("') includes a disallowed escape character.");
    return true;
  }
  return false;
}

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
