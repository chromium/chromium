// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/first_party_sets_mojom_traits.h"

#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "services/network/public/cpp/schemeful_site_mojom_traits.h"
#include "services/network/public/mojom/first_party_sets.mojom-shared.h"

namespace mojo {

bool StructTraits<network::mojom::FirstPartySetEntryDataView,
                  net::FirstPartySetEntry>::
    Read(network::mojom::FirstPartySetEntryDataView entry,
         net::FirstPartySetEntry* out) {
  net::SchemefulSite primary;
  if (!entry.ReadPrimary(&primary))
    return false;

  *out = net::FirstPartySetEntry(primary);
  return true;
}

}  // namespace mojo
