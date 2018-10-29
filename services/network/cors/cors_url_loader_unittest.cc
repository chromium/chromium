// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace cors {

namespace {

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() : weak_factory_(this) {}
  ~TestURLLoaderFactory() override = default;

  base::WeakPtr<TestURLLoaderFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NotifyClientOnReceiveResponse(
      int status_code,
      const std::vector<std::string>& extra_headers) {
    ResourceResponseHead response;
    response.headers = new net::HttpResponseHeaders(
        base::StringPrintf("HTTP/1.1 %d OK\n"
                           "Content-Type: image/png\n",
                           status_code));
    for (const auto& header : extra_headers)
      response.headers->AddHeader(header);

    client_ptr_->OnReceiveResponse(response);
  }

  void NotifyClientOnComplete(int error_code) {
    DCHECK(client_ptr_);
    client_ptr_->OnComplete(URLLoaderCompletionStatus(error_code));
  }

  void NotifyClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::string>& extra_headers) {
    ResourceResponseHead response;
    response.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        base::StringPrintf("HTTP/1.1 %d\n", redirect_info.status_code));
    for (const auto& header : extra_headers)
      response.headers->AddHeader(header);

    client_ptr_->OnReceiveRedirect(redirect_info, response);
  }

  bool IsCreateLoaderAndStartCalled() { return !!client_ptr_; }

  void SetOnCreateLoaderAndStart(const base::RepeatingClosure& closure) {
    on_create_loader_and_start_ = closure;
  }

  const network::ResourceRequest& request() const { return request_; }
  const GURL& GetRequestedURL() const { return request_.url; }
  int num_created_loaders() const { return num_created_loaders_; }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    ++num_created_loaders_;
    DCHECK(client);
    request_ = resource_request;
    client_ptr_ = std::move(client);

    if (on_create_loader_and_start_)
      on_create_loader_and_start_.Run();
  }

  void Clone(mojom::URLLoaderFactoryRequest request) override { NOTREACHED(); }

  mojom::URLLoaderClientPtr client_ptr_;

  ResourceRequest request_;

  int num_created_loaders_ = 0;

  base::RepeatingClosure on_create_loader_and_start_;

  base::WeakPtrFactory<TestURLLoaderFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class CORSURLLoaderTest : public testing::Test {
 public:
  using ReferrerPolicy = net::URLRequest::ReferrerPolicy;

  CORSURLLoaderTest() {
    std::unique_ptr<TestURLLoaderFactory> factory =
        std::make_unique<TestURLLoaderFactory>();
    test_url_loader_factory_ = factory->GetWeakPtr();
    cors_url_loader_factory_ = std::make_unique<CORSURLLoaderFactory>(
        false, std::move(factory), base::RepeatingCallback<void(int)>(),
        &origin_access_list_);
  }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kOutOfBlinkCORS);
  }

  void CreateLoaderAndStart(const GURL& origin,
                            const GURL& url,
                            mojom::FetchRequestMode fetch_request_mode) {
    ResourceRequest request;
    request.fetch_request_mode = fetch_request_mode;
    request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
    request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
    request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = url;
    request.request_initiator = url::Origin::Create(origin);
    CreateLoaderAndStart(request);
  }

  void CreateLoaderAndStart(const ResourceRequest& request) {
    cors_url_loader_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&url_loader_), 0 /* routing_id */, 0 /* request_id */,
        mojom::kURLLoadOptionNone, request,
        test_cors_loader_client_.CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  bool IsNetworkLoaderStarted() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->IsCreateLoaderAndStartCalled();
  }

  void NotifyLoaderClientOnReceiveResponse(
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(200, extra_headers);
  }

  void NotifyLoaderClientOnReceiveResponse(
      int status_code,
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(status_code,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::string>& extra_headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveRedirect(redirect_info,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnComplete(int error_code) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnComplete(error_code);
  }

  void FollowRedirect() {
    DCHECK(url_loader_);
    url_loader_->FollowRedirect(base::nullopt, base::nullopt);
  }

  const ResourceRequest& GetRequest() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->request();
  }

  const GURL& GetRequestedURL() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->GetRequestedURL();
  }

  int num_created_loaders() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->num_created_loaders();
  }

  const TestURLLoaderClient& client() const { return test_cors_loader_client_; }
  void ClearHasReceivedRedirect() {
    test_cors_loader_client_.ClearHasReceivedRedirect();
  }

  void RunUntilCreateLoaderAndStartCalled() {
    DCHECK(test_url_loader_factory_);
    base::RunLoop run_loop;
    test_url_loader_factory_->SetOnCreateLoaderAndStart(run_loop.QuitClosure());
    run_loop.Run();
    test_url_loader_factory_->SetOnCreateLoaderAndStart({});
  }
  void RunUntilComplete() { test_cors_loader_client_.RunUntilComplete(); }
  void RunUntilRedirectReceived() {
    test_cors_loader_client_.RunUntilRedirectReceived();
  }

  void AddAllowListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  bool allow_subdomains) {
    origin_access_list_.AddAllowListEntryForOrigin(
        source_origin, protocol, domain, allow_subdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  }

  static net::RedirectInfo CreateRedirectInfo(
      int status_code,
      base::StringPiece method,
      const GURL& url,
      base::StringPiece referrer = base::StringPiece(),
      ReferrerPolicy referrer_policy = net::URLRequest::NO_REFERRER) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = status_code;
    redirect_info.new_method = method.as_string();
    redirect_info.new_url = url;
    redirect_info.new_referrer = referrer.as_string();
    redirect_info.new_referrer_policy = referrer_policy;
    return redirect_info;
  }

 private:
  // Be the first member so it is destroyed last.
  base::MessageLoop message_loop_;

  // Testing instance to enable kOutOfBlinkCORS feature.
  base::test::ScopedFeatureList feature_list_;

  // CORSURLLoaderFactory instance under tests.
  std::unique_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;

  // TestURLLoaderFactory instance owned by CORSURLLoaderFactory.
  base::WeakPtr<TestURLLoaderFactory> test_url_loader_factory_;

  // Holds URLLoaderPtr that CreateLoaderAndStart() creates.
  mojom::URLLoaderPtr url_loader_;

  // TestURLLoaderClient that records callback activities.
  TestURLLoaderClient test_cors_loader_client_;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;

  DISALLOW_COPY_AND_ASSIGN(CORSURLLoaderTest);
};

