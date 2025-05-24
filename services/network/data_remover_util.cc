// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/data_remover_util.h"

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/base/does_url_match_filter.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"

namespace network {

net::UrlFilterType ConvertClearDataFilterType(
    mojom::ClearDataFilter_Type filter_type) {
  return filter_type == mojom::ClearDataFilter_Type::DELETE_MATCHES
             ? net::UrlFilterType::kTrueIfMatches
             : net::UrlFilterType::kFalseIfMatches;
}

base::RepeatingCallback<bool(const GURL&)> BindDoesUrlMatchFilter(
    mojom::ClearDataFilter_Type filter_type,
    const std::vector<url::Origin>& origins,
    const std::vector<std::string>& domains) {
  base::flat_set<url::Origin> origin_set(origins.begin(), origins.end());
  base::flat_set<std::string> domain_set(domains.begin(), domains.end());
  return base::BindRepeating(&net::DoesUrlMatchFilter,
                             ConvertClearDataFilterType(filter_type),
                             std::move(origin_set), std::move(domain_set));
}

}  // namespace network
