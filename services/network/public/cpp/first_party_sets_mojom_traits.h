// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_

#include "net/cookies/first_party_set_entry.h"

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
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

  static const absl::optional<net::FirstPartySetEntry::SiteIndex>& site_index(
      const net::FirstPartySetEntry& e) {
    return e.site_index();
  }

  static bool Read(network::mojom::FirstPartySetEntryDataView entry,
                   net::FirstPartySetEntry* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    EnumTraits<network::mojom::SamePartyCookieContextType,
               net::SamePartyContext::Type> {
  static network::mojom::SamePartyCookieContextType ToMojom(
      net::SamePartyContext::Type context_type);

  static bool FromMojom(network::mojom::SamePartyCookieContextType context_type,
                        net::SamePartyContext::Type* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::SamePartyContextDataView,
                 net::SamePartyContext> {
  static net::SamePartyContext::Type context_type(
      const net::SamePartyContext& s) {
    return s.context_type();
  }

  static bool Read(network::mojom::SamePartyContextDataView bundle,
                   net::SamePartyContext* out);
};

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetMetadataDataView,
                 net::FirstPartySetMetadata> {
  static net::SamePartyContext context(const net::FirstPartySetMetadata& m) {
    return m.context();
  }

  static absl::optional<net::FirstPartySetEntry> frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.frame_entry();
  }

  static absl::optional<net::FirstPartySetEntry> top_frame_entry(
      const net::FirstPartySetMetadata& m) {
    return m.top_frame_entry();
  }

  static bool Read(network::mojom::FirstPartySetMetadataDataView metadata,
                   net::FirstPartySetMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
