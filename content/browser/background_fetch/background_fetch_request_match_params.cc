// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_match_params.h"

#include <utility>

namespace content {

BackgroundFetchRequestMatchParams::BackgroundFetchRequestMatchParams(
    blink::mojom::FetchAPIRequestPtr request_to_match,
    blink::mojom::CacheQueryOptionsPtr cache_query_options,
    bool match_all)
    : request_to_match_(std::move(request_to_match)),
      cache_query_options_(std::move(cache_query_options)),
      match_all_(match_all) {}

BackgroundFetchRequestMatchParams::BackgroundFetchRequestMatchParams() =
    default;
BackgroundFetchRequestMatchParams::~BackgroundFetchRequestMatchParams() =
    default;

}  // namespace content