TEST_F(CORSURLLoaderTest, SameOriginWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kSameOrigin;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, NoCORSWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CORSWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, NavigateWithoutInitiator) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther1) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther2) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CredentialsModeAndLoadFlagsContradictEachOther3) {
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNavigate;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.url = GURL("http://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, SameOriginRequest) {
  const GURL url("http://example.com/foo.png");
  CreateLoaderAndStart(url.GetOrigin(), url,
                       mojom::FetchRequestMode::kSameOrigin);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithNoCORSMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kNoCORS);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_FALSE(
      GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithNoCORSModeAndPatchMethod) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kNoCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kInclude;
  request.method = "PATCH";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  std::string origin_header;
  EXPECT_TRUE(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin,
                                             &origin_header));
  EXPECT_EQ(origin_header, "http://example.com");
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestFetchRequestModeSameOrigin) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CORSError::kDisallowedByMode,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithCORSModeButMissingCORSHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  std::string origin_header;
  EXPECT_TRUE(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin,
                                             &origin_header));
  EXPECT_EQ(origin_header, "http://example.com");
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CORSError::kMissingAllowOriginHeader,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CORSURLLoaderTest, CrossOriginRequestWithCORSMode) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://example.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest,
       CrossOriginRequestFetchRequestWithCORSModeButMismatchedCORSHeader) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://some-other-domain.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CORSError::kAllowOriginMismatch,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CORSURLLoaderTest, StripUsernameAndPassword) {
  const GURL origin("http://example.com");
  const GURL url("http://foo:bar@other.com/foo.png");
  std::string stripped_url = "http://other.com/foo.png";
  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: http://example.com"});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_EQ(stripped_url, GetRequestedURL().spec());
}

