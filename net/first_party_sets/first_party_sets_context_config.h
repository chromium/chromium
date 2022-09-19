// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Modifies this config such that `aliases` are taken into account.
  //
  // This method requires that no override exists in this config for any key in
  // `aliases`; and that an override exists in this config for every value in
  // `aliases`.
  void IngestAliases(base::flat_map<SchemefulSite, SchemefulSite> aliases);

  const OverrideSets& customizations() const { return customizations_; }

 private:
  OverrideSets customizations_;
};

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SETS_CONTEXT_CONFIG_H_