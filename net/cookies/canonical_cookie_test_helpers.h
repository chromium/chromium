// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_
#define NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

MATCHER_P(MatchesCookieLine, cookie_line, "") {
  std::string argument_line = CanonicalCookie::BuildCookieLine(arg);
  if (argument_line == cookie_line)
    return true;

  *result_listener << argument_line;
  return false;
}

// Matches a CanonicalCookie with the given name.
MATCHER_P(MatchesCookieWithName, name, "") {
  return testing::ExplainMatchResult(name, arg.Name(), result_listener);
}

MATCHER_P2(MatchesCookieNameValue, name, value, "") {
  const CanonicalCookie& cookie = arg;
  return testing::ExplainMatchResult(name, cookie.Name(), result_listener) &&
         testing::ExplainMatchResult(value, cookie.Value(), result_listener);
}

MATCHER_P(MatchesCookieAccessWithName, name, "") {
  return testing::ExplainMatchResult(MatchesCookieWithName(name), arg.cookie,
                                     result_listener);
}

// Splits a string into key-value pairs, and executes the provided matcher on
// the result.
MATCHER_P3(WhenKVSplit, pair_delim, kv_delim, inner_matcher, "") {
  std::vector<std::pair<std::string, std::string>> pairs;
  // Ignore the return value of SplitStringIntoKeyValuePairs, to allow pairs
  // with no associated value.
  base::SplitStringIntoKeyValuePairs(arg, kv_delim, pair_delim, &pairs);
  return testing::ExplainMatchResult(inner_matcher, pairs, result_listener);
}

// Splits a ';'-delimited string of Cookie 'name=value' or 'name' pairs, and
// executes the provided matcher on the result.
MATCHER_P(CookieStringIs, inner_matcher, "") {
  return testing::ExplainMatchResult(WhenKVSplit(';', '=', inner_matcher), arg,
                                     result_listener);
}

MATCHER_P2(MatchesCookieWithAccessResult, cookie, access_result, "") {
  const CookieWithAccessResult& cwar = arg;
  return testing::ExplainMatchResult(cookie, cwar.cookie, result_listener) &&
         testing::ExplainMatchResult(access_result, cwar.access_result,
                                     result_listener);
}

// Helper for checking that status.IsInclude() == true.
MATCHER(IsInclude, "") {
  const CookieInclusionStatus& status = arg;
  return testing::ExplainMatchResult(true, status.IsInclude(), result_listener);
}

// Helper for checking that status.HasDowngradeWarning() == true.
MATCHER(HasDowngradeWarning, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasDowngradeWarning(),
                                     result_listener);
}

// Helper for checking that status.HasWarningReason(reason) == true.
MATCHER_P(HasWarningReason, reason, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasWarningReason(reason),
                                     result_listener);
}

// Helper for checking that status.HasExclusionReason(reason) == true.
MATCHER_P(HasExclusionReason, reason, "") {
  CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.HasExclusionReason(reason),
                                     result_listener);
}

// Helper for checking that status.HasExactlyExclusionReasonsForTesting(reasons)
// == true.
MATCHER_P(HasExactlyExclusionReasonsForTesting, reasons, "") {
  const CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(
      true, status.HasExactlyExclusionReasonsForTesting(reasons),
      result_listener);
}

// Helper for checking that status.HasExactlyWarningReasonsForTesting(reasons)
// == true.
MATCHER_P(HasExactlyWarningReasonsForTesting, reasons, "") {
  const CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(
      true, status.HasExactlyWarningReasonsForTesting(reasons),
      result_listener);
}

MATCHER(ShouldWarn, "") {
  net::CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.ShouldWarn(),
                                     result_listener);
}

// Helper for checking CookieAccessResults. Should be called with matchers (or
// values) for each of the fields of a CookieAccessResult.
MATCHER_P4(MatchesCookieAccessResult,
           status,
           effective_same_site,
           access_semantics,
           is_allowed_to_access_secure_cookies,
           "") {
  const CookieAccessResult& car = arg;
  return testing::ExplainMatchResult(status, car.status, result_listener) &&
         testing::ExplainMatchResult(
             effective_same_site, car.effective_same_site, result_listener) &&
         testing::ExplainMatchResult(access_semantics, car.access_semantics,
                                     result_listener) &&
         testing::ExplainMatchResult(is_allowed_to_access_secure_cookies,
                                     car.is_allowed_to_access_secure_cookies,
                                     result_listener);
}

MATCHER_P3(MatchesCookieAndLineWithAccessResult,
           cookie,
           line,
           access_result,
           "") {
  const CookieAndLineWithAccessResult& cookie_and_line_with_access_result = arg;
  return testing::ExplainMatchResult(cookie,
                                     cookie_and_line_with_access_result.cookie,
                                     result_listener) &&
         testing::ExplainMatchResult(
             line, cookie_and_line_with_access_result.cookie_string,
             result_listener) &&
         testing::ExplainMatchResult(
             access_result, cookie_and_line_with_access_result.access_result,
             result_listener);
}

}  // namespace net

#endif  // NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_
