// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_SETS_MUTATION_H_
#define NET_FIRST_PARTY_SETS_SETS_MUTATION_H_

#include "base/containers/flat_map.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

// SetsMutation represents a mutation to be applied to the list of global
// Related Website Sets. A mutation can come from the
// RelatedWebsiteSetsOverrides policy.
//
// See `GlobalFirstPartySets` for how SetsMutations are layered on top of the
// public sets and the local set declaration (if any).
class NET_EXPORT SetsMutation {
 public:
  SetsMutation();

  // Preconditions:
  // * Sets defined by `replacement_sets` and `addition_sets` must be disjoint.
  // * All of the canonical sites in `aliases` (the "values") must occur in
  // (exactly) one of the replacement or addition sets, and have an identical
  // entry to that of the alias (the "key").
  SetsMutation(std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
                   replacement_sets,
               std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
                   addition_sets,
               base::flat_map<SchemefulSite, SchemefulSite> aliases);

  ~SetsMutation();

  SetsMutation(const SetsMutation&);
  SetsMutation& operator=(const SetsMutation&);
  SetsMutation(SetsMutation&&);
  SetsMutation& operator=(SetsMutation&&);

  bool operator==(const SetsMutation& other) const;

  const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
  replacements() const {
    return replacements_;
  }

  const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
  additions() const {
    return additions_;
  }

  const base::flat_map<SchemefulSite, SchemefulSite>& aliases() const {
    return aliases_;
  }

 private:
  // The list of "replacement" sets.
  std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>> replacements_;

  // The list of "addition" sets.
  std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>> additions_;

  // Any aliases to be used in this mutation.
  base::flat_map<SchemefulSite, SchemefulSite> aliases_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const SetsMutation& mutation);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_SETS_MUTATION_H_
