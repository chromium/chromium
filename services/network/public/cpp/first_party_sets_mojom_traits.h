// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "services/network/public/mojom/first_party_sets.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::SiteIndexDataView,
                 net::FirstPartySetEntry::SiteIndex> {
  static uint32_t value(const net::FirstPartySetEntry::SiteIndex& i) {
    return i.value();
  }

  static bool Read(network::mojom::SiteIndexDataView index,
                   net::FirstPartySetEntry::SiteIndex* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    EnumTraits<network::mojom::SiteType, net::SiteType> {
  static network::mojom::SiteType ToMojom(net::SiteType site_type);

  static bool FromMojom(network::mojom::SiteType site_type, net::SiteType* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetEntryDataView,
                 net::FirstPartySetEntry> {
  static net::SchemefulSite primary(const net::FirstPartySetEntry& e) {
    return e.primary();
  }

  static net::SiteType site_type(const net::FirstPartySetEntry& e) {
    return e.site_type();
  }

  static const std::optional<net::FirstPartySetEntry::SiteIndex>& site_index(
      const net::FirstPartySetEntry& e) {
    return e.site_index();
  }

  static bool Read(network::mojom::FirstPartySetEntryDataView entry,
                   net::FirstPartySetEntry* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetMetadataDataView,
                 net::FirstPartySetMetadata> {
  static std::optional<net::FirstPartySetEntry> frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.frame_entry();
  }

  static std::optional<net::FirstPartySetEntry> top_frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.top_frame_entry();
  }

  static bool Read(network::mojom::FirstPartySetMetadataDataView metadata,
                   net::FirstPartySetMetadata* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::GlobalFirstPartySetsDataView,
                 net::GlobalFirstPartySets> {
  static const base::Version& public_sets_version(
      const net::GlobalFirstPartySets& sets) {
    return sets.public_sets_version_;
  }

  static const base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>&
  sets(const net::GlobalFirstPartySets& sets) {
    return sets.entries_;
  }

  static const base::flat_map<net::SchemefulSite, net::SchemefulSite>& aliases(
      const net::GlobalFirstPartySets& sets) {
    return sets.aliases_;
  }

  static const net::FirstPartySetsContextConfig& manual_config(
      const net::GlobalFirstPartySets& sets) {
    return sets.manual_config_;
  }

  static const base::flat_map<net::SchemefulSite, net::SchemefulSite>&
  manual_aliases(const net::GlobalFirstPartySets& sets) {
    return sets.manual_aliases_;
  }

  static bool Read(network::mojom::GlobalFirstPartySetsDataView sets,
                   net::GlobalFirstPartySets* out_sets);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetEntryOverrideDataView,
                 net::FirstPartySetEntryOverride> {
  static const std::optional<net::FirstPartySetEntry>& entry(
      const net::FirstPartySetEntryOverride& override) {
    return override.entry_;
  }

  static bool Read(network::mojom::FirstPartySetEntryOverrideDataView override,
                   net::FirstPartySetEntryOverride* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetsContextConfigDataView,
                 net::FirstPartySetsContextConfig> {
  static const base::flat_map<net::SchemefulSite,
                              net::FirstPartySetEntryOverride>&
  customizations(const net::FirstPartySetsContextConfig& config) {
    return config.customizations_;
  }

  static bool Read(network::mojom::FirstPartySetsContextConfigDataView config,
                   net::FirstPartySetsContextConfig* out_config);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetsCacheFilterDataView,
                 net::FirstPartySetsCacheFilter> {
  static const base::flat_map<net::SchemefulSite, int64_t>& filter(
      const net::FirstPartySetsCacheFilter& cache_filter) {
    return cache_filter.filter_;
  }

  static int64_t browser_run_id(
      const net::FirstPartySetsCacheFilter& cache_filter) {
    return cache_filter.browser_run_id_;
  }

  static bool Read(
      network::mojom::FirstPartySetsCacheFilterDataView cache_filter,
      net::FirstPartySetsCacheFilter* out_cache_filter);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
