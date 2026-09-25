// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
#define NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/types/optional_ref.h"
#include "base/version.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
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
  GlobalFirstPartySets(base::Version public_sets_version,
                       FirstPartySetsContextConfig public_config);

  // Convenience factory for tests. CHECKs if inputs are not valid.
  static GlobalFirstPartySets CreateForTesting(
      base::Version public_sets_version,
      base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
      base::flat_map<SchemefulSite, SchemefulSite> aliases = {});

  GlobalFirstPartySets(GlobalFirstPartySets&&);
  GlobalFirstPartySets& operator=(GlobalFirstPartySets&&);

  ~GlobalFirstPartySets();

  bool operator==(const GlobalFirstPartySets& other) const;

  // Creates a clone of this instance.
  GlobalFirstPartySets Clone() const;

  // Returns the entry corresponding to the given `site`, if one exists.  This
  // is semi-agnostic to scheme: it just cares whether the scheme is secure or
  // insecure.
  std::optional<FirstPartySetEntry> FindEntry(const SchemefulSite& site) const;

  // Computes the First-Party Set metadata related to the given request context.
  FirstPartySetMetadata ComputeMetadata(
      const SchemefulSite& site,
      base::optional_ref<const SchemefulSite> top_frame_site) const;

  // Synchronously iterate over all entries in the public sets (i.e. not
  // including any manual set entries). Returns early if any of the iterations
  // returns false. Returns false if iteration was incomplete; true if all
  // iterations returned true. No guarantees are made re: iteration order.
  // Aliases are included.
  bool ForEachPublicSetEntry(
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Synchronously iterate over all the effective entries (i.e. anything that
  // could be returned by `FindEntry` using this instance, including aliases).
  // Returns early if any of the iterations returns false. Returns false if
  // iteration was incomplete; true if all iterations returned true. No
  // guarantees are made re: iteration order.
  bool ForEachEffectiveSetEntry(
      base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)>
          f) const;

  // Whether the global sets are empty.
  bool empty() const { return public_config_.empty(); }

  const base::Version& public_sets_version() const {
    return public_sets_version_;
  }

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<network::mojom::GlobalFirstPartySetsDataView,
                                   GlobalFirstPartySets>;

  friend NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                             const GlobalFirstPartySets& sets);

  // Synchronously iterate over all the effective entries. Returns true iff all
  // the entries are valid.
  bool IsValid() const;

  // Resolves an alias site into the canonical representative site, if possible.
  // The returned reference's lifetime is the *minimum* of the lifetimes of
  // `site` and `this`.
  const SchemefulSite& ResolveAlias(const SchemefulSite& site) const
      LIFETIME_BOUND;

  // The version associated with the component_updater-provided public sets.
  // This may be invalid if the "First-Party Sets" component has not been
  // installed yet, or has been corrupted. Entries and aliases from invalid
  // components are ignored.
  base::Version public_sets_version_;

  // Stores the sets defined by the public repository.
  FirstPartySetsContextConfig public_config_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const GlobalFirstPartySets& sets);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_GLOBAL_FIRST_PARTY_SETS_H_
