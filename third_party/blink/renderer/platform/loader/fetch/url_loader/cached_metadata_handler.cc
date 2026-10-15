// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"

#include "base/time/time.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// This is a CachedMetadataSender implementation for normal responses.
class CachedMetadataSenderImpl : public CachedMetadataSender {
 public:
  CachedMetadataSenderImpl(const ResourceResponse&,
                           mojom::blink::CodeCacheType);
  ~CachedMetadataSenderImpl() override = default;

  void Send(CodeCacheHost*, base::span<const uint8_t>) override;
  bool IsServedFromCacheStorage() override { return false; }

 private:
  const KURL response_url_;
  const base::Time response_time_;
  const base::Time original_response_time_;
  const mojom::blink::CodeCacheType code_cache_type_;
};

CachedMetadataSenderImpl::CachedMetadataSenderImpl(
    const ResourceResponse& response,
    mojom::blink::CodeCacheType code_cache_type)
    : response_url_(response.CurrentRequestUrl()),
      response_time_(response.ResponseTime()),
      original_response_time_(response.OriginalResponseTime()),
      code_cache_type_(code_cache_type) {
  // WebAssembly always uses the site isolated code cache.
  DCHECK(!response.CacheStorageSideDataWriter().is_bound() ||
         code_cache_type_ == mojom::blink::CodeCacheType::kWebAssembly);
  DCHECK(!response.WasFetchedViaServiceWorker() ||
         response.IsServiceWorkerPassThrough() ||
         code_cache_type_ == mojom::blink::CodeCacheType::kWebAssembly);
}

void CachedMetadataSenderImpl::Send(CodeCacheHost* code_cache_host,
                                    base::span<const uint8_t> data) {
  if (!code_cache_host)
    return;
  // TODO(crbug.com/862940): This should use the Blink variant of the
  // interface.
  code_cache_host->get()->DidGenerateCacheableMetadata(
      code_cache_type_, response_url_, original_response_time_,
      mojo_base::BigBuffer(data));
}

// This is a CachedMetadataSender implementation that does nothing.
class NullCachedMetadataSender : public CachedMetadataSender {
 public:
  NullCachedMetadataSender() = default;
  ~NullCachedMetadataSender() override = default;

  void Send(CodeCacheHost*, base::span<const uint8_t>) override {}
  bool IsServedFromCacheStorage() override { return false; }
};

// This is a CachedMetadataSender implementation for responses that are served
// by a ServiceWorker from cache storage.
class ServiceWorkerCachedMetadataSender : public CachedMetadataSender {
 public:
  explicit ServiceWorkerCachedMetadataSender(const ResourceResponse&);
  ~ServiceWorkerCachedMetadataSender() override = default;

  void Send(CodeCacheHost*, base::span<const uint8_t>) override;
  bool IsServedFromCacheStorage() override { return true; }

 private:
  const mojo::SharedRemote<network::mojom::blink::CacheStorageSideDataWriter>
      cache_storage_side_data_writer_;
};

ServiceWorkerCachedMetadataSender::ServiceWorkerCachedMetadataSender(
    const ResourceResponse& response)
    : cache_storage_side_data_writer_(response.CacheStorageSideDataWriter()) {
  CHECK(cache_storage_side_data_writer_.is_bound());
}

void ServiceWorkerCachedMetadataSender::Send(CodeCacheHost*,
                                             base::span<const uint8_t> data) {
  if (!cache_storage_side_data_writer_.is_bound()) {
    return;
  }
  cache_storage_side_data_writer_->WriteSideData(mojo_base::BigBuffer(data));
}

// static
void CachedMetadataSender::SendToCodeCacheHost(
    CodeCacheHost* code_cache_host,
    mojom::blink::CodeCacheType code_cache_type,
    String url,
    base::Time response_time,
    const mojo::SharedRemote<network::mojom::blink::CacheStorageSideDataWriter>&
        cache_storage_side_data_writer,
    base::span<const uint8_t> data) {
  if (cache_storage_side_data_writer.is_bound()) {
    cache_storage_side_data_writer->WriteSideData(mojo_base::BigBuffer(data));
    return;
  }
  if (!code_cache_host) {
    return;
  }
  code_cache_host->get()->DidGenerateCacheableMetadata(
      code_cache_type, KURL(url), response_time, mojo_base::BigBuffer(data));
}

// static
std::unique_ptr<CachedMetadataSender> CachedMetadataSender::Create(
    const ResourceResponse& response,
    mojom::blink::CodeCacheType code_cache_type,
    scoped_refptr<const SecurityOrigin> requestor_origin) {
  if (!RuntimeEnabledFeatures::ServiceWorkerCodeCacheEnabled() &&
      response.WasFetchedViaServiceWorker()) {
    return std::make_unique<NullCachedMetadataSender>();
  }

  // Non-ServiceWorker scripts and passthrough SW responses use the site
  // isolated code cache.
  if (!response.WasFetchedViaServiceWorker() ||
      response.IsServiceWorkerPassThrough()) {
    return std::make_unique<CachedMetadataSenderImpl>(response,
                                                      code_cache_type);
  }

  // If the service worker provided a Response produced from cache_storage,
  // then we need to use a different code cache sender.
  CHECK_EQ(response.GetServiceWorkerResponseSource() ==
               network::mojom::FetchResponseSource::kCacheStorage,
           response.CacheStorageSideDataWriter().is_bound());
  if (response.CacheStorageSideDataWriter().is_bound()) {
    // TODO(leszeks): Check whether it's correct that |origin| can be nullptr.
    if (!requestor_origin) {
      return std::make_unique<NullCachedMetadataSender>();
    }
    // If the service worker uses a synthetic response (`new Response()`) or a
    // response fetched from a different URL, disable code caching.
    if (!response.HasMatchingServiceWorkerUrl()) {
      return std::make_unique<NullCachedMetadataSender>();
    }
    return std::make_unique<ServiceWorkerCachedMetadataSender>(response);
  }

  // If the service worker provides a synthetic `new Response()` or a
  // Response with a different URL then we disable code caching.  In the
  // synthetic case there is no actual backing storage.  In the case where
  // the service worker uses a Response with a different URL we don't
  // currently have a way to read the code cache since the we begin
  // loading it based on the request URL before the response is available.
  if (!response.IsServiceWorkerPassThrough()) {
    return std::make_unique<NullCachedMetadataSender>();
  }

  return std::make_unique<CachedMetadataSenderImpl>(response, code_cache_type);
}

bool ShouldUseIsolatedCodeCache(
    mojom::blink::RequestContextType request_context,
    const ResourceResponse& response) {
  if (!RuntimeEnabledFeatures::ServiceWorkerCodeCacheEnabled() &&
      response.WasFetchedViaServiceWorker()) {
    return false;
  }

  // Service worker script has its own code cache.
  if (request_context == mojom::blink::RequestContextType::SERVICE_WORKER)
    return false;

  // Also, we only support code cache for other service worker provided
  // resources when a direct pass-through fetch handler is used. If the service
  // worker synthesizes a new Response or provides a Response fetched from a
  // different URL, then do not use the code cache.
  // Also, responses coming from cache storage use a separate code cache
  // mechanism.
  return !response.WasFetchedViaServiceWorker() ||
         response.IsServiceWorkerPassThrough();
}

}  // namespace blink
