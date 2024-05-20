// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_CODE_CACHE_HOST_PROXY_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_CODE_CACHE_HOST_PROXY_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "url/gurl.h"

namespace content {

// Proxy CodeCacheHost, to limit the requests that a shared storage worklet can
// make. It also implicitly adds a scope -- requests are only allowed during the
// lifetime of `SharedStorageCodeCacheHostProxy`.
class CONTENT_EXPORT SharedStorageCodeCacheHostProxy
    : public blink::mojom::CodeCacheHost {
 public:
  explicit SharedStorageCodeCacheHostProxy(
      mojo::PendingRemote<blink::mojom::CodeCacheHost> actual_code_cache_host,
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver,
      const GURL& script_url);
  ~SharedStorageCodeCacheHostProxy() override;

  void DidGenerateCacheableMetadata(blink::mojom::CodeCacheType cache_type,
                                    const GURL& url,
                                    base::Time expected_response_time,
                                    mojo_base::BigBuffer data) override;

  void FetchCachedCode(blink::mojom::CodeCacheType cache_type,
                       const GURL& url,
                       FetchCachedCodeCallback callback) override;

  void ClearCodeCacheEntry(blink::mojom::CodeCacheType cache_type,
                           const GURL& url) override;

  void DidGenerateCacheableMetadataInCacheStorage(
      const GURL& url,
      base::Time expected_response_time,
      mojo_base::BigBuffer data,
      const std::string& cache_storage_cache_name) override;

 private:
  mojo::Remote<blink::mojom::CodeCacheHost> actual_code_cache_host_;
  mojo::Receiver<blink::mojom::CodeCacheHost> receiver_;
  const GURL script_url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_CODE_CACHE_HOST_PROXY_H_
