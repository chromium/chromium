// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// This is a CachedMetadataSender implementation for normal responses.
class CachedMetadataSenderImpl : public CachedMetadataSender {
 public:
  CachedMetadataSenderImpl(const ResourceResponse&,
                           blink::mojom::CodeCacheType);
  ~CachedMetadataSenderImpl() override = default;

  void Send(const uint8_t*, size_t) override;
  bool IsServedFromCacheStorage() override { return false; }

 private:
  const KURL response_url_;
  const base::Time response_time_;
  const blink::mojom::CodeCacheType code_cache_type_;
};

CachedMetadataSenderImpl::CachedMetadataSenderImpl(
    const ResourceResponse& response,
    blink::mojom::CodeCacheType code_cache_type)
    : response_url_(response.CurrentRequestUrl()),
      response_time_(response.ResponseTime()),
      code_cache_type_(code_cache_type) {
  // WebAssembly always uses the site isolated code cache.
  DCHECK(response.CacheStorageCacheName().IsNull() ||
         code_cache_type_ == blink::mojom::CodeCacheType::kWebAssembly);
  DCHECK(!response.WasFetchedViaServiceWorker() ||
         response.IsServiceWorkerPassThrough() ||
         code_cache_type_ == blink::mojom::CodeCacheType::kWebAssembly);
}

void CachedMetadataSenderImpl::Send(const uint8_t* data, size_t size) {
  Platform::Current()->CacheMetadata(code_cache_type_, response_url_,
                                     response_time_, data, size);
}

// This is a CachedMetadataSender implementation that does nothing.
class NullCachedMetadataSender : public CachedMetadataSender {
 public:
  NullCachedMetadataSender() = default;
  ~NullCachedMetadataSender() override = default;

  void Send(const uint8_t*, size_t) override {}
  bool IsServedFromCacheStorage() override { return false; }
};

// This is a CachedMetadataSender implementation for responses that are served
// by a ServiceWorker from cache storage.
class ServiceWorkerCachedMetadataSender : public CachedMetadataSender {
 public:
  ServiceWorkerCachedMetadataSender(const ResourceResponse&,
                                    scoped_refptr<const SecurityOrigin>);
  ~ServiceWorkerCachedMetadataSender() override = default;

  void Send(const uint8_t*, size_t) override;
  bool IsServedFromCacheStorage() override { return true; }

 private:
  const KURL response_url_;
  const base::Time response_time_;
  const String cache_storage_cache_name_;
  scoped_refptr<const SecurityOrigin> security_origin_;
};

ServiceWorkerCachedMetadataSender::ServiceWorkerCachedMetadataSender(
    const ResourceResponse& response,
    scoped_refptr<const SecurityOrigin> security_origin)
    : response_url_(response.CurrentRequestUrl()),
      response_time_(response.ResponseTime()),
      cache_storage_cache_name_(response.CacheStorageCacheName()),
      security_origin_(std::move(security_origin)) {
  DCHECK(!cache_storage_cache_name_.IsNull());
}

void ServiceWorkerCachedMetadataSender::Send(const uint8_t* data, size_t size) {
  Platform::Current()->CacheMetadataInCacheStorage(
      response_url_, response_time_, data, size,
      WebSecurityOrigin(security_origin_), cache_storage_cache_name_);
}

// static
std::unique_ptr<CachedMetadataSender> CachedMetadataSender::Create(
    const ResourceResponse& response,
    blink::mojom::CodeCacheType code_cache_type,
    scoped_refptr<const SecurityOrigin> requestor_origin) {
  // Non-ServiceWorker scripts and WebAssembly use the site isolated code cache.
  if (!response.WasFetchedViaServiceWorker() ||
      code_cache_type == blink::mojom::CodeCacheType::kWebAssembly) {
    return std::make_unique<CachedMetadataSenderImpl>(response,
                                                      code_cache_type);
  }

  // If the service worker provided a Response produced from cache_storage,
  // then we need to use a different code cache sender.
  if (!response.CacheStorageCacheName().IsNull()) {
    // TODO(leszeks): Check whether it's correct that |origin| can be nullptr.
    if (!requestor_origin) {
      return std::make_unique<NullCachedMetadataSender>();
    }
    return std::make_unique<ServiceWorkerCachedMetadataSender>(
        response, std::move(requestor_origin));
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

bool ShouldUseIsolatedCodeCache(mojom::RequestContextType request_context,
                                const ResourceResponse& response) {
  if (!RuntimeEnabledFeatures::IsolatedCodeCacheEnabled())
    return false;

  // WebAssembly always uses the site isolated code cache.
  if (response.MimeType() == "application/wasm")
    return true;

  // Service worker script has its own code cache.
  if (request_context == mojom::RequestContextType::SERVICE_WORKER)
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
