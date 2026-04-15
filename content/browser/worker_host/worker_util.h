// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_UTIL_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

// Returns the StorageKey that should be used for a worker (dedicated or shared)
// with the given `script_url`.
//
// Workers created from data: URLs are required by the HTML spec (10.2.6.2
// Script settings for workers, Step 3) to have a unique opaque origin. When
// the kDataUrlWorkerOpaqueOrigin feature is enabled, this function returns an
// opaque StorageKey derived from the
// `creator_storage_key`.
//
// This opaque origin is important for correctly partitioning storage and other
// APIs. For more context on storage partitioning, see:
// - https://privacysandbox.google.com/cookies/storage-partitioning/
// -
// https://github.com/wanderview/quota-storage-partitioning/blob/main/explainer.md
CONTENT_EXPORT blink::StorageKey CalculateWorkerStorageKey(
    const GURL& script_url,
    const blink::StorageKey& creator_storage_key,
    bool is_opaque_origin_enabled);

// Returns the origin that should be used by the worker on the renderer side.
// This will almost always be the same as the origin of `worker_storage_key`,
// except in the case of data: URL workers when the kDataUrlWorkerOpaqueOrigin
// feature is disabled.
// TODO(crbug.com/40051700): Remove this when the feature is enabled by default.
CONTENT_EXPORT url::Origin CalculateWorkerRendererOrigin(
    const GURL& script_url,
    const blink::StorageKey& worker_storage_key,
    bool is_opaque_origin_enabled);

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_UTIL_H_
