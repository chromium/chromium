// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_code_cache_host_proxy.h"

#include "base/logging.h"

namespace content {

SharedStorageCodeCacheHostProxy::SharedStorageCodeCacheHostProxy(
    mojo::PendingRemote<blink::mojom::CodeCacheHost> actual_code_cache_host,
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
    const GURL& script_url)
    : actual_code_cache_host_(std::move(actual_code_cache_host)),
      receiver_(this, std::move(receiver)),
      script_url_(script_url) {}

SharedStorageCodeCacheHostProxy::~SharedStorageCodeCacheHostProxy() = default;

void SharedStorageCodeCacheHostProxy::DidGenerateCacheableMetadata(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    base::Time expected_response_time,
    mojo_base::BigBuffer data) {
  // The only URL we expect to use the code cache is the worklet's script URL.
  if (script_url_ != url) {
    receiver_.ReportBadMessage("Unexpected request url");
    return;
  }
  actual_code_cache_host_->DidGenerateCacheableMetadata(
      cache_type, url, expected_response_time, std::move(data));
}

void SharedStorageCodeCacheHostProxy::FetchCachedCode(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    FetchCachedCodeCallback callback) {
  // The only URL we expect to use the code cache is the worklet's script URL.
  if (script_url_ != url) {
    receiver_.ReportBadMessage("Unexpected request url");
    return;
  }
  actual_code_cache_host_->FetchCachedCode(cache_type, url,
                                           std::move(callback));
}

void SharedStorageCodeCacheHostProxy::ClearCodeCacheEntry(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url) {
  // The only URL we expect to use the code cache is the worklet's script URL.
  if (script_url_ != url) {
    receiver_.ReportBadMessage("Unexpected request url");
    return;
  }
  actual_code_cache_host_->ClearCodeCacheEntry(cache_type, url);
}

void SharedStorageCodeCacheHostProxy::
    DidGenerateCacheableMetadataInCacheStorage(
        const GURL& url,
        base::Time expected_response_time,
        mojo_base::BigBuffer data,
        const std::string& cache_storage_cache_name) {
  // CacheStorage writes require a Service Worker context, which is inapplicable
  // here for Shared Storage Worklets. Thus, this method should never invoke.
  receiver_.ReportBadMessage(
      "Unexpected call of DidGenerateCacheableMetadataInCacheStorage");
}

}  // namespace content
