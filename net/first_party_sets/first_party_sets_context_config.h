// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

#include "base/containers/flat_map.h"
#include "base/functional/function_ref.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo
namespace network::mojom {
class FirstPartySetsContextConfigDataView;
}  // namespace network::mojom

namespace net {

// This struct bundles together the customized settings to First-Party Sets
// info in the given network context.
class NET_EXPORT FirstPartySetsContextConfig {
 public:
  using OverrideSets =
      base::flat_map<SchemefulSite, absl::optional<FirstPartySetEntry>>;

  FirstPartySetsContextConfig();
  explicit FirstPartySetsContextConfig(OverrideSets customizations);

  FirstPartySetsContextConfig(FirstPartySetsContextConfig&& other);
  FirstPartySetsContextConfig& operator=(FirstPartySetsContextConfig&& other);

  ~FirstPartySetsContextConfig();

  FirstPartySetsContextConfig Clone() const;

  bool operator==(const FirstPartySetsContextConfig& other) const;

  bool empty() const { return customizations_.empty(); }

  // Finds an override for the given site, in this context. Returns:
  // - nullopt if no override was found.
  // - optional(nullopt) if an override was found, and it's a deletion.
  // - optional(optional(entry)) if an override was found, and it's a
  // modification/addition.
  absl::optional<absl::optional<FirstPartySetEntry>> FindOverride(
      const SchemefulSite& site) const;

  // Returns whether an override can be found for the given site in this
  // context.
  bool Contains(const SchemefulSite& site) const;

  // Synchronously iterate over all the override entries. Each iteration will be
  // invoked with the relevant site and the override that applies to it. The
  // override will be `nullopt` if it is a deletion, or `optional(entry)` if it
  // is a modification of an existing entry, or an addition.
  //
  // Returns early if any of the iterations returns false. Returns false if
  // iteration was incomplete; true if all iterations returned true. No
  // guarantees are made re: iteration order.
  bool ForEachCustomizationEntry(
      base::FunctionRef<bool(const SchemefulSite&,
                             const absl::optional<FirstPartySetEntry>&)> f)
      const;

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<
      network::mojom::FirstPartySetsContextConfigDataView,
      FirstPartySetsContextConfig>;

  const OverrideSets& customizations() const { return customizations_; }

  OverrideSets customizations_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_