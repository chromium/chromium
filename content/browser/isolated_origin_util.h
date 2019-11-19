// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_ISOLATED_ORIGIN_UTIL_H_
#define CONTENT_BROWSER_ISOLATED_ORIGIN_UTIL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/strings/string_util.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// This class holds isolated origin patterns, providing support for double
// wildcard origins, e.g. https://[*.]foo.com indicates that all domains under
// foo.com are to be treated as if they are distinct isolated
// origins. Non-wildcard origins to be isolated are also supported, e.g.
// https://bar.com.
class CONTENT_EXPORT IsolatedOriginPattern {
 public:
  explicit IsolatedOriginPattern(base::StringPiece pattern);
  explicit IsolatedOriginPattern(const url::Origin& origin);
  ~IsolatedOriginPattern();

  // Copying and moving supported.
  IsolatedOriginPattern(const IsolatedOriginPattern& other);
  IsolatedOriginPattern& operator=(const IsolatedOriginPattern& other);

  IsolatedOriginPattern(IsolatedOriginPattern&& other);
  IsolatedOriginPattern& operator=(IsolatedOriginPattern&& other);

  bool operator==(const IsolatedOriginPattern& other) const {
    // |pattern_| is deliberately not considered during equality comparison as
    // it stores the pattern as supplied at construction time, before
    // normalisation. This leads to erroneous cases of mismatch where
    // IsolatedOriginPattern("foo.com") and IsolatedOriginPattern("foo.com/")
    // will fail equality comparison, despite both resolving to the same origin.
    return origin_ == other.origin_ &&
           isolate_all_subdomains_ == other.isolate_all_subdomains_ &&
           is_valid_ == other.is_valid_;
  }

  // Returns the url::Origin corresponding to the pattern supplied at
  // construction time or via a call to Parse. In the event of parsing failure
  // this oriqin will be opaque.
  const url::Origin& origin() const { return origin_; }

  // True if the supplied pattern was of the form https://[*.]foo.com,
  // indicating all subdomains of foo.com are to be isolated.
  bool isolate_all_subdomains() const { return isolate_all_subdomains_; }

  // Return the original pattern used to construct this instance.
  const base::StringPiece pattern() const { return pattern_; }

  // Return if this origin is valid for isolation purposes.
  bool is_valid() const { return is_valid_; }

 private:
  friend class ChildProcessSecurityPolicyTest;
  FRIEND_TEST_ALL_PREFIXES(ChildProcessSecurityPolicyTest,
                           IsolatedOriginPattern);

  // Checks if |pattern| is a wildcard pattern, checks the scheme is one of
  // {http, https} and constructs a url::Origin() that can be retrieved if
  // parsing is successful. Returns true on successful parsing.
  bool Parse(const base::StringPiece& pattern);

  std::string pattern_;
  url::Origin origin_;
  bool isolate_all_subdomains_;
  bool is_valid_;
};

class CONTENT_EXPORT IsolatedOriginUtil {
 public:
  // Checks whether |origin| matches the isolated origin specified by
  // |isolated_origin|.  Subdomains are considered to match isolated origins,
  // so this will be true if
  // (1) |origin| has the same scheme, host, and port as |isolated_origin|, or
  // (2) |origin| has the same scheme and port as |isolated_origin|, and its
  //     host is a subdomain of |isolated_origin|'s host.
  // This does not consider site URLs, which don't care about port.
  //
  // For example, if |isolated_origin| is https://isolated.foo.com, this will
  // return true if |origin| is https://isolated.foo.com or
  // https://bar.isolated.foo.com, but it will return false for an |origin| of
  // https://unisolated.foo.com or https://foo.com.
  static bool DoesOriginMatchIsolatedOrigin(const url::Origin& origin,
                                            const url::Origin& isolated_origin);

  // Check if |origin| is a valid isolated origin.  Invalid isolated origins
  // include unique origins, origins that don't have an HTTP or HTTPS scheme,
  // and origins without a valid registry-controlled domain.  IP addresses are
  // allowed.
  static bool IsValidIsolatedOrigin(const url::Origin& origin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ISOLATED_ORIGIN_UTIL_H_
