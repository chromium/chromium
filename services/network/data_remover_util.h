// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DATA_REMOVER_UTIL_H_
#define SERVICES_NETWORK_DATA_REMOVER_UTIL_H_

#include <set>
#include <string>

#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// A utility function to determine if a given |url| must be removed for a
// dataset.
// `filter_type` indicates if a given match should be keeped / deleted.
// `origins` set of url::Origins to match with
// `domains` set of strings representing registrable domains to match with
// Returns true if |url| matches any of the origins or domains, and
// filter_type == DELETE_MATCHES, or |url| doesn't match any of the origins or
// domains and filter_type == KEEP_MATCHES.
COMPONENT_EXPORT(NETWORK_SERVICE)
bool DoesUrlMatchFilter(mojom::ClearDataFilter_Type filter_type,
                        const std::set<url::Origin>& origins,
                        const std::set<std::string>& domains,
                        const GURL& url);

}  // namespace network

#endif  // SERVICES_NETWORK_DATA_REMOVER_UTIL_H_
