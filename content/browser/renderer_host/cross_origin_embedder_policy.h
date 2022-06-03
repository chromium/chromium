// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_EMBEDDER_POLICY_H_
#define CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_EMBEDDER_POLICY_H_

#include "services/network/public/cpp/cross_origin_embedder_policy.h"

class GURL;
namespace network {
namespace mojom {
class URLResponseHead;
}  // namespace mojom
}  // namespace network

namespace content {

// Return the COEP policy from a context's main response. The context can be one
// of Document, DedicatedWorker, SharedWorker, or ServiceWorker.
// This check for OriginTrial in the response and can downgrade the result to
// COEP:unsafe-none when the feature shouldn't be enabled for this context.
network::CrossOriginEmbedderPolicy CoepFromMainResponse(
    const GURL& url,
    const network::mojom::URLResponseHead* main_response);
}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_EMBEDDER_POLICY_H_
