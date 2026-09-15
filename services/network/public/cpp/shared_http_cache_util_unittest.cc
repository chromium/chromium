// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_util.h"

#include <string>

#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

ResourceRequest CreateValidRequest() {
  ResourceRequest request;
  request.url = GURL("https://example.com/script.js");
  request.method = "GET";
  request.destination = mojom::RequestDestination::kScript;
  return request;
}

TEST(SharedHttpCacheUtilTest, ValidRequests) {
  auto request = CreateValidRequest();
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.url = GURL("https://example.com/style.css");
  request.destination = mojom::RequestDestination::kStyle;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.url = GURL("https://example.com/image.png");
  request.destination = mojom::RequestDestination::kImage;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.url = GURL("https://example.com/font.woff2");
  request.destination = mojom::RequestDestination::kFont;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));
}

TEST(SharedHttpCacheUtilTest, IneligibleDestinations) {
  auto request = CreateValidRequest();

  const mojom::RequestDestination kIneligibleDestinations[] = {
      mojom::RequestDestination::kEmpty,
      mojom::RequestDestination::kAudio,
      mojom::RequestDestination::kAudioWorklet,
      mojom::RequestDestination::kDocument,
      mojom::RequestDestination::kEmbed,
      mojom::RequestDestination::kFrame,
      mojom::RequestDestination::kIframe,
      mojom::RequestDestination::kManifest,
      mojom::RequestDestination::kObject,
      mojom::RequestDestination::kPaintWorklet,
      mojom::RequestDestination::kReport,
      mojom::RequestDestination::kServiceWorker,
      mojom::RequestDestination::kSharedWorker,
      mojom::RequestDestination::kTrack,
      mojom::RequestDestination::kVideo,
      mojom::RequestDestination::kWebBundle,
      mojom::RequestDestination::kWorker,
      mojom::RequestDestination::kXslt,
      mojom::RequestDestination::kFencedframe,
      mojom::RequestDestination::kWebIdentity,
      mojom::RequestDestination::kCompressionDictionary,
      mojom::RequestDestination::kSpeculationRules,
      mojom::RequestDestination::kJson,
      mojom::RequestDestination::kEmailVerification,
  };

  for (auto destination : kIneligibleDestinations) {
    request.destination = destination;
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request))
        << "Destination " << static_cast<int>(destination)
        << " should be ineligible for write.";
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request))
        << "Destination " << static_cast<int>(destination)
        << " should be ineligible for lookup.";
  }
}

TEST(SharedHttpCacheUtilTest, IneligibleMethods) {
  auto request = CreateValidRequest();

  for (const std::string& method :
       {"POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"}) {
    request.method = method;
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request))
        << "Method " << method << " should be ineligible for write.";
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request))
        << "Method " << method << " should be ineligible for lookup.";
  }
}

TEST(SharedHttpCacheUtilTest, IneligibleSchemes) {
  auto request = CreateValidRequest();

  for (const std::string& url_str : {
           "http://example.com/script.js",
           "data:text/javascript,console.log('hi')",
           "blob:https://example.com/uuid",
           "file:///path/to/file.js",
           "chrome://settings",
           "about:blank",
           "",
       }) {
    request.url = GURL(url_str);
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request))
        << "URL " << url_str << " should be ineligible for write.";
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request))
        << "URL " << url_str << " should be ineligible for lookup.";
  }
}

TEST(SharedHttpCacheUtilTest, RangeHeader) {
  auto request = CreateValidRequest();
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=0-100");
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));
}

TEST(SharedHttpCacheUtilTest, LoadFlags) {
  auto request = CreateValidRequest();

  // LOAD_DISABLE_CACHE disables both read and write.
  request.load_flags = net::LOAD_DISABLE_CACHE;
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));

  // LOAD_BYPASS_CACHE (shift-reload) bypasses cache read, but the fresh
  // response is written to the cache.
  request.load_flags = net::LOAD_BYPASS_CACHE;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.load_flags = net::LOAD_NORMAL;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));

  // LOAD_VALIDATE_CACHE forces network validation, so it cannot be served
  // directly from the shared cache without network validation, but the
  // validated network response can be written to the cache.
  request.load_flags = net::LOAD_VALIDATE_CACHE;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));

  request.load_flags = net::LOAD_ONLY_FROM_CACHE;
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
  EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheLookup(request));
}

TEST(SharedHttpCacheUtilTest, Revalidation) {
  // Revalidation requests cannot be served directly from shared cache, but the
  // network response can be written to the cache.
  {
    auto request = CreateValidRequest();
    request.is_revalidating = true;
    EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));
  }
  {
    auto request = CreateValidRequest();
    request.revalidation_etag = "etag-123";
    EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));
  }
  {
    auto request = CreateValidRequest();
    request.revalidation_last_modified = "Wed, 21 Oct 2015 07:28:00 GMT";
    EXPECT_TRUE(IsRequestEligibleForSharedHttpCacheWrite(request));
    EXPECT_FALSE(IsRequestEligibleForSharedHttpCacheLookup(request));
  }
}

}  // namespace
}  // namespace network
