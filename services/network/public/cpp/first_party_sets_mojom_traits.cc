// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "base/version.h"
#include "mojo/public/cpp/base/version_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/same_party_context.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/first_party_sets.mojom-shared.h"

namespace mojo {

bool StructTraits<network::mojom::SiteIndexDataView,
                  net::FirstPartySetEntry::SiteIndex>::
    Read(network::mojom::SiteIndexDataView index,
         net::FirstPartySetEntry::SiteIndex* out) {
  *out = net::FirstPartySetEntry::SiteIndex(index.value());
  return true;
}

bool EnumTraits<network::mojom::SiteType, net::SiteType>::FromMojom(
    network::mojom::SiteType site_type,
    net::SiteType* out) {
  switch (site_type) {
    case network::mojom::SiteType::kPrimary:
      *out = net::SiteType::kPrimary;
      return true;
    case network::mojom::SiteType::kAssociated:
      *out = net::SiteType::kAssociated;
      return true;
    case network::mojom::SiteType::kService:
      *out = net::SiteType::kService;
      return true;
  }
  return false;
}

network::mojom::SiteType
EnumTraits<network::mojom::SiteType, net::SiteType>::ToMojom(
    net::SiteType site_type) {
  switch (site_type) {
    case net::SiteType::kPrimary:
      return network::mojom::SiteType::kPrimary;
    case net::SiteType::kAssociated:
      return network::mojom::SiteType::kAssociated;
    case net::SiteType::kService:
      return network::mojom::SiteType::kService;
  }
  NOTREACHED();
  return network::mojom::SiteType::kPrimary;
}

bool StructTraits<network::mojom::FirstPartySetEntryDataView,
                  net::FirstPartySetEntry>::
    Read(network::mojom::FirstPartySetEntryDataView entry,
         net::FirstPartySetEntry* out) {
  net::SchemefulSite primary;
  if (!entry.ReadPrimary(&primary))
    return false;

  net::SiteType site_type;
  if (!entry.ReadSiteType(&site_type))
    return false;

  absl::optional<net::FirstPartySetEntry::SiteIndex> site_index;
  if (!entry.ReadSiteIndex(&site_index))
    return false;

  *out = net::FirstPartySetEntry(primary, site_type, site_index);
  return true;
}

bool EnumTraits<network::mojom::SamePartyCookieContextType,
                net::SamePartyContext::Type>::
    FromMojom(network::mojom::SamePartyCookieContextType context_type,
              net::SamePartyContext::Type* out) {
  switch (context_type) {
    case network::mojom::SamePartyCookieContextType::kCrossParty:
      *out = net::SamePartyContext::Type::kCrossParty;
      return true;
    case network::mojom::SamePartyCookieContextType::kSameParty:
      *out = net::SamePartyContext::Type::kSameParty;
      return true;
  }
  return false;
}

network::mojom::SamePartyCookieContextType
EnumTraits<network::mojom::SamePartyCookieContextType,
           net::SamePartyContext::Type>::ToMojom(net::SamePartyContext::Type
                                                     context_type) {
  switch (context_type) {
    case net::SamePartyContext::Type::kCrossParty:
      return network::mojom::SamePartyCookieContextType::kCrossParty;
    case net::SamePartyContext::Type::kSameParty:
      return network::mojom::SamePartyCookieContextType::kSameParty;
  }
  NOTREACHED();
  return network::mojom::SamePartyCookieContextType::kCrossParty;
}

bool StructTraits<network::mojom::SamePartyContextDataView,
                  net::SamePartyContext>::
    Read(network::mojom::SamePartyContextDataView context,
         net::SamePartyContext* out) {
  net::SamePartyContext::Type context_type;
  if (!context.ReadContextType(&context_type))
    return false;

  *out = net::SamePartyContext(context_type);
  return true;
}

bool StructTraits<network::mojom::FirstPartySetMetadataDataView,
                  net::FirstPartySetMetadata>::
    Read(network::mojom::FirstPartySetMetadataDataView metadata,
         net::FirstPartySetMetadata* out_metadata) {
  net::SamePartyContext context;
  if (!metadata.ReadContext(&context))
    return false;

  absl::optional<net::FirstPartySetEntry> frame_entry;
  if (!metadata.ReadFrameEntry(&frame_entry))
    return false;

  absl::optional<net::FirstPartySetEntry> top_frame_entry;
  if (!metadata.ReadTopFrameEntry(&top_frame_entry))
    return false;

  *out_metadata =
      net::FirstPartySetMetadata(context, base::OptionalToPtr(frame_entry),
                                 base::OptionalToPtr(top_frame_entry));

  return true;
}

bool StructTraits<network::mojom::GlobalFirstPartySetsDataView,
                  net::GlobalFirstPartySets>::
    Read(network::mojom::GlobalFirstPartySetsDataView sets,
         net::GlobalFirstPartySets* out_sets) {
  base::Version public_sets_version;
  if (!sets.ReadPublicSetsVersion(&public_sets_version))
    return false;

  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry> entries;
  if (public_sets_version.IsValid() && !sets.ReadSets(&entries))
    return false;

  base::flat_map<net::SchemefulSite, net::SchemefulSite> aliases;
  if (public_sets_version.IsValid() && !sets.ReadAliases(&aliases))
    return false;

  if (!base::ranges::all_of(aliases, [&](const auto& pair) {
        return entries.contains(pair.second);
      })) {
    return false;
  }

  net::FirstPartySetsContextConfig manual_config;
  if (!sets.ReadManualConfig(&manual_config))
    return false;

  *out_sets = net::GlobalFirstPartySets(std::move(public_sets_version), entries,
                                        aliases, std::move(manual_config));

  return true;
}

bool StructTraits<network::mojom::FirstPartySetEntryOverrideDataView,
                  net::FirstPartySetEntryOverride>::
    Read(network::mojom::FirstPartySetEntryOverrideDataView override,
         net::FirstPartySetEntryOverride* out) {
  absl::optional<net::FirstPartySetEntry> entry;
  if (!override.ReadEntry(&entry))
    return false;

  if (entry.has_value()) {
    *out = net::FirstPartySetEntryOverride(entry.value());
  } else {
    *out = net::FirstPartySetEntryOverride();
  }
  return true;
}

bool StructTraits<network::mojom::FirstPartySetsContextConfigDataView,
                  net::FirstPartySetsContextConfig>::
    Read(network::mojom::FirstPartySetsContextConfigDataView config,
         net::FirstPartySetsContextConfig* out_config) {
  base::flat_map<net::SchemefulSite, net::FirstPartySetEntryOverride>
      customizations;
  if (!config.ReadCustomizations(&customizations))
    return false;

  *out_config = net::FirstPartySetsContextConfig(std::move(customizations));

  return true;
}

bool StructTraits<network::mojom::FirstPartySetsCacheFilterDataView,
                  net::FirstPartySetsCacheFilter>::
    Read(network::mojom::FirstPartySetsCacheFilterDataView cache_filter,
         net::FirstPartySetsCacheFilter* out_cache_filter) {
  base::flat_map<net::SchemefulSite, int64_t> filter;
  if (!cache_filter.ReadFilter(&filter))
    return false;

  *out_cache_filter = net::FirstPartySetsCacheFilter(
      std::move(filter), std::move(cache_filter.browser_run_id()));

  return true;
}

}  // namespace mojo