TEST_F(CORSURLLoaderTest, CORSCheckPassOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CORSURLLoaderTest, CORSCheckFailOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(client().completion_status().error_code, net::ERR_FAILED);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CORSError::kMissingAllowOriginHeader);
}

TEST_F(CORSURLLoaderTest, SameOriginToSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, SameOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  RunUntilCreateLoaderAndStartCalled();

  // A new loader is created.
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CrossOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, CrossOriginToOriginalOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();

  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  // We got redirected back to the original origin, but we need an
  // access-control-allow-origin header, and we don't have it in this test case.
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(client().completion_status().cors_error_status->cors_error,
            mojom::CORSError::kMissingAllowOriginHeader);
}

TEST_F(CORSURLLoaderTest, CrossOriginToAnotherCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  // The request is tained, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // original_loader->FollowRedirect() is called, so no new loader is created.
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest,
       CrossOriginToAnotherCrossOriginRedirectWithPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  original_request.method = "PATCH";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com",
       "Access-Control-Allow-Methods: PATCH"});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "PATCH");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "PATCH", new_url),
      {"Access-Control-Allow-Origin: https://example.com"});
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  // the second preflight request
  EXPECT_EQ(3, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null",
                                       "Access-Control-Allow-Methods: PATCH"});
  RunUntilCreateLoaderAndStartCalled();

  // the second actual request
  EXPECT_EQ(4, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "PATCH");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse({"Access-Control-Allow-Origin: null"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, RedirectInfoShouldBeUsed) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/foo.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.referrer = url;
  request.referrer_policy =
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  CreateLoaderAndStart(request);

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "POST");
  EXPECT_EQ(GetRequest().referrer, url);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "https://other.example.com",
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().referrer, GURL("https://other.example.com"));
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, TooManyRedirects) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(1, num_created_loaders());

    GURL new_url(base::StringPrintf("https://example.com/foo.png?%d", i));
    NotifyLoaderClientOnReceiveRedirect(
        CreateRedirectInfo(301, "GET", new_url));

    RunUntilRedirectReceived();
    ASSERT_TRUE(client().has_received_redirect());
    ASSERT_FALSE(client().has_received_response());
    ASSERT_FALSE(client().has_received_completion());

    ClearHasReceivedRedirect();
    FollowRedirect();
  }

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://example.com/bar.png")));
  RunUntilComplete();
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, FollowErrorRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  original_request.fetch_redirect_mode = mojom::FetchRedirectMode::kError;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Tests if OriginAccessList is actually used to decide the cors flag.
// Does not verify detailed functionalities that are verified in
// OriginAccessListTest.
TEST_F(CORSURLLoaderTest, OriginAccessList) {
  const GURL origin("http://example.com");
  const GURL url("http://other.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(), false);

  CreateLoaderAndStart(origin, url, mojom::FetchRequestMode::kCORS);

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, 304ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, 304ForSimpleGet) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(304, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, 200ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);

  // No preflight, no CORS response headers.
  NotifyLoaderClientOnReceiveResponse(200, {});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

TEST_F(CORSURLLoaderTest, RevalidationAndPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.fetch_request_mode = mojom::FetchRequestMode::kCORS;
  original_request.fetch_credentials_mode = mojom::FetchCredentialsMode::kOmit;
  original_request.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  original_request.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  original_request.headers.SetHeader("If-Modified-Since", "x");
  original_request.headers.SetHeader("If-None-Match", "y");
  original_request.headers.SetHeader("Cache-Control", "z");
  original_request.headers.SetHeader("foo", "bar");
  original_request.is_revalidating = true;
  CreateLoaderAndStart(original_request);

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  std::string preflight_request_headers;
  EXPECT_TRUE(GetRequest().headers.GetHeader("access-control-request-headers",
                                             &preflight_request_headers));
  EXPECT_EQ(preflight_request_headers, "foo");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com",
       "Access-Control-Allow-Headers: foo"});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {"Access-Control-Allow-Origin: https://example.com"});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

}  // namespace

}  // namespace cors

}  // namespace network
