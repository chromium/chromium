// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_TIMING_UTILS_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_TIMING_UTILS_H_

#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-forward.h"
#include "url/origin.h"

namespace content {

blink::mojom::ResourceTimingInfoPtr GenerateResourceTimingForNavigation(
    const url::Origin& parent_origin,
    const blink::mojom::CommonNavigationParams& common_params,
    const blink::mojom::CommitNavigationParams& commit_params,
    const network::mojom::URLResponseHead& response_head,
    blink::mojom::RequestContextType context_type);

}  // namespace content

#endif
