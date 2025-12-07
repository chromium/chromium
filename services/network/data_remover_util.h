// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DATA_REMOVER_UTIL_H_
#define SERVICES_NETWORK_DATA_REMOVER_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "net/base/does_url_match_filter.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

// Convert the mojom::ClearDataFilter_Type enum to the net::UrlFilterType enum
// used by net::DoesUrlMatchFilter().
COMPONENT_EXPORT(NETWORK_SERVICE)
net::UrlFilterType ConvertClearDataFilterType(
    mojom::ClearDataFilter_Type filter_type);

// Returns a base::RepeatingClosure which takes a GURL as an argument returns
// true if net::DoesURLMatchFilter() returns true with the supplied conditions,
// and false otherwise.
COMPONENT_EXPORT(NETWORK_SERVICE)
base::RepeatingCallback<bool(const GURL&)> BindDoesUrlMatchFilter(
    mojom::ClearDataFilter_Type filter_type,
    const std::vector<url::Origin>& origins,
    const std::vector<std::string>& domains);

}  // namespace network

#endif  // SERVICES_NETWORK_DATA_REMOVER_UTIL_H_
