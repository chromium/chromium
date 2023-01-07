// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_SAME_PARTY_CONTEXT_H_
#define NET_FIRST_PARTY_SETS_SAME_PARTY_CONTEXT_H_

#include <ostream>

#include "net/base/net_export.h"

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
  explicit SamePartyContext(Type context_type);

  bool operator==(const SamePartyContext& other) const;

  // How trusted is the current browser environment when it comes to accessing
  // SameParty cookies. Default is not trusted, e.g. kCrossParty.
  Type context_type() const { return context_type_; }

  // Creates a SamePartyContext that is as permissive as possible.
  static SamePartyContext MakeInclusive();

 private:
  Type context_type_ = Type::kCrossParty;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const SamePartyContext& spc);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_SAME_PARTY_CONTEXT_H_
