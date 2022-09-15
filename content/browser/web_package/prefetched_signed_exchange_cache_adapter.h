// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/system/data_pipe.h"

class GURL;

namespace storage {
class BlobBuilderFromStream;
class BlobDataHandle;
}  // namespace storage

namespace content {
class PrefetchURLLoader;

// This class is used by PrefetchURLLoader to store the prefetched and verified
// signed exchanges to the PrefetchedSignedExchangeCache.
class PrefetchedSignedExchangeCacheAdapter {
 public:
  PrefetchedSignedExchangeCacheAdapter(
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      BrowserContext::BlobContextGetter blob_context_getter,
      const GURL& request_url,
      PrefetchURLLoader* prefetch_url_loader);

  PrefetchedSignedExchangeCacheAdapter(
      const PrefetchedSignedExchangeCacheAdapter&) = delete;
  PrefetchedSignedExchangeCacheAdapter& operator=(
      const PrefetchedSignedExchangeCacheAdapter&) = delete;

  ~PrefetchedSignedExchangeCacheAdapter();

  void OnReceiveSignedExchange(
      std::unique_ptr<PrefetchedSignedExchangeCacheEntry> entry);
  void OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

 private:
  void StreamingBlobDone(storage::BlobBuilderFromStream* builder,
                         std::unique_ptr<storage::BlobDataHandle> result);

  void MaybeCallOnSignedExchangeStored();

  // Creates and starts the blob builder.
  static std::unique_ptr<storage::BlobBuilderFromStream>
  CreateBlobBuilderFromStream(
      base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
      mojo::ScopedDataPipeConsumerHandle body,
      uint64_t length_hint,
      BrowserContext::BlobContextGetter blob_context_getter);

  // Sets |blob_builder_from_stream| on |adapter|. If |adapter| is no longer
  // valid, aborts the blob builder.
  static void SetBlobBuilderFromStream(
      base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
      std::unique_ptr<storage::BlobBuilderFromStream> blob_builder_from_stream);

  // Calls StreamingBlobDone() on the correct thread.
  static void StreamingBlobDoneOnIO(
      base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
      storage::BlobBuilderFromStream* builder,
      std::unique_ptr<storage::BlobDataHandle> result);

  // Holds the prefetched signed exchanges which will be used in the next
  // navigation. This is shared with RenderFrameHostImpl that created this.
  const scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Used to create a BlobDataHandle from a DataPipe of signed exchange's inner
  // response body to store to |prefetched_signed_exchange_cache_|.
  BrowserContext::BlobContextGetter blob_context_getter_;

  // A temporary entry of PrefetchedSignedExchangeCache, which will be stored
  // to |prefetched_signed_exchange_cache_|.
  std::unique_ptr<PrefetchedSignedExchangeCacheEntry> cached_exchange_;

  // Used to create a BlobDataHandle from a DataPipe of signed exchange's inner
  // response body. This should only be accessed on the IO thread.
  std::unique_ptr<storage::BlobBuilderFromStream> blob_builder_from_stream_;
  bool blob_is_streaming_ = false;

  // |prefetch_url_loader_| owns |this|.
  raw_ptr<PrefetchURLLoader> prefetch_url_loader_;

  base::WeakPtrFactory<PrefetchedSignedExchangeCacheAdapter> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_PREFETCHED_SIGNED_EXCHANGE_CACHE_ADAPTER_H_
