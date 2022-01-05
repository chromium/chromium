// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_SAME_PARTY_CONTEXT_H_
#define NET_COOKIES_SAME_PARTY_CONTEXT_H_

#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"

namespace net {

// This struct bundles together a few different notions of same-party-ness.
// `context_type()` gives the notion of same-party-ness that Chromium should use
// in all cases except metrics; other accessors are just for metrics purposes,
// to explore the impact of different definitions of "same-party".
class NET_EXPORT SamePartyContext {
 public:
  // Computed for every cookie access attempt but is only relevant for SameParty
  // cookies.
  enum class Type {
    // The opposite to kSameParty. Should be the default value.
    kCrossParty = 0,
    // If the request URL is in the same First-Party Sets as the top-frame site
    // and each member of the isolation_info.party_context.
    kSameParty = 1,
  };

  SamePartyContext() = default;
  explicit SamePartyContext(Type type);
  SamePartyContext(Type context_type,
                   Type ancestors_for_metrics,
                   Type top_resource_for_metrics);

  bool operator==(const SamePartyContext& other) const;

  // How trusted is the current browser environment when it comes to accessing
  // SameParty cookies. Default is not trusted, e.g. kCrossParty.
  Type context_type() const { return context_type_; }

  // We store the type of the SameParty context if we inferred singleton sets,
  // for the purpose of metrics.
  Type ancestors_for_metrics_only() const {
    return ancestors_for_metrics_only_;
  }
  // We also store the type of the SameParty context if it were computed using
  // only the top frame and resource URL and inferred singleton sets, for the
  // purpose of metrics.
  Type top_resource_for_metrics_only() const {
    return top_resource_for_metrics_only_;
  }

  // Creates a SamePartyContext that is as permissive as possible.
  static SamePartyContext MakeInclusive();

 private:
  Type context_type_ = Type::kCrossParty;
  Type ancestors_for_metrics_only_ = Type::kCrossParty;
  Type top_resource_for_metrics_only_ = Type::kCrossParty;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const SamePartyContext& spc);

}  // namespace net

#endif  // NET_COOKIES_SAME_PARTY_CONTEXT_H_
