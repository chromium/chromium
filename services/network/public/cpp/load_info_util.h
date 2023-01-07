// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOAD_INFO_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOAD_INFO_UTIL_H_

#include "base/component_export.h"

namespace network {

namespace mojom {
class LoadInfo;
}

// This function returns true if the LoadInfo of |a| is "more interesting"
// than the LoadInfo of |b|.  The load that is currently sending the larger
// request body is considered more interesting.  If neither request is
// sending a body (Neither request has a body, or any request that has a body
// is not currently sending the body), the request that is further along is
// considered more interesting.
//
// This takes advantage of the fact that the load states are an enumeration
// listed in the order in which they usually occur during the lifetime of a
// request, so states with larger numeric values are generally further along
// toward completion.
//
// For example, by this measure "tranferring data" is a more interesting state
// than "resolving host" because when transferring data something is being
// done that corresponds to changes that the user might observe, whereas
// waiting for a host name to resolve implies being stuck.
COMPONENT_EXPORT(NETWORK_CPP)
bool LoadInfoIsMoreInteresting(const mojom::LoadInfo& a,
                               const mojom::LoadInfo& b);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOAD_INFO_UTIL_H_