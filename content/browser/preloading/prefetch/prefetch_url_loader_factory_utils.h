// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace content {

class BrowserContext;
class PrefetchRequest;

// Creates the common factory params used for prefetching, including by
// `CreatePrefetchURLLoaderFactory()` below.
network::mojom::URLLoaderFactoryParamsPtr CreatePrefetchURLLoaderFactoryParams(
    const base::UnguessableToken& network_restrictions_id);

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
                               const PrefetchRequest& prefetch_request,
                               scoped_refptr<network::SharedURLLoaderFactory>
                                   pre_prefetch_url_loader_factory = nullptr);

// Creates a `URLLoaderFactory` to be used for pre-prefetch requests.
// This must be called/created on the UI thread and then will be passed to/used
// on the `PrePrefetchServiceCore` sequence.
CONTENT_EXPORT mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreatePrePrefetchURLLoaderFactoryOnUI(BrowserContext* browser_context);

// Sets the URLLoaderFactory to intercept *network requests* just before being
// sent to a NetworkContext. This intercepts all prefetch-related requests:
// - Default network main-thread prefetches
// - Isolated network main-thread prefetches
// - Default network off-the-main-thread preprefetches
// but does NOT intercept PrePrefetch-backed prefetches, because the request
// doesn't go to the network.
//
// This is inserted at the last step of the URLLoaderFactory chain and not e.g.
// at `PrefetchService::GetURLLoaderFactoryForCurrentPrefetch()`, to fully
// exercise PrePrefetch-backed Prefetch code path.
//
// Note that this does not take ownership of `url_loader_factory`, and caller
// must keep ownership over the course of the test.
CONTENT_EXPORT void SetTerminalPrefetchURLLoaderFactoryForTesting(
    network::SharedURLLoaderFactory* url_loader_factory);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_FACTORY_UTILS_H_
