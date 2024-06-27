// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
#define CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_

#include "content/common/content_export.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"

namespace content {

// For NetworkService glues:
// Navigation parameters that are necessary to set-up a subresource loader
// for the frame that is going to be created by the navigation.
// Passed from the browser to the renderer when the navigation commits when
// NetworkService or its glue code for relevant features is enabled.
struct CONTENT_EXPORT SubresourceLoaderParams {
  SubresourceLoaderParams();
  ~SubresourceLoaderParams();

  SubresourceLoaderParams(SubresourceLoaderParams&& other);
  SubresourceLoaderParams& operator=(SubresourceLoaderParams&& other);

  // When signed exchanges were prefetched in the previous page and were stored
  // to the PrefetchedSignedExchangeCache, and the main resource for the
  // navigation was served from the cache, |prefetched_signed_exchanges|
  // contains the all prefetched signed exchanges and they will be passed to the
  // renderer.
  std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>
      prefetched_signed_exchanges;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_SUBRESOURCE_LOADER_PARAMS_H_
