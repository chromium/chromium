// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/subresource_signed_exchange_url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/functions.h"
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

std::unique_ptr<PrefetchedSignedExchangeCacheEntry> CreateCacheEntry(
    const GURL& outer_url,
    const GURL& inner_url,
    storage::BlobStorageContext* blob_context) {
  auto entry = std::make_unique<PrefetchedSignedExchangeCacheEntry>();
  auto status = std::make_unique<network::URLLoaderCompletionStatus>();
  entry->SetCompletionStatus(std::move(status));
  entry->SetOuterUrl(outer_url);
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
  std::unique_ptr<storage::BlobDataHandle> blob_handle =
      blob_context->AddBrokenBlob("broken_uuid", "", "",
                                  storage::BlobStatus::ERR_OUT_OF_MEMORY);
  entry->SetBlobDataHandle(std::move(blob_handle));
  entry->SetSignatureExpireTime(base::Time::Now() + base::Days(1));
  return entry;
}

// This is a regression test for https://crbug.com/345261068.
// (Note that the repro may require `enable_dangling_raw_ptr_checks` in
// `args.gn`. See also `//docs/dangling_ptr_guide.md`)
TEST(SubresourceSignedExchangeURLLoaderFactoryTest,
     ShortlivedFactoryAndLonglivedLoader) {
  BrowserTaskEnvironment task_environment;
  GURL inner_url("https://foo.com/outer/inner");
  GURL outer_url("https://foo.com/outer");
  auto initiator_origin = url::Origin::Create(GURL("https://foo.com"));

  storage::BlobStorageContext blob_context;
  auto entry = CreateCacheEntry(outer_url, inner_url, &blob_context);

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

TEST(SubresourceSignedExchangeURLLoaderFactoryTest,
     CreateLoaderAndStart_InvalidURL) {
  BrowserTaskEnvironment task_environment;
  GURL inner_url("https://foo.com/outer/inner");
  GURL outer_url("https://foo.com/outer");
  auto initiator_origin = url::Origin::Create(GURL("https://foo.com"));

  storage::BlobStorageContext blob_context;
  auto entry = CreateCacheEntry(outer_url, inner_url, &blob_context);

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  new content::SubresourceSignedExchangeURLLoaderFactory(
      factory.BindNewPipeAndPassReceiver(), std::move(entry),
      /* request_initiator_origin_lock = */ initiator_origin);

  std::string received_error;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = GURL("https://attacker.com/fake.json");
  request.request_initiator = initiator_origin;
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 123,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  factory.FlushForTesting();
  EXPECT_EQ(received_error,
            "SubresourceSignedExchangeURLLoaderFactory: "
            "request.url does not match inner_url");

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

TEST(SubresourceSignedExchangeURLLoaderFactoryTest,
     CreateLoaderAndStart_InvalidInitiator) {
  BrowserTaskEnvironment task_environment;
  GURL inner_url("https://foo.com/outer/inner");
  GURL outer_url("https://foo.com/outer");
  auto initiator_origin_lock = url::Origin::Create(GURL("https://foo.com"));

  storage::BlobStorageContext blob_context;
  auto entry = CreateCacheEntry(outer_url, inner_url, &blob_context);

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  new content::SubresourceSignedExchangeURLLoaderFactory(
      factory.BindNewPipeAndPassReceiver(), std::move(entry),
      /* request_initiator_origin_lock = */ initiator_origin_lock);

  std::string received_error;
  mojo::SetDefaultProcessErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { received_error = error; }));

  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  network::ResourceRequest request;
  request.url = inner_url;
  // Use a different origin than the lock.
  request.request_initiator = url::Origin::Create(GURL("https://attacker.com"));
  factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), 123,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  factory.FlushForTesting();
  EXPECT_EQ(received_error,
            "SubresourceSignedExchangeURLLoaderFactory: "
            "lock VS initiator mismatch");

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());
}

}  // namespace
}  // namespace content
