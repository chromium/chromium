// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
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
      net::FirstPartySetMetadata(context, base::OptionalOrNullptr(frame_entry),
                                 base::OptionalOrNullptr(top_frame_entry));

  return true;
}

}  // namespace mojo
