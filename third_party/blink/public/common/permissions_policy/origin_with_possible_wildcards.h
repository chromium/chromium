// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_

#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "url/origin.h"

namespace blink {

// This struct can represent an origin like https://foo.com/ or like
// https://*.foo.com/. The wildcard can only represent a subdomain.
// Note that https://*.foo.com/ matches domains like https://example.foo.com/
// or https://test.example.foo.com/ but does not match https://foo.com/.
// Origins that do have wildcards cannot be opaque.
class BLINK_COMMON_EXPORT OriginWithPossibleWildcards {
 public:
  // Indicates the source of a parsed permissions policy. kHeader represents a
  // permissions policy in an HTTP header. kAttribute represents an iframe allow
  // policy. kUnknown indicated an unknown source. This affects which wildcards
  // types are permitted in allowlist origins.
  enum NodeType { kHeader, kAttribute, kUnknown };

  OriginWithPossibleWildcards();
  OriginWithPossibleWildcards(const OriginWithPossibleWildcards& rhs);
  OriginWithPossibleWildcards& operator=(
      const OriginWithPossibleWildcards& rhs);
  ~OriginWithPossibleWildcards();

  // This is a shortcut for making a wildcard-free OriginWithPossibleWildcards.
  // The returned value will be empty if the origin is opaque or Parse fails.
  static std::optional<OriginWithPossibleWildcards> FromOrigin(
      const url::Origin& origin);

  // This is a shortcut for making a OriginWithPossibleWildcards while
  // overriding the wildcard options. Non-tests should use Parse() if they want
  // wildcard support, or FromOrigin() if they don't need it.
  // The returned value will be empty if the origin is opaque or Parse fails.
  static std::optional<OriginWithPossibleWildcards>
  FromOriginAndWildcardsForTest(const url::Origin& origin,
                                bool has_subdomain_wildcard);

  // This constructs a OriginWithPossibleWildcards from an allowlist_entry which
  // might or might not have a subdomain wildcard (only if the type is kHeader).
  // This does not support special types like *, 'self', 'src', or 'none'.
  // If the entry cannot be parsed then std::nullopt is returned.
  static std::optional<OriginWithPossibleWildcards> Parse(
      const std::string& allowlist_entry,
      const NodeType type);

  // This should neatly undo the work of Parse, which is to say it
  // serializes the origin and inserts a *. back into the front of the host
  // if there was a subdomain wildcard.
  std::string Serialize() const;

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

  const network::mojom::CSPSource& CSPSourceForTest() const {
    return csp_source;
  }

 private:
  friend struct mojo::StructTraits<
      blink::mojom::OriginWithPossibleWildcardsDataView,
      blink::OriginWithPossibleWildcards>;
  BLINK_COMMON_EXPORT
  friend bool operator==(const OriginWithPossibleWildcards& lhs,
                         const OriginWithPossibleWildcards& rhs);
  BLINK_COMMON_EXPORT
  friend bool operator!=(const OriginWithPossibleWildcards& lhs,
                         const OriginWithPossibleWildcards& rhs);
  BLINK_COMMON_EXPORT
  friend bool operator<(const OriginWithPossibleWildcards& lhs,
                        const OriginWithPossibleWildcards& rhs);

  network::mojom::CSPSource csp_source;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_ORIGIN_WITH_POSSIBLE_WILDCARDS_H_
