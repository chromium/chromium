// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
#define NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/version.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "net/first_party_sets/sets_mutation.h"

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
      base::Version public_sets_version,
      base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
      base::flat_map<SchemefulSite, SchemefulSite> aliases);

  GlobalFirstPartySets(GlobalFirstPartySets&&);
  GlobalFirstPartySets& operator=(GlobalFirstPartySets&&);

  ~GlobalFirstPartySets();

  bool operator==(const GlobalFirstPartySets& other) const;
  bool operator!=(const GlobalFirstPartySets& other) const;

  // Creates a clone of this instance.
  GlobalFirstPartySets Clone() const;

  // Returns a FirstPartySetsContextConfig that respects the overrides given by
  // `mutation`, relative to this instance's state.
  FirstPartySetsContextConfig ComputeConfig(const SetsMutation& mutation) const;

  // Returns the entry corresponding to the given `site`, if one exists.
  // Respects any customization/overlay specified by `config`. This is
  // semi-agnostic to scheme: it just cares whether the scheme is secure or
  // insecure.
  std::optional<FirstPartySetEntry> FindEntry(
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
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Modifies this instance such that it will respect the given
  // manually-specified set.
  void ApplyManuallySpecifiedSet(
      const LocalSetDeclaration& local_set_declaration);

  // Directly sets this instance's manual config. This is unsafe, because it
  // assumes that the config was computed by this instance (or one with
  // identical data), but cannot enforce that as a precondition.
  //
  // This must be public since at least one caller is above the //net layer, so
  // we can't refer to the caller's type here (and therefore can't "friend" it
  // and also can't use a base::Passkey).
  //
  // Must not be called if the manual config has already been set.
  void UnsafeSetManualConfig(FirstPartySetsContextConfig manual_config);

  // Synchronously iterate over all entries in the public sets (i.e. not
  // including any manual set entries). Returns early if any of the iterations
  // returns false. Returns false if iteration was incomplete; true if all
  // iterations returned true. No guarantees are made re: iteration order.
  // Aliases are included.
  bool ForEachPublicSetEntry(
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Synchronously iterate over the manual config. Returns early if any of the
  // iterations returns false. Returns false if iteration was incomplete; true
  // if all iterations returned true. No guarantees are made re: iteration
  // order.
  bool ForEachManualConfigEntry(
      base::FunctionRef<bool(const SchemefulSite&,
                             const FirstPartySetEntryOverride&)> f) const;

  // Synchronously iterate over all the effective entries (i.e. anything that
  // could be returned by `FindEntry` using this instance and `config`,
  // including the manual set, policy sets, and aliases). Returns early if any
  // of the iterations returns false. Returns false if iteration was incomplete;
  // true if all iterations returned true. No guarantees are made re: iteration
  // order.
  bool ForEachEffectiveSetEntry(
      const FirstPartySetsContextConfig& config,
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Whether the global sets are empty.
  bool empty() const { return entries_.empty() && manual_config_.empty(); }

  const base::Version& public_sets_version() const {
    return public_sets_version_;
  }

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<network::mojom::GlobalFirstPartySetsDataView,
                                   GlobalFirstPartySets>;

  friend NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                             const GlobalFirstPartySets& sets);

  GlobalFirstPartySets(
      base::Version public_sets_version,
      base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
      base::flat_map<SchemefulSite, SchemefulSite> aliases,
      FirstPartySetsContextConfig manual_config,
      base::flat_map<SchemefulSite, SchemefulSite> manual_aliases);

  // Same as the public version of FindEntry, but is allowed to omit the
  // `config` argument (i.e. pass nullptr instead of a reference).
  std::optional<FirstPartySetEntry> FindEntry(
      const SchemefulSite& site,
      const FirstPartySetsContextConfig* config) const;

  using FlattenedSets = base::flat_map<SchemefulSite, FirstPartySetEntry>;

  // Finds the existing primary sites whose sets are affected by a set of custom
  // additions.
  base::flat_map<SchemefulSite, FirstPartySetEntry>
  FindPrimariesAffectedByAdditions(const FlattenedSets& additions) const;

  // Finds the existing primary sites whose sets are affected by a set of custom
  // replacements.
  //
  // Returns the set of existing primaries that may become a singleton (along
  // with the sites in their set that have left due to the replacements); and
  // the set of existing primaries that themselves were in a replacement set.
  std::pair<base::flat_map<SchemefulSite, base::flat_set<SchemefulSite>>,
            base::flat_set<SchemefulSite>>
  FindPrimariesAffectedByReplacements(
      const FlattenedSets& replacements,
      const FlattenedSets& additions,
      const base::flat_map<SchemefulSite, FirstPartySetEntry>&
          addition_intersected_primaries) const;

  // Preprocesses a collection of "addition" sets, such that any sets that
  // transitively overlap (when taking the current `entries_` of this map, plus
  // the manual config, into account) are unioned together. I.e., this ensures
  // that at most one addition set intersects with any given global set.
  std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
  NormalizeAdditionSets(
      const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
          addition_sets) const;

  // Same as the public version of ForEachEffectiveSetEntry, but is allowed to
  // omit the `config` argument (i.e. pass nullptr instead of a reference).
  bool ForEachEffectiveSetEntry(
      const FirstPartySetsContextConfig* config,
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Iterates over the mappings in `manual_aliases_` and `aliases_` (skipping
  // entries of `aliases_` that are shadowed), invoking `f` for each `alias,
  // canonical` pair.
  void ForEachAlias(base::FunctionRef<void(const SchemefulSite&,
                                           const SchemefulSite&)> f) const;

  // Synchronously iterate over all the effective entries. Returns true iff all
  // the entries are valid.
  bool IsValid(const FirstPartySetsContextConfig* config = nullptr) const;

  // The version associated with the component_updater-provided public sets.
  // This may be invalid if the "First-Party Sets" component has not been
  // installed yet, or has been corrupted. Entries and aliases from invalid
  // components are ignored.
  base::Version public_sets_version_;

  // Represents the mapping of site -> entry, where keys are sites within sets,
  // and values are entries of the sets.
  base::flat_map<SchemefulSite, FirstPartySetEntry> entries_;

  // The site aliases. Used to normalize a given SchemefulSite into its
  // canonical representative, before looking it up in `entries_`.
  base::flat_map<SchemefulSite, SchemefulSite> aliases_;

  // Stores the customizations induced by the manually-specified set. May be
  // empty if no switch was provided.
  FirstPartySetsContextConfig manual_config_;

  // Stores the aliases contained in the manually-specified set. (Note that the
  // aliases are *also* stored in `manual_config_`.)
  base::flat_map<SchemefulSite, SchemefulSite> manual_aliases_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const GlobalFirstPartySets& sets);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
