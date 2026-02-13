// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace content {

class PrefetchRequest;

// Creates a `URLLoaderFactory` (which is finally connected to
// `network_context`) to be used for the prefetch. The configurations like
// isolated network context and prefetch proxy are reflected in
// `network_context` and don't affect the rest of
// `CreatePrefetchURLLoaderFactory()`. See
// https://docs.google.com/document/d/12cjL04kEjtLs5hSthgg8o_UK-LeS_RcF992Z2vCp7Vk/edit?usp=sharing
// for illustration of the current/possibly planned status of how
// `URLLoaderFactory` is created around prefetching.
CONTENT_EXPORT scoped_refptr<network::SharedURLLoaderFactory>
CreatePrefetchURLLoaderFactory(network::mojom::NetworkContext* network_context,
                               const PrefetchRequest& prefetch_request);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_
