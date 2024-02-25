// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_SUITABLE_TRUST_TOKEN_ORIGIN_H_
#define SERVICES_NETWORK_TRUST_TOKENS_SUITABLE_TRUST_TOKEN_ORIGIN_H_

#include <optional>

#include "base/types/pass_key.h"
#include "url/origin.h"

namespace network {

// Class SuitableTrustTokenOrigin is a thin wrapper over url::Origin enforcing
// invariants required of all origins suitable for keying persistent Trust
// Tokens state (https://github.com/wicg/trust-token-api). These origins must
// be:
// - potentially trustworthy, in the sense of
// network::IsOriginPotentiallyTrustworthy (this is a security requirement); and
// - either HTTP or HTTPS (this is so that the origins have unique
// serializations).
class SuitableTrustTokenOrigin {
 public:
  SuitableTrustTokenOrigin() = delete;
  ~SuitableTrustTokenOrigin();

  SuitableTrustTokenOrigin(const SuitableTrustTokenOrigin& rhs);
  SuitableTrustTokenOrigin& operator=(const SuitableTrustTokenOrigin& rhs);
  SuitableTrustTokenOrigin(SuitableTrustTokenOrigin&& rhs);
  SuitableTrustTokenOrigin& operator=(SuitableTrustTokenOrigin&& rhs);

  // Returns nullopt if |origin| (or |url|) is unsuitable for keying Trust
  // Tokens persistent state. Otherwise, returns a new SuitableTrustTokenOrigin
  // wrapping |origin| (or |url|).
  static std::optional<SuitableTrustTokenOrigin> Create(url::Origin origin);
  static std::optional<SuitableTrustTokenOrigin> Create(const GURL& url);

  std::string Serialize() const;
  const url::Origin& origin() const { return origin_; }

  // This implicit "widening" conversion is allowed to ease drop-in use of
  // SuitableTrustTokenOrigin in places currently requiring url::Origins with
  // guaranteed preconditions. The intended use is creating a
  // SuitableTrustTokenOrigin to confirm the preconditions, then directly
  // passing the SuitableTrustTokenOrigin to url::Origin-accepting callsite.
  operator const url::Origin&() const { return origin_; }  // NOLINT

  // Constructs a SuitableTrustTokenOrigin from the given origin. Public only as
  // an implementation detail; clients should use |Create|.
  SuitableTrustTokenOrigin(base::PassKey<SuitableTrustTokenOrigin>,
                           url::Origin&& origin);

 private:
  friend bool operator==(const SuitableTrustTokenOrigin& lhs,
                         const SuitableTrustTokenOrigin& rhs);
  friend bool operator<(const SuitableTrustTokenOrigin& lhs,
                        const SuitableTrustTokenOrigin& rhs);
  url::Origin origin_;
};

inline bool operator==(const SuitableTrustTokenOrigin& lhs,
                       const SuitableTrustTokenOrigin& rhs) {
  return lhs.origin_ == rhs.origin_;
}

inline bool operator<(const SuitableTrustTokenOrigin& lhs,
                      const SuitableTrustTokenOrigin& rhs) {
  return lhs.origin_ < rhs.origin_;
}

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_SUITABLE_TRUST_TOKEN_ORIGIN_H_
