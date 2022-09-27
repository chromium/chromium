// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_

#include "third_party/blink/public/common/common_export.h"
#include "url/origin.h"

namespace blink {

// This struct can represent an origin like https://foo.com/ or like
// https://*.foo.com/. The wildcard can only represent a subdomain.
// Note that https://*.foo.com/ matches domains like https://example.foo.com/
// or https://test.example.foo.com/ but does not match https://foo.com/.
// Origins that do have wildcards cannot be opaque.
struct BLINK_COMMON_EXPORT OriginWithPossibleWildcards {
  OriginWithPossibleWildcards();
  OriginWithPossibleWildcards(const url::Origin& origin,
                              bool has_subdomain_wildcard);
  OriginWithPossibleWildcards(const OriginWithPossibleWildcards& rhs);
  OriginWithPossibleWildcards& operator=(
      const OriginWithPossibleWildcards& rhs);
  ~OriginWithPossibleWildcards();

  // If there is no subdomain wildcard, this function returns true if the
  // origins match.
  // For example: https://foo.com/ matches <https://foo.com/, false> but
  // https://bar.foo.com/ does not match <https://foo.com/, false>.
  //
  // If there is a subdomain wildcard, this function returns
  // true if and only if the first origin is a subdomain of the second.
  // For example: https://bar.foo.com/ matches <https://foo.com/, true> but
  // https://foo.com/ does not match <https://foo.com/, true>.
  //
  // For more details on use see:
  // https://github.com/w3c/webappsec-permissions-policy/pull/482
  bool DoesMatchOrigin(const url::Origin& match_origin) const;

  url::Origin origin;
  bool has_subdomain_wildcard{false};
};

bool BLINK_COMMON_EXPORT operator==(const OriginWithPossibleWildcards& lhs,
                                    const OriginWithPossibleWildcards& rhs);
bool BLINK_COMMON_EXPORT operator!=(const OriginWithPossibleWildcards& lhs,
                                    const OriginWithPossibleWildcards& rhs);
bool BLINK_COMMON_EXPORT operator<(const OriginWithPossibleWildcards& lhs,
                                   const OriginWithPossibleWildcards& rhs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_
