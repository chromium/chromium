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
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This class holds all of the info associated with the public First-Party
// Sets, after they've been parsed. This is suitable for plumbing from the
// browser process to the network service, or for answering queries.
class NET_EXPORT PublicSets {
 public:
  PublicSets();
  PublicSets(base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
             base::flat_map<SchemefulSite, SchemefulSite> aliases);
  PublicSets(base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
             base::flat_map<SchemefulSite, SchemefulSite> aliases,
             FirstPartySetsContextConfig manual_config);

  PublicSets(PublicSets&&);
  PublicSets& operator=(PublicSets&&);

  ~PublicSets();

  bool operator==(const PublicSets& other) const;
  bool operator!=(const PublicSets& other) const;

  // The accessors below should only be used by mojo plumbing.
  const base::flat_map<SchemefulSite, FirstPartySetEntry>& entries() const {
    return entries_;
  }
  const base::flat_map<SchemefulSite, SchemefulSite>& aliases() const {
    return aliases_;
  }
  const FirstPartySetsContextConfig& manual_config() const {
    return manual_config_;
  }

  // Creates a clone of this instance.
  PublicSets Clone() const;

  // Returns a FirstPartySetsContextConfig suitable for passing into
  // FindEntries, in order to respect the overrides given by `replacement_sets`
  // and `normalized_additions`.
  //
  // Preconditions: sets defined by `replacement_sets` and
  // `normalized_additions` must be disjoint. `normalized_additions` must be
  // preprocessed such that no two addition sets intersect with the same public
  // set (i.e. they must be pre-unioned using a disjoint-set data structure).
  FirstPartySetsContextConfig ComputeConfig(
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          replacement_sets,
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          normalized_additions) const;

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
  // Represents the mapping of site -> entry, where keys are sites within
  // sets, and values are entries of the sets.
  base::flat_map<SchemefulSite, FirstPartySetEntry> entries_;

  // The site aliases. Used to normalize a given SchemefulSite into its
  // canonical representative, before looking it up in `entries_`.
  base::flat_map<SchemefulSite, SchemefulSite> aliases_;

  // Stores the customizations induced by the manually-specified set. May be
  // empty if no switch was provided.
  FirstPartySetsContextConfig manual_config_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os, const PublicSets& ps);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_ENTRY_H_
