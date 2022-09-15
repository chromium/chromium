// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_PUBLIC_SETS_H_
#define NET_FIRST_PARTY_SETS_PUBLIC_SETS_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

class FirstPartySetsContextConfig;

// This class holds all of the info associated with the public First-Party
// Sets, after they've been parsed. This is suitable for plumbing from the
// browser process to the network service, or for answering queries.
class NET_EXPORT PublicSets {
 public:
  PublicSets();
  PublicSets(base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
             base::flat_map<SchemefulSite, SchemefulSite> aliases);

  PublicSets(PublicSets&&);
  PublicSets& operator=(PublicSets&&);

  ~PublicSets();

  bool operator==(const PublicSets& other) const;
  bool operator!=(const PublicSets& other) const;

  const base::flat_map<SchemefulSite, FirstPartySetEntry>& entries() const {
    return entries_;
  }

  const base::flat_map<SchemefulSite, SchemefulSite>& aliases() const {
    return aliases_;
  }

  // Creates a clone of this instance.
  PublicSets Clone() const;

  // Returns the entry corresponding to the given `site`, if one exists.
  // Respects any customization/overlay specified by `config`. This is
  // semi-agnostic to scheme: it just cares whether the scheme is secure or
  // insecure.
  absl::optional<FirstPartySetEntry> FindEntry(
      const SchemefulSite& site,
      const FirstPartySetsContextConfig* config) const;

  // Batched version of `FindEntry`. Where `FindEntry` would have returned
  // nullopt, this just omits from the result map.
  base::flat_map<SchemefulSite, FirstPartySetEntry> FindEntries(
      const base::flat_set<SchemefulSite>& sites,
      const FirstPartySetsContextConfig* config) const;

  // Modifies this instance such that it will respect the given
  // manually-specified set.
  void ApplyManuallySpecifiedSet(
      const SchemefulSite& manual_primary,
      const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries,
      const base::flat_map<SchemefulSite, SchemefulSite>& manual_aliases);

 private:
  // Finds the intersection between the underlying entries and the given
  // manually-specified set.
  //
  // The returned collection also includes any sites in the underlying entries
  // whose primary was in the intersection.
  base::flat_set<SchemefulSite> FindIntersection(
      const SchemefulSite& manual_primary,
      const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries)
      const;

  // Finds singleton sets in the underlying entries, which are sets that consist
  // of only a single site.
  base::flat_set<SchemefulSite> FindSingletons() const;

  // Represents the mapping of site -> entry, where keys are sites within sets,
  // and values are entries of the sets.
  base::flat_map<SchemefulSite, FirstPartySetEntry> entries_;

  // The site aliases. Used to normalize a given SchemefulSite into its
  // canonical representative, before looking it up in `entries_`.
  base::flat_map<SchemefulSite, SchemefulSite> aliases_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os, const PublicSets& ps);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_ENTRY_H_
