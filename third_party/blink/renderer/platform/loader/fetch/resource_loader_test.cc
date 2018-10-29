// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"

#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class ResourceLoaderTest : public testing::Test {
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderTest);

 public:
  enum class From {
    kServiceWorker,
    kNetwork,
  };

  ResourceLoaderTest()
      : foo_url_("https://foo.test"), bar_url_("https://bar.test"){};

  void SetUp() override {
    context_ = MockFetchContext::Create(
        MockFetchContext::kShouldLoadNewResource, nullptr,
        std::make_unique<TestWebURLLoaderFactory>());
  }

 protected:
  using FetchRequestMode = network::mojom::FetchRequestMode;
  using FetchResponseType = network::mojom::FetchResponseType;

  struct TestCase {
    const KURL url;
    const FetchRequestMode request_mode;
    const From from;
    const scoped_refptr<const SecurityOrigin> allowed_origin;
    const FetchResponseType original_response_type;
    const FetchResponseType expectation;
  };

  Persistent<MockFetchContext> context_;

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

  const KURL foo_url_;
  const KURL bar_url_;

 private:
  class TestWebURLLoaderFactory final : public WebURLLoaderFactory {
    std::unique_ptr<WebURLLoader> CreateURLLoader(
        const WebURLRequest& request,
        std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>)
        override {
      return std::make_unique<TestWebURLLoader>();
    }
  };

  class TestWebURLLoader final : public WebURLLoader {
   public:
    ~TestWebURLLoader() override = default;
    void LoadSynchronously(const WebURLRequest&,
                           WebURLLoaderClient*,
                           WebURLResponse&,
                           base::Optional<WebURLError>&,
                           WebData&,
                           int64_t& encoded_data_length,
                           int64_t& encoded_body_length,
                           WebBlobInfo& downloaded_blob) override {
      NOTREACHED();
    }
    void LoadAsynchronously(const WebURLRequest&,
                            WebURLLoaderClient*) override {}

    void Cancel() override {}
    void SetDefersLoading(bool) override {}
    void DidChangePriority(WebURLRequest::Priority, int) override {
      NOTREACHED();
    }
  };
};

std::ostream& operator<<(std::ostream& o, const ResourceLoaderTest::From& f) {
  switch (f) {
    case ResourceLoaderTest::From::kServiceWorker:
      o << "service worker";
      break;
    case ResourceLoaderTest::From::kNetwork:
      o << "network";
      break;
  }
  return o;
}

TEST_F(ResourceLoaderTest, ResponseType) {
  // This test will be trivial if EnableOutOfBlinkCORS is enabled.
  WebRuntimeFeatures::EnableOutOfBlinkCORS(false);

  const scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::Create(foo_url_);
  const scoped_refptr<const SecurityOrigin> no_origin = nullptr;
  const KURL same_origin_url = foo_url_;
  const KURL cross_origin_url = bar_url_;

  TestCase cases[] = {
      // Same origin response:
      {same_origin_url, FetchRequestMode::kNoCORS, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kCORS, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kBasic},

      // Cross origin, no-cors:
      {cross_origin_url, FetchRequestMode::kNoCORS, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kOpaque},

      // Cross origin, cors:
      {cross_origin_url, FetchRequestMode::kCORS, From::kNetwork, origin,
       FetchResponseType::kDefault, FetchResponseType::kCORS},
      {cross_origin_url, FetchRequestMode::kCORS, From::kNetwork, no_origin,
       FetchResponseType::kDefault, FetchResponseType::kError},

      // From service worker, no-cors:
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kCORS, FetchResponseType::kCORS},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kDefault, FetchResponseType::kDefault},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kOpaque, FetchResponseType::kOpaque},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kOpaqueRedirect,
       FetchResponseType::kOpaqueRedirect},

      // From service worker, cors:
      {same_origin_url, FetchRequestMode::kCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kBasic, FetchResponseType::kBasic},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kCORS, FetchResponseType::kCORS},
      {same_origin_url, FetchRequestMode::kNoCORS, From::kServiceWorker,
       no_origin, FetchResponseType::kDefault, FetchResponseType::kDefault},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "url: " << test.url.GetString()
                 << ", requets mode: " << test.request_mode
                 << ", from: " << test.from << ", allowed_origin: "
                 << (test.allowed_origin ? test.allowed_origin->ToString()
                                         : String("<no allowed origin>"))
                 << ", original_response_type: "
                 << test.original_response_type);

    context_->SetSecurityOrigin(origin);
    ResourceFetcher* fetcher = ResourceFetcher::Create(context_);

    ResourceRequest request;
    request.SetURL(test.url);
    request.SetFetchRequestMode(test.request_mode);
    request.SetRequestContext(mojom::RequestContextType::FETCH);

    FetchParameters fetch_parameters(request);
    if (test.request_mode == network::mojom::FetchRequestMode::kCORS) {
      fetch_parameters.SetCrossOriginAccessControl(
          origin.get(), network::mojom::FetchCredentialsMode::kOmit);
    }
    Resource* resource = RawResource::Fetch(fetch_parameters, fetcher, nullptr);
    ResourceLoader* loader = resource->Loader();

    ResourceResponse response(test.url);
    response.SetHTTPStatusCode(200);
    response.SetType(test.original_response_type);
    response.SetWasFetchedViaServiceWorker(test.from == From::kServiceWorker);
    if (test.allowed_origin) {
      response.SetHTTPHeaderField("access-control-allow-origin",
                                  test.allowed_origin->ToAtomicString());
    }
    response.SetType(test.original_response_type);

    loader->DidReceiveResponse(WrappedResourceResponse(response));
    EXPECT_EQ(test.expectation, resource->GetResponse().GetType());
  }
}

}  // namespace blink
