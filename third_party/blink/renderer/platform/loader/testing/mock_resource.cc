// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"

#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

namespace blink {

namespace {

class MockResourceFactory final : public NonTextResourceFactory {
 public:
  MockResourceFactory() : NonTextResourceFactory(ResourceType::kMock) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options) const override {
    return new MockResource(request, options);
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

// static
MockResource* MockResource::Create(const ResourceRequest& request) {
  ResourceLoaderOptions options;
  return new MockResource(request, options);
}

MockResource* MockResource::Create(const KURL& url) {
  ResourceRequest request(url);
  return Create(request);
}

MockResource::MockResource(const ResourceRequest& request,
                           const ResourceLoaderOptions& options)
    : Resource(request, ResourceType::kMock, options) {}

CachedMetadataHandler* MockResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  return new MockCacheHandler(std::move(send_callback));
}

void MockResource::SetSerializedCachedMetadata(const char* data, size_t size) {
  Resource::SetSerializedCachedMetadata(data, size);
  MockCacheHandler* cache_handler =
      static_cast<MockCacheHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->Set(data, size);
  }
}

void MockResource::SendCachedMetadata(const char* data, size_t size) {
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

void MockCacheHandler::Set(const char* data, size_t size) {
  data_.emplace();
  data_->Append(data, size);
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
