// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include "net/http/http_no_vary_search_data.h"

namespace network {

mojom::NoVarySearchPtr ParseNoVarySearch(
    const net::HttpResponseHeaders& headers) {
  mojom::NoVarySearchPtr no_vary_search;
  // See No-Vary-Search header structure at
  // https://github.com/WICG/nav-speculation/blob/main/no-vary-search.md#the-header
  const auto no_vary_search_data =
      net::HttpNoVarySearchData::ParseFromHeaders(headers);
  if (!no_vary_search_data.has_value())
    return no_vary_search;

  no_vary_search = network::mojom::NoVarySearch::New();
  no_vary_search->vary_on_key_order = no_vary_search_data->vary_on_key_order();
  if (no_vary_search_data->vary_by_default()) {
    no_vary_search->search_variance =
        mojom::SearchParamsVariance::NewNoVaryParams(std::vector<std::string>(
            no_vary_search_data->no_vary_params().begin(),
            no_vary_search_data->no_vary_params().end()));
    return no_vary_search;
  }
  no_vary_search->search_variance = mojom::SearchParamsVariance::NewVaryParams(
      std::vector<std::string>(no_vary_search_data->vary_params().begin(),
                               no_vary_search_data->vary_params().end()));
  return no_vary_search;
}

}  // namespace network
