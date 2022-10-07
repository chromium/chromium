// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
#define NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_

#include <set>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo
namespace network::mojom {
class GlobalFirstPartySetsDataView;
}  // namespace network::mojom

namespace net {

class FirstPartySetMetadata;

// This class holds all of the info associated with the First-Party Sets known
// to this browser, after they've been parsed. This is suitable for plumbing
// from the browser process to the network service, or for answering queries.
// This class does not contain per-BrowserContext customizations, but supports
// application of those customizations.
class NET_EXPORT GlobalFirstPartySets {
 public:
  GlobalFirstPartySets();
  GlobalFirstPartySets(
      base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
      base::flat_map<SchemefulSite, SchemefulSite> aliases);
  GlobalFirstPartySets(
      base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
      base::flat_map<SchemefulSite, SchemefulSite> aliases,
      FirstPartySetsContextConfig manual_config);

  GlobalFirstPartySets(GlobalFirstPartySets&&);
  GlobalFirstPartySets& operator=(GlobalFirstPartySets&&);

  ~GlobalFirstPartySets();

  bool operator==(const GlobalFirstPartySets& other) const;
  bool operator!=(const GlobalFirstPartySets& other) const;

  // Creates a clone of this instance.
  GlobalFirstPartySets Clone() const;

  // Returns a FirstPartySetsContextConfig suitable for passing into
  // FindEntries, in order to respect the overrides given by `replacement_sets`
  // and `addition_sets`.
  //
  // Preconditions: sets defined by `replacement_sets` and
  // `addition_sets` must be disjoint.
  FirstPartySetsContextConfig ComputeConfig(
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          replacement_sets,
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          addition_sets) const;

  // Returns the entry corresponding to the given `site`, if one exists.
  // Respects any customization/overlay specified by `config`. This is
  // semi-agnostic to scheme: it just cares whether the scheme is secure or
  // insecure.
  absl::optional<FirstPartySetEntry> FindEntry(
      const SchemefulSite& site,
      const FirstPartySetsContextConfig& config) const;

  // Batched version of `FindEntry`. Where `FindEntry` would have returned
  // nullopt, this just omits from the result map.
  base::flat_map<SchemefulSite, FirstPartySetEntry> FindEntries(
      const base::flat_set<SchemefulSite>& sites,
      const FirstPartySetsContextConfig& config) const;

  // Computes the First-Party Set metadata related to the given request context.
  FirstPartySetMetadata ComputeMetadata(
      const SchemefulSite& site,
      const SchemefulSite* top_frame_site,
      const std::set<SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Modifies this instance such that it will respect the given
  // manually-specified set. `manual_entries` should contain entries for aliases
  // as well as "canonical" sites.
  void ApplyManuallySpecifiedSet(
      const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries);

  // Synchronously iterate over all entries in the public sets (i.e. not
  // including any manual set entries). Returns early if any of the iterations
  // returns false. Returns false if iteration was incomplete; true if all
  // iterations returned true. No guarantees are made re: iteration order.
  // Aliases are included.
  bool ForEachPublicSetEntry(
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Whether the global sets are empty.
  bool empty() const { return entries_.empty() && manual_config_.empty(); }

  const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_sets() const {
    return manual_sets_;
  }

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<network::mojom::GlobalFirstPartySetsDataView,
                                   GlobalFirstPartySets>;

  friend NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                             const GlobalFirstPartySets& sets);

  // Same as the public version of FindEntry, but is allowed to omit the
  // `config` argument (i.e. pass nullptr instead of a reference).
  absl::optional<FirstPartySetEntry> FindEntry(
      const SchemefulSite& site,
      const FirstPartySetsContextConfig* config) const;

  // Preprocesses a collection of "addition" sets, such that any sets that
  // transitively overlap (when taking the current `entries_` of this map, plus
  // the manual config, into account) are unioned together. I.e., this ensures
  // that at most one addition set intersects with any given global set.
  std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
  NormalizeAdditionSets(
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          addition_sets) const;

  // Returns whether `site` is same-party with `party_context`, and
  // `top_frame_site` (if it is not nullptr). That is, is `site`'s owner the
  // same as the owners of every member of `party_context` and of
  // `top_frame_site`? Note: if `site` is not a member of a First-Party Set,
  // then this returns false. If `top_frame_site` is nullptr, then it is
  // ignored.
  bool IsContextSamePartyWithSite(
      const SchemefulSite& site,
      const SchemefulSite* top_frame_site,
      const std::set<SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config) const;

  const base::flat_map<SchemefulSite, FirstPartySetEntry>& entries() const {
    return entries_;
  }

  const base::flat_map<SchemefulSite, SchemefulSite>& aliases() const {
    return aliases_;
  }

  const FirstPartySetsContextConfig& manual_config() const {
    return manual_config_;
  }

  // Represents the mapping of site -> entry, where keys are sites within sets,
  // and values are entries of the sets.
  base::flat_map<SchemefulSite, FirstPartySetEntry> entries_;

  // The site aliases. Used to normalize a given SchemefulSite into its
  // canonical representative, before looking it up in `entries_`.
  base::flat_map<SchemefulSite, SchemefulSite> aliases_;

  // Stores the customizations induced by the manually-specified set. May be
  // empty if no switch was provided.
  FirstPartySetsContextConfig manual_config_;

  // A map representing the manually-specified sets. Contains entries for
  // aliases as well as canonical sites.
  base::flat_map<SchemefulSite, FirstPartySetEntry> manual_sets_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const GlobalFirstPartySets& sets);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
