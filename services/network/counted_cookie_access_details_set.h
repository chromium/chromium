// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COUNTED_COOKIE_ACCESS_DETAILS_SET_H_
#define SERVICES_NETWORK_COUNTED_COOKIE_ACCESS_DETAILS_SET_H_

#include <set>
#include <tuple>

#include "base/component_export.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"

namespace network {

// The CountedCookieAccessDetailsPtr, the set and the comparator are all aimed
// at enabling deduplication of CookieAccessDetails notifications in the
// RestrictedCookieManager. The associated index is _not_ the count (this is
// stored on the details themselves), but rather an index used for sorting the
// deduplicated details.

using CountedCookieAccessDetailsPtr =
    std::pair<mojom::CookieAccessDetailsPtr, std::unique_ptr<size_t>>;

struct COMPONENT_EXPORT(NETWORK_SERVICE) CookieAccessDetailsPtrComparer {
  bool operator()(const CountedCookieAccessDetailsPtr& lhs,
                  const CountedCookieAccessDetailsPtr& rhs) const;
};

using CookieAccessDetails =
    std::set<CountedCookieAccessDetailsPtr, CookieAccessDetailsPtrComparer>;

struct COMPONENT_EXPORT(NETWORK_SERVICE) CookieWithAccessResultComparer {
  bool operator()(
      const net::CookieWithAccessResult& cookie_with_access_result1,
      const net::CookieWithAccessResult& cookie_with_access_result2) const;
};

COMPONENT_EXPORT(NETWORK_SERVICE)
bool CookieAccessDetailsPrecede(const mojom::CookieAccessDetailsPtr& lhs,
                                const mojom::CookieAccessDetailsPtr& rhs);

}  // namespace network

#endif  // SERVICES_NETWORK_COUNTED_COOKIE_ACCESS_DETAILS_SET_H_
