// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEME_HOST_PORT_MATCHER_RESULT_H_
#define NET_BASE_SCHEME_HOST_PORT_MATCHER_RESULT_H_

namespace net {

// The result of evaluating an URLMatcherRule.
//
// Matches can be for including a URL, or for excluding a URL, or neither of
// them.
enum class SchemeHostPortMatcherResult {
  // No match.
  kNoMatch,
  // The URL should be included.
  kInclude,
  // The URL should be excluded.
  kExclude,
};

}  // namespace net
#endif  // NET_BASE_SCHEME_HOST_PORT_MATCHER_RESULT_H_
