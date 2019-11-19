// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/prefetched_signed_exchange_cache_adapter.h"

#include "base/task/post_task.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/public/browser/browser_task_traits.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "storage/browser/blob/blob_builder_from_stream.h"
#include "storage/browser/blob/blob_data_handle.h"

namespace content {
namespace {

void AbortAndDeleteBlobBuilder(
    std::unique_ptr<storage::BlobBuilderFromStream> blob_builder) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    blob_builder->Abort();
    return;
  }

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&storage::BlobBuilderFromStream::Abort,
                                std::move(blob_builder)));
}

}  // namespace

PrefetchedSignedExchangeCacheAdapter::PrefetchedSignedExchangeCacheAdapter(
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    BrowserContext::BlobContextGetter blob_context_getter,
    const GURL& request_url,
    PrefetchURLLoader* prefetch_url_loader)
    : prefetched_signed_exchange_cache_(
          std::move(prefetched_signed_exchange_cache)),
      blob_context_getter_(std::move(blob_context_getter)),
      cached_exchange_(
          std::make_unique<PrefetchedSignedExchangeCache::Entry>()),
      prefetch_url_loader_(prefetch_url_loader) {
  cached_exchange_->SetOuterUrl(request_url);
}

PrefetchedSignedExchangeCacheAdapter::~PrefetchedSignedExchangeCacheAdapter() {
  if (blob_builder_from_stream_)
    AbortAndDeleteBlobBuilder(std::move(blob_builder_from_stream_));
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveOuterResponse(
    const network::ResourceResponseHead& response) {
  cached_exchange_->SetOuterResponse(
      std::make_unique<network::ResourceResponseHead>(response));
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveRedirect(
    const GURL& new_url,
    const base::Optional<net::SHA256HashValue> header_integrity,
    const base::Time& signature_expire_time) {
  DCHECK(header_integrity);
  DCHECK(!signature_expire_time.is_null());
  cached_exchange_->SetHeaderIntegrity(
      std::make_unique<net::SHA256HashValue>(*header_integrity));
  cached_exchange_->SetInnerUrl(new_url);
  cached_exchange_->SetSignatureExpireTime(signature_expire_time);
}

void PrefetchedSignedExchangeCacheAdapter::OnReceiveInnerResponse(
    const network::ResourceResponseHead& response) {
  std::unique_ptr<network::ResourceResponseHead> inner_response =
      std::make_unique<network::ResourceResponseHead>(response);
  inner_response->was_fetched_via_cache = true;
  inner_response->was_in_prefetch_cache = true;
  cached_exchange_->SetInnerResponse(std::move(inner_response));
}

void PrefetchedSignedExchangeCacheAdapter::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(cached_exchange_->inner_response());
  DCHECK(!cached_exchange_->completion_status());
  uint64_t length_hint = 0;
  if (cached_exchange_->inner_response()->content_length > 0) {
    length_hint = cached_exchange_->inner_response()->content_length;
  }
  blob_is_streaming_ = true;
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &PrefetchedSignedExchangeCacheAdapter::CreateBlobBuilderFromStream,
          weak_factory_.GetWeakPtr(), std::move(body), length_hint,
          blob_context_getter_),
      base::BindOnce(
          &PrefetchedSignedExchangeCacheAdapter::SetBlobBuilderFromStream,
          weak_factory_.GetWeakPtr()));
}

void PrefetchedSignedExchangeCacheAdapter::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  cached_exchange_->SetCompletionStatus(
      std::make_unique<network::URLLoaderCompletionStatus>(status));
  MaybeCallOnSignedExchangeStored();
}

void PrefetchedSignedExchangeCacheAdapter::StreamingBlobDone(
    storage::BlobBuilderFromStream* builder,
    std::unique_ptr<storage::BlobDataHandle> result) {
  blob_is_streaming_ = false;
  base::DeleteSoon(FROM_HERE, {BrowserThread::IO},
                   std::move(blob_builder_from_stream_));
  cached_exchange_->SetBlobDataHandle(std::move(result));
  MaybeCallOnSignedExchangeStored();
}

void PrefetchedSignedExchangeCacheAdapter::MaybeCallOnSignedExchangeStored() {
  if (!cached_exchange_->completion_status() || blob_is_streaming_) {
    return;
  }

  const network::URLLoaderCompletionStatus completion_status =
      *cached_exchange_->completion_status();

  // When SignedExchangePrefetchHandler failed to load the response (eg: invalid
  // signed exchange format), the inner response is not set. In that case, we
  // don't send the body to avoid the DCHECK() failure in URLLoaderClientImpl::
  // OnStartLoadingResponseBody().
  const bool should_send_body = cached_exchange_->inner_response().get();

  if (completion_status.error_code == net::OK &&
      cached_exchange_->blob_data_handle() &&
      cached_exchange_->blob_data_handle()->size()) {
    prefetched_signed_exchange_cache_->Store(std::move(cached_exchange_));
  }

  if (should_send_body) {
    if (!prefetch_url_loader_->SendEmptyBody())
      return;
  }
  prefetch_url_loader_->SendOnComplete(completion_status);
}

// static
std::unique_ptr<storage::BlobBuilderFromStream>
PrefetchedSignedExchangeCacheAdapter::CreateBlobBuilderFromStream(
    base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
    mojo::ScopedDataPipeConsumerHandle body,
    uint64_t length_hint,
    BrowserContext::BlobContextGetter blob_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto blob_builder_from_stream =
      std::make_unique<storage::BlobBuilderFromStream>(
          blob_context_getter.Run(), "" /* content_type */,
          "" /* content_disposition */,
          base::BindOnce(
              &PrefetchedSignedExchangeCacheAdapter::StreamingBlobDoneOnIO,
              std::move(adapter)));

  blob_builder_from_stream->Start(
      length_hint, std::move(body),
      mojo::NullAssociatedRemote() /*  progress_client */);
  return blob_builder_from_stream;
}

// static
void PrefetchedSignedExchangeCacheAdapter::SetBlobBuilderFromStream(
    base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
    std::unique_ptr<storage::BlobBuilderFromStream> blob_builder_from_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!adapter) {
    AbortAndDeleteBlobBuilder(std::move(blob_builder_from_stream));
    return;
  }

  adapter->blob_builder_from_stream_ = std::move(blob_builder_from_stream);
}

// static
void PrefetchedSignedExchangeCacheAdapter::StreamingBlobDoneOnIO(
    base::WeakPtr<PrefetchedSignedExchangeCacheAdapter> adapter,
    storage::BlobBuilderFromStream* builder,
    std::unique_ptr<storage::BlobDataHandle> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PrefetchedSignedExchangeCacheAdapter::StreamingBlobDone,
                     adapter, builder, std::move(result)));
}

}  // namespace content
