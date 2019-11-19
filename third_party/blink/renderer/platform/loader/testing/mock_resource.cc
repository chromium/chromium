// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"

#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

class MockResourceFactory final : public NonTextResourceFactory {
 public:
  MockResourceFactory() : NonTextResourceFactory(ResourceType::kMock) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options) const override {
    return MakeGarbageCollected<MockResource>(request, options);
  }
};

}  // namespace

// static
MockResource* MockResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher,
                                  ResourceClient* client) {
  params.SetRequestContext(mojom::RequestContextType::SUBRESOURCE);
  return static_cast<MockResource*>(
      fetcher->RequestResource(params, MockResourceFactory(), client));
}

MockResource::MockResource(const KURL& url)
    : MockResource(ResourceRequest(url)) {}
MockResource::MockResource(const ResourceRequest& request)
    : MockResource(request, ResourceLoaderOptions()) {}
MockResource::MockResource(const ResourceRequest& request,
                           const ResourceLoaderOptions& options)
    : Resource(request, ResourceType::kMock, options) {}

CachedMetadataHandler* MockResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  return MakeGarbageCollected<MockCacheHandler>(std::move(send_callback));
}

void MockResource::SetSerializedCachedMetadata(mojo_base::BigBuffer data) {
  // Resource ignores the cached metadata.
  Resource::SetSerializedCachedMetadata(mojo_base::BigBuffer());
  MockCacheHandler* cache_handler =
      static_cast<MockCacheHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->Set(data.data(), data.size());
  }
}

void MockResource::SendCachedMetadata(const uint8_t* data, size_t size) {
  MockCacheHandler* cache_handler =
      static_cast<MockCacheHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->Set(data, size);
    cache_handler->Send();
  }
}

MockCacheHandler* MockResource::CacheHandler() {
  return static_cast<MockCacheHandler*>(Resource::CacheHandler());
}

MockCacheHandler::MockCacheHandler(
    std::unique_ptr<CachedMetadataSender> send_callback)
    : send_callback_(std::move(send_callback)) {}

void MockCacheHandler::Set(const uint8_t* data, size_t size) {
  data_.emplace();
  data_->Append(data, SafeCast<wtf_size_t>(size));
}

void MockCacheHandler::ClearCachedMetadata(
    CachedMetadataHandler::CacheType cache_type) {
  if (cache_type == CachedMetadataHandler::kSendToPlatform) {
    Send();
  }
  data_.reset();
}

void MockCacheHandler::Send() {
  if (data_) {
    send_callback_->Send(data_->data(), data_->size());
  } else {
    send_callback_->Send(nullptr, 0);
  }
}

}  // namespace blink
