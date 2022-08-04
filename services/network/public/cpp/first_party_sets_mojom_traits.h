// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_

#include "net/cookies/first_party_set_entry.h"

#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/first_party_sets.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(FIRST_PARTY_SETS_MOJOM_TRAITS)
    StructTraits<network::mojom::FirstPartySetEntryDataView,
                 net::FirstPartySetEntry> {
  static net::SchemefulSite primary(const net::FirstPartySetEntry& e) {
    return e.primary();
  }

  static bool Read(network::mojom::FirstPartySetEntryDataView entry,
                   net::FirstPartySetEntry* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FIRST_PARTY_SETS_MOJOM_TRAITS_H_
