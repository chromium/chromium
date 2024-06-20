// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/subresource_signed_exchange_url_loader_factory.h"

#include <memory>
#include <utility>

#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// This is a regression test for https://crbug.com/345261068.
// (Note that the repro may require `enable_dangling_raw_ptr_checks` in
// `args.gn`. See also `//docs/dangling_ptr_guide.md`)
TEST(SubresourceSignedExchangeURLLoaderFactoryTest,
     ShortlivedFactoryAndLonglivedLoader) {
  BrowserTaskEnvironment task_environment;
  GURL inner_url("https://foo.com/outer/inner");
  auto initiator_origin = url::Origin::Create(GURL("https://foo.com"));

  // Construct a minimal `PrefetchedSignedExchangeCacheEntry` needed to make the
  // unit tests work.
  auto entry = std::make_unique<PrefetchedSignedExchangeCacheEntry>();
  auto status = std::make_unique<network::URLLoaderCompletionStatus>();
  entry->SetCompletionStatus(std::move(status));
  entry->SetOuterUrl(GURL("https://foo.com/outer"));
  entry->SetInnerUrl(inner_url);
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\nContent-type: application/custom\n\n"));
  auto outer_response = network::mojom::URLResponseHead::New();
  outer_response->headers = headers;
  entry->SetOuterResponse(std::move(outer_response));
  auto header_integrity = std::make_unique<net::SHA256HashValue>();
  entry->SetHeaderIntegrity(std::move(header_integrity));
  auto inner_response = network::mojom::URLResponseHead::New();
  inner_response->headers = headers;
  entry->SetInnerResponse(std::move(inner_response));
  storage::BlobStorageContext blob_context;
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_context.AddBrokenBlob("broken_uuid", "", "",
                                 storage::BlobStatus::ERR_OUT_OF_MEMORY);
  entry->SetBlobDataHandle(std::move(blob_handle));
  entry->SetSignatureExpireTime(base::Time::Now() + base::Days(1));

  // Create a `SubresourceSignedExchangeURLLoaderFactory`.
  //
  // `SubresourceSignedExchangeURLLoaderFactory` manages its own lifetime
  // (see `SubresourceSignedExchangeURLLoaderFactory::OnMojoDisconnect`)
  // and therefore a raw `new` is okay and expected below.
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  new content::SubresourceSignedExchangeURLLoaderFactory(
      factory.BindNewPipeAndPassReceiver(), std::move(entry),
      /* request_initiator_origin_lock = */ initiator_origin);

  // Create a `SignedExchangeInnerResponseURLLoader`.
  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  constexpr int kRequestId = 456;
  network::ResourceRequest request;
  request.url = inner_url;
  request.request_initiator = initiator_origin;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), kRequestId,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // The essence of the repro for https://crbug.com/345261068 is that the
  // remote to `PrefetchedSignedExchangeCacheEntry` is destroyed before
  // `SignedExchangeInnerResponseURLLoader`.  This is orchestrated by the
  // test steps below:
  factory.reset();
  task_environment.RunUntilIdle();
  loader.reset();
}

}  // namespace
}  // namespace content
