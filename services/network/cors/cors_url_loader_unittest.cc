// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace cors {

namespace {

const uint32_t kRendererProcessId = 573;

constexpr char kTestCorsExemptHeader[] = "x-test-cors-exempt";

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory() {}
  ~TestURLLoaderFactory() override = default;

  base::WeakPtr<TestURLLoaderFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void NotifyClientOnReceiveResponse(
      int status_code,
      const std::vector<std::pair<std::string, std::string>>& extra_headers) {
    DCHECK(client_remote_);
    auto response = mojom::URLResponseHead::New();
    response->headers = new net::HttpResponseHeaders(
        base::StringPrintf("HTTP/1.1 %d OK\n"
                           "Content-Type: image/png\n",
                           status_code));
    for (const auto& header : extra_headers)
      response->headers->SetHeader(header.first, header.second);

    client_remote_->OnReceiveResponse(std::move(response));
  }

  void NotifyClientOnComplete(int error_code) {
    DCHECK(client_remote_);
    client_remote_->OnComplete(URLLoaderCompletionStatus(error_code));
  }

  void NotifyClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::pair<std::string, std::string>>& extra_headers) {
    auto response = mojom::URLResponseHead::New();
    response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        base::StringPrintf("HTTP/1.1 %d\n", redirect_info.status_code));
    for (const auto& header : extra_headers)
      response->headers->SetHeader(header.first, header.second);

    client_remote_->OnReceiveRedirect(redirect_info, std::move(response));
  }

  bool IsCreateLoaderAndStartCalled() { return !!client_remote_; }

  void SetOnCreateLoaderAndStart(const base::RepeatingClosure& closure) {
    on_create_loader_and_start_ = closure;
  }

  const ResourceRequest& request() const { return request_; }
  const GURL& GetRequestedURL() const { return request_.url; }
  int num_created_loaders() const { return num_created_loaders_; }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    ++num_created_loaders_;
    DCHECK(client);
    request_ = resource_request;
    client_remote_.reset();
    client_remote_.Bind(std::move(client));

    if (on_create_loader_and_start_)
      on_create_loader_and_start_.Run();
  }

  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
    NOTREACHED();
  }

  mojo::Remote<mojom::URLLoaderClient> client_remote_;

  ResourceRequest request_;

  int num_created_loaders_ = 0;

  base::RepeatingClosure on_create_loader_and_start_;

  base::WeakPtrFactory<TestURLLoaderFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

class CorsURLLoaderTest : public testing::Test {
 public:
  using ReferrerPolicy = net::ReferrerPolicy;

  CorsURLLoaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    net::URLRequestContextBuilder context_builder;
    context_builder.set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());
    url_request_context_ = context_builder.Build();
  }

 protected:
  // testing::Test implementation.
  void SetUp(network::mojom::NetworkContextParamsPtr context_params) {
    network_service_ = NetworkService::CreateForTesting();

    // Use a dummy CertVerifier that always passes cert verification, since
    // these unittests don't need to test CertVerifier behavior.
    context_params->cert_verifier_params =
        FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    context_params->cors_exempt_header_list.push_back(kTestCorsExemptHeader);
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(),
        network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(context_params));

    const url::Origin default_initiator_origin =
        url::Origin::Create(GURL("https://example.com"));
    ResetFactory(default_initiator_origin, kRendererProcessId);
  }
  void SetUp() override { SetUp(mojom::NetworkContextParams::New()); }

  void CreateLoaderAndStart(const GURL& origin,
                            const GURL& url,
                            mojom::RequestMode mode) {
    ResourceRequest request;
    request.mode = mode;
    request.credentials_mode = mojom::CredentialsMode::kOmit;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = url;
    request.request_initiator = url::Origin::Create(origin);
    CreateLoaderAndStart(request);
  }

  void CreateLoaderAndStart(const ResourceRequest& request) {
    test_cors_loader_client_ = std::make_unique<TestURLLoaderClient>();
    url_loader_.reset();
    cors_url_loader_factory_->CreateLoaderAndStart(
        url_loader_.BindNewPipeAndPassReceiver(), 0 /* routing_id */,
        0 /* request_id */, mojom::kURLLoadOptionNone, request,
        test_cors_loader_client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  bool IsNetworkLoaderStarted() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->IsCreateLoaderAndStartCalled();
  }

  void NotifyLoaderClientOnReceiveResponse(
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(200, extra_headers);
  }

  void NotifyLoaderClientOnReceiveResponse(
      int status_code,
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(status_code,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveRedirect(redirect_info,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnComplete(int error_code) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnComplete(error_code);
  }

  void FollowRedirect(
      const std::vector<std::string>& removed_headers = {},
      const net::HttpRequestHeaders& modified_headers =
          net::HttpRequestHeaders(),
      const net::HttpRequestHeaders& modified_cors_exempt_headers =
          net::HttpRequestHeaders()) {
    DCHECK(url_loader_);
    url_loader_->FollowRedirect(removed_headers, modified_headers,
                                modified_cors_exempt_headers,
                                base::nullopt /*new_url*/);
  }

  void AddHostHeaderAndFollowRedirect() {
    DCHECK(url_loader_);
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
    url_loader_->FollowRedirect({},  // removed_headers
                                modified_headers,
                                {},              // modified_cors_exempt_headers
                                base::nullopt);  // new_url
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

  const TestURLLoaderClient& client() const {
    return *test_cors_loader_client_;
  }
  void ClearHasReceivedRedirect() {
    test_cors_loader_client_->ClearHasReceivedRedirect();
  }

  void RunUntilCreateLoaderAndStartCalled() {
    DCHECK(test_url_loader_factory_);
    base::RunLoop run_loop;
    test_url_loader_factory_->SetOnCreateLoaderAndStart(run_loop.QuitClosure());
    run_loop.Run();
    test_url_loader_factory_->SetOnCreateLoaderAndStart({});
  }
  void RunUntilComplete() { test_cors_loader_client_->RunUntilComplete(); }
  void RunUntilRedirectReceived() {
    test_cors_loader_client_->RunUntilRedirectReceived();
  }

  void AddAllowListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode) {
    origin_access_list_.AddAllowListEntryForOrigin(
        source_origin, protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  }

  void AddBlockListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode) {
    origin_access_list_.AddBlockListEntryForOrigin(
        source_origin, protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kHighPriority);
  }

  void AddFactoryBoundAllowListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const mojom::CorsDomainMatchMode mode) {
    factory_bound_allow_patterns_.push_back(mojom::CorsOriginPattern::New(
        protocol, domain, /*port=*/0, mode,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
    ResetFactory(source_origin, kRendererProcessId);
  }

  static net::RedirectInfo CreateRedirectInfo(
      int status_code,
      base::StringPiece method,
      const GURL& url,
      base::StringPiece referrer = base::StringPiece(),
      ReferrerPolicy referrer_policy = net::ReferrerPolicy::NO_REFERRER,
      net::SiteForCookies site_for_cookies = net::SiteForCookies()) {
    net::RedirectInfo redirect_info;
    redirect_info.status_code = status_code;
    redirect_info.new_method = method.as_string();
    redirect_info.new_url = url;
    redirect_info.new_referrer = referrer.as_string();
    redirect_info.new_referrer_policy = referrer_policy;
    redirect_info.new_site_for_cookies = site_for_cookies;
    return redirect_info;
  }

  void ResetFactory(base::Optional<url::Origin> initiator,
                    uint32_t process_id,
                    bool is_trusted,
                    bool ignore_isolated_world_origin,
                    bool skip_cors_enabled_scheme_check) {
    if (process_id != mojom::kBrowserProcessId)
      DCHECK(initiator.has_value());

    test_url_loader_factory_ = std::make_unique<TestURLLoaderFactory>();
    test_url_loader_factory_receiver_ =
        std::make_unique<mojo::Receiver<mojom::URLLoaderFactory>>(
            test_url_loader_factory_.get());

    auto factory_params = network::mojom::URLLoaderFactoryParams::New();
    if (initiator) {
      factory_params->request_initiator_origin_lock = *initiator;
      if (!initiator->opaque()) {
        factory_params->factory_bound_access_patterns =
            network::mojom::CorsOriginAccessPatterns::New();
        factory_params->factory_bound_access_patterns->source_origin =
            *initiator;
        for (const auto& item : factory_bound_allow_patterns_) {
          factory_params->factory_bound_access_patterns->allow_patterns
              .push_back(item.Clone());
        }
      }
    }
    factory_params->is_trusted = is_trusted;
    factory_params->process_id = process_id;
    factory_params->is_corb_enabled = (process_id != mojom::kBrowserProcessId);
    factory_params->ignore_isolated_world_origin = ignore_isolated_world_origin;
    factory_params->factory_override = mojom::URLLoaderFactoryOverride::New();
    factory_params->factory_override->overriding_factory =
        test_url_loader_factory_receiver_->BindNewPipeAndPassRemote();
    factory_params->factory_override->skip_cors_enabled_scheme_check =
        skip_cors_enabled_scheme_check;
    auto resource_scheduler_client =
        base::MakeRefCounted<ResourceSchedulerClient>(
            process_id, ++last_issued_route_id, &resource_scheduler_,
            url_request_context_->network_quality_estimator());
    cors_url_loader_factory_remote_.reset();
    cors_url_loader_factory_ = std::make_unique<CorsURLLoaderFactory>(
        network_context_.get(), std::move(factory_params),
        resource_scheduler_client,
        cors_url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
        &origin_access_list_);
  }

  void ResetFactory(base::Optional<url::Origin> initiator,
                    uint32_t process_id) {
    auto params = network::mojom::URLLoaderFactoryParams::New();
    ResetFactory(initiator, process_id, params->is_trusted,
                 params->ignore_isolated_world_origin,
                 false /* skip_cors_enabled_scheme_check */);
  }

  NetworkContext* network_context() { return network_context_.get(); }

 private:
  // Test environment.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ResourceScheduler resource_scheduler_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  // CorsURLLoaderFactory instance under tests.
  std::unique_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> cors_url_loader_factory_remote_;

  // Factory bound origin access list for testing.
  std::vector<mojom::CorsOriginPatternPtr> factory_bound_allow_patterns_;

  std::unique_ptr<TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<mojo::Receiver<mojom::URLLoaderFactory>>
      test_url_loader_factory_receiver_;

  // Holds URLLoader that CreateLoaderAndStart() creates.
  mojo::Remote<mojom::URLLoader> url_loader_;

  // TestURLLoaderClient that records callback activities.
  std::unique_ptr<TestURLLoaderClient> test_cors_loader_client_;

  int last_issued_route_id = 765;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoaderTest);
};

class BadMessageTestHelper {
 public:
  BadMessageTestHelper()
      : dummy_message_(0, 0, 0, 0, nullptr), context_(&dummy_message_) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BadMessageTestHelper::OnBadMessage, base::Unretained(this)));
  }

  ~BadMessageTestHelper() {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  const std::vector<std::string>& bad_message_reports() const {
    return bad_message_reports_;
  }

 private:
  void OnBadMessage(const std::string& reason) {
    bad_message_reports_.push_back(reason);
  }

  std::vector<std::string> bad_message_reports_;

  mojo::Message dummy_message_;
  mojo::internal::MessageDispatchContext context_;

  DISALLOW_COPY_AND_ASSIGN(BadMessageTestHelper);
};

TEST_F(CorsURLLoaderTest, NoCorsWithInvalidMethod) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);
  request.method = "GET\r\nHost: other.example.com";

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: invalid characters in method"));
}

TEST_F(CorsURLLoaderTest, SameOriginWithoutInitiator) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kSameOrigin;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NoCorsWithoutInitiator) {
  // This test needs to simulate a factory used from the browser process,
  // because only the browser process may start requests with no
  // |request_initiator|.  A renderer process would have run into NOTREACHED and
  // mojo::ReportBadMessage via InitiatorLockCompatibility::kNoInitiator case in
  // CorsURLLoaderFactory::IsValidRequest.
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CorsWithoutInitiator) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: cors without initiator"));
}

TEST_F(CorsURLLoaderTest, NavigateWithoutInitiator) {
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, NavigationFromRenderer) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre(
                  "CorsURLLoaderFactory: navigate from non-browser-process"));
}

TEST_F(CorsURLLoaderTest, SameOriginRequest) {
  const GURL url("https://example.com/foo.png");
  CreateLoaderAndStart(url.GetOrigin(), url, mojom::RequestMode::kSameOrigin);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsMode) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithNoCorsModeAndPatchMethod) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.method = "PATCH";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

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
  EXPECT_EQ(origin_header, "https://example.com");
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestFetchRequestModeSameOrigin) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kSameOrigin);

  RunUntilComplete();

  // This call never hits the network URLLoader (i.e. the TestURLLoaderFactory)
  // because it is fails right away.
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kDisallowedByMode,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsModeButMissingCorsHeader) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  std::string origin_header;
  EXPECT_TRUE(GetRequest().headers.GetHeader(net::HttpRequestHeaders::kOrigin,
                                             &origin_header));
  EXPECT_EQ(origin_header, "https://example.com");
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kMissingAllowOriginHeader,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CrossOriginRequestWithCorsMode) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest,
       CrossOriginRequestFetchRequestWithCorsModeButMismatchedCorsHeader) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "http://some-other-domain.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kAllowOriginMismatch,
            client().completion_status().cors_error_status->cors_error);
}

TEST_F(CorsURLLoaderTest, CorsEnabledSameCustomSchemeRequest) {
  // Custom scheme should not be permitted by default.
  const GURL origin("my-scheme://foo/index.html");
  const GURL url("my-scheme://bar/baz.png");
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
  ASSERT_TRUE(client().completion_status().cors_error_status);
  EXPECT_EQ(mojom::CorsError::kCorsDisabledScheme,
            client().completion_status().cors_error_status->cors_error);

  // Scheme check can be skipped via the factory params.
  auto params = network::mojom::URLLoaderFactoryParams::New();
  ResetFactory(url::Origin::Create(origin), mojom::kBrowserProcessId,
               params->is_trusted, params->ignore_isolated_world_origin,
               true /* skip_cors_enabled_scheme_check */);

  // "Access-Control-Allow-Origin: *" accepts the custom scheme.
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse({{"Access-Control-Allow-Origin", "*"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  // "Access-Control-Allow-Origin: null" accepts the custom scheme as a custom
  // scheme is an opaque origin.
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, StripUsernameAndPassword) {
  const GURL origin("https://example.com");
  const GURL url("http://foo:bar@other.example.com/foo.png");
  std::string stripped_url = "http://other.example.com/foo.png";
  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_EQ(stripped_url, GetRequestedURL().spec());
}

TEST_F(CorsURLLoaderTest, CorsCheckPassOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, CorsCheckFailOnRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

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
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, NetworkLoaderErrorDuringRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  // Underlying network::URLLoader may call OnComplete with an error at anytime.
  NotifyLoaderClientOnComplete(net::ERR_FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());
}

TEST_F(CorsURLLoaderTest, SameOriginToSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, SameOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

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
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, CrossOriginToCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});

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

TEST_F(CorsURLLoaderTest, CrossOriginToOriginalOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
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
            mojom::CorsError::kMissingAllowOriginHeader);
}

TEST_F(CorsURLLoaderTest, CrossOriginToAnotherCrossOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();

  // The request is tained, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
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

TEST_F(CorsURLLoaderTest,
       CrossOriginToAnotherCrossOriginRedirectWithPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.method = "PATCH";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Methods", "PATCH"}});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "PATCH");

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "PATCH", new_url),
      {{"Access-Control-Allow-Origin", "https://example.com"}});
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
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"},
       {"Access-Control-Allow-Methods", "PATCH"}});
  RunUntilCreateLoaderAndStartCalled();

  // the second actual request
  EXPECT_EQ(4, num_created_loaders());
  EXPECT_EQ(GetRequest().url, new_url);
  EXPECT_EQ(GetRequest().method, "PATCH");
  ASSERT_TRUE(GetRequest().request_initiator);
  EXPECT_EQ(GetRequest().request_initiator->Serialize(), "https://example.com");

  // The request is tainted, so the origin is "null".
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RedirectInfoShouldBeUsed) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://other.example.com/foo.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.referrer = url;
  request.referrer_policy =
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(url, GetRequest().url);
  EXPECT_EQ("POST", GetRequest().method);
  EXPECT_EQ(url, GetRequest().referrer);
  EXPECT_EQ(net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
            GetRequest().referrer_policy);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "https://other.example.com",
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(new_url, GetRequest().url);
  EXPECT_EQ("GET", GetRequest().method);
  EXPECT_EQ(GURL("https://other.example.com"), GetRequest().referrer);
  EXPECT_EQ(net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
            GetRequest().referrer_policy);

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Makes sure that if an intercepted redirect updates the IsolationInfo and the
// SiteForCookies values, the CorsURLLoader respects those changes. The former
// only happens for frames, and the latter for subframes, but should make
// assumptions about whether these need to be updated in CorsURLLoader.
TEST_F(CorsURLLoaderTest,
       InterceptedRedirectChangesIsolationInfoAndSiteForCookies) {
  const GURL url("https://example.com/foo.png");
  const url::Origin url_origin = url::Origin::Create(url);
  const net::SiteForCookies url_site_for_cookies =
      net::SiteForCookies::FromOrigin(url_origin);

  const GURL new_url("https://other.example.com/foo.png");
  const url::Origin new_url_origin = url::Origin::Create(new_url);
  const net::SiteForCookies new_url_site_for_cookies =
      net::SiteForCookies::FromOrigin(new_url_origin);

  auto params = network::mojom::URLLoaderFactoryParams::New();
  ResetFactory(url_origin, kRendererProcessId, true /* is_trusted */,
               params->ignore_isolated_world_origin,
               false /* skip_cors_enabled_scheme_check */);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = url;
  request.request_initiator = url_origin;
  request.site_for_cookies = url_site_for_cookies;
  request.update_first_party_url_on_redirect = true;
  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateTopFrame,
      url_origin /* top_frame_origin */, url_origin /* frame_origin */,
      url_site_for_cookies);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(url, GetRequest().url);

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      303, "GET", new_url, "" /* referrer */, net::ReferrerPolicy::NO_REFERRER,
      new_url_site_for_cookies));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  ClearHasReceivedRedirect();
  FollowRedirect();
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(new_url, GetRequest().url);
  EXPECT_EQ("GET", GetRequest().method);
  EXPECT_TRUE(
      GetRequest().site_for_cookies.IsEquivalent(new_url_site_for_cookies));
  EXPECT_TRUE(GetRequest().trusted_params->isolation_info.IsEqualForTesting(
      net::IsolationInfo::Create(
          net::IsolationInfo::RedirectMode::kUpdateTopFrame,
          new_url_origin /* top_frame_origin */,
          new_url_origin /* frame_origin */, new_url_site_for_cookies)));

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, TooManyRedirects) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();
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

TEST_F(CorsURLLoaderTest, FollowErrorRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.redirect_mode = mojom::RedirectMode::kError;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, CorsExemptHeaderRemovalOnCrossOriginRedirects) {
  ResourceRequest request;
  request.url = GURL("https://example.com/foo.png");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.mode = mojom::RequestMode::kCors;
  request.cors_exempt_headers.SetHeader(kTestCorsExemptHeader, "test-value");
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(1, num_created_loaders());

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(
      301, "GET", GURL("https://other.example.com/bar.png")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  ASSERT_TRUE(client().has_received_redirect());
  ASSERT_FALSE(client().has_received_response());
  ASSERT_FALSE(client().has_received_completion());
  EXPECT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));

  FollowRedirect({kTestCorsExemptHeader});
  RunUntilCreateLoaderAndStartCalled();

  EXPECT_EQ(2, num_created_loaders());
  EXPECT_FALSE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));
}

TEST_F(CorsURLLoaderTest, CorsExemptHeaderModificationOnRedirects) {
  ResourceRequest request;
  request.url = GURL("https://example.com/foo.png");
  request.request_initiator = url::Origin::Create(request.url);
  request.cors_exempt_headers.SetHeader(kTestCorsExemptHeader, "test-value");
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  EXPECT_EQ(1, num_created_loaders());

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://example.com/bar.png")));
  RunUntilRedirectReceived();

  ASSERT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));

  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(kTestCorsExemptHeader, "test-modified");
  FollowRedirect({}, modified_headers);
  RunUntilComplete();

  ASSERT_EQ(1, num_created_loaders());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  ASSERT_TRUE(
      GetRequest().cors_exempt_headers.HasHeader(kTestCorsExemptHeader));
}

// Tests if OriginAccessList is actually used to decide the cors flag.
// Details for the OriginAccessList behaviors are verified in
// OriginAccessListTest, but this test intends to verify if CorsURlLoader calls
// the list properly.
TEST_F(CorsURLLoaderTest, OriginAccessList_Allowed) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes into account
// ResourceRequest::isolated_world_origin when consulting OriginAccessList.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOrigin) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");

  ResetFactory(main_world_origin, kRendererProcessId, false /* trusted */,
               false /* ignore_isolated_world_origin */,
               false /* skip_cors_enabled_scheme_check */);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  ASSERT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes into account
// ResourceRequest::isolated_world_origin when consulting OriginAccessList
// after redirects.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOrigin_Redirect) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");
  // |new_url| is same-origin as |url| to avoid tainting the response
  // in CorsURLLoader::OnReceiveRedirect.
  const GURL new_url("http://other.example.com/bar.png");

  ResetFactory(main_world_origin, kRendererProcessId, false /* trusted */,
               false /* ignore_isolated_world_origin */,
               false /* skip_cors_enabled_scheme_check */);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddAllowListEntryForOrigin(isolated_world_origin, new_url.scheme(),
                             new_url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  // Using no-cors to force opaque response (unless the allowlist entry added
  // above is taken into account).
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_completion());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_redirect());

  FollowRedirect();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  ASSERT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Tests if CorsURLLoader takes ignores ResourceRequest::isolated_world_origin
// when URLLoaderFactoryParams::ignore_isolated_world_origin is set to true.
TEST_F(CorsURLLoaderTest, OriginAccessList_IsolatedWorldOriginIgnored) {
  const url::Origin main_world_origin =
      url::Origin::Create(GURL("http://main-world.example.com"));
  const url::Origin isolated_world_origin =
      url::Origin::Create(GURL("http://isolated-world.example.com"));
  const GURL url("http://other.example.com/foo.png");

  ResetFactory(main_world_origin, kRendererProcessId, false /* trusted */,
               true /* ignore_isolated_world_origin */,
               false /* skip_cors_enabled_scheme_check */);

  AddAllowListEntryForOrigin(isolated_world_origin, url.scheme(), url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  request.request_initiator = main_world_origin;
  request.isolated_world_origin = isolated_world_origin;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Check if higher-priority block list wins.
TEST_F(CorsURLLoaderTest, OriginAccessList_Blocked) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddBlockListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// CorsURLLoader manages two lists, per-NetworkContext list and
// per-URLLoaderFactory list. This test verifies if per-URLLoaderFactory list
// works.
TEST_F(CorsURLLoaderTest, OriginAccessList_AllowedByFactoryList) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  AddFactoryBoundAllowListEntryForOrigin(
      url::Origin::Create(origin), url.scheme(), url.host(),
      mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Checks if CorsURLLoader can respect the per-NetworkContext block list.
TEST_F(CorsURLLoaderTest, OriginAccessList_AllowedByFactoryListButBlocked) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  AddFactoryBoundAllowListEntryForOrigin(
      url::Origin::Create(origin), url.scheme(), url.host(),
      mojom::CorsDomainMatchMode::kDisallowSubdomains);
  AddBlockListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_FAILED, client().completion_status().error_code);
}

// Tests if OriginAccessList is actually used to decide response tainting.
TEST_F(CorsURLLoaderTest, OriginAccessList_NoCors) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request without using
  // CORS.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_EQ(network::mojom::FetchResponseType::kBasic,
            client().response_head()->response_type);
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, OriginAccessList_POST) {
  const GURL origin("https://example.com");
  const GURL url("http://other.example.com/foo.png");

  // Adds an entry to allow the cross origin request beyond the CORS
  // rules.
  AddAllowListEntryForOrigin(url::Origin::Create(origin), url.scheme(),
                             url.host(),
                             mojom::CorsDomainMatchMode::kDisallowSubdomains);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "POST";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);

  // preflight request
  ASSERT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "POST");
  std::string attached_origin;
  EXPECT_TRUE(GetRequest().headers.GetHeader("origin", &attached_origin));
  EXPECT_EQ(attached_origin, url::Origin::Create(origin).Serialize());
}

TEST_F(CorsURLLoaderTest, 304ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, 304ForSimpleGet) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, 200ForSimpleRevalidation) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = "GET";
  request.url = url;
  request.request_initiator = url::Origin::Create(origin);
  request.headers.SetHeader("If-Modified-Since", "x");
  request.headers.SetHeader("If-None-Match", "y");
  request.headers.SetHeader("Cache-Control", "z");
  request.is_revalidating = true;
  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

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

TEST_F(CorsURLLoaderTest, RevalidationAndPreflight) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://other2.example.com/bar.png");

  ResourceRequest original_request;
  original_request.mode = mojom::RequestMode::kCors;
  original_request.credentials_mode = mojom::CredentialsMode::kOmit;
  original_request.method = "GET";
  original_request.url = url;
  original_request.request_initiator = url::Origin::Create(origin);
  original_request.headers.SetHeader("If-Modified-Since", "x");
  original_request.headers.SetHeader("If-None-Match", "y");
  original_request.headers.SetHeader("Cache-Control", "z");
  original_request.headers.SetHeader("foo", "bar");
  original_request.is_revalidating = true;
  CreateLoaderAndStart(original_request);
  RunUntilCreateLoaderAndStartCalled();

  // preflight request
  EXPECT_EQ(1, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "OPTIONS");
  std::string preflight_request_headers;
  EXPECT_TRUE(GetRequest().headers.GetHeader("access-control-request-headers",
                                             &preflight_request_headers));
  EXPECT_EQ(preflight_request_headers, "foo");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"},
       {"Access-Control-Allow-Headers", "foo"}});
  RunUntilCreateLoaderAndStartCalled();

  // the actual request
  EXPECT_EQ(2, num_created_loaders());
  EXPECT_EQ(GetRequest().url, url);
  EXPECT_EQ(GetRequest().method, "GET");

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  ASSERT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
}

// Keep this in sync with the CalculateResponseTainting test in
// Blink's cors_test.cc.
TEST(CorsURLLoaderTaintingTest, CalculateResponseTainting) {
  using mojom::FetchResponseType;
  using mojom::RequestMode;

  const GURL same_origin_url("https://example.com/");
  const GURL cross_origin_url("https://example2.com/");
  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  const base::Optional<url::Origin> no_origin;

  OriginAccessList origin_access_list;

  // CORS flag is false, same-origin request
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kSameOrigin, origin,
                base::nullopt, false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNoCors, origin, base::nullopt,
                false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kCors, origin, base::nullopt,
                false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                base::nullopt, false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNavigate, origin, base::nullopt,
                false, false, &origin_access_list));

  // CORS flag is false, cross-origin request
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kNoCors, origin, base::nullopt,
                false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kNavigate, origin, base::nullopt,
                false, false, &origin_access_list));

  // CORS flag is true, same-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kCors, origin, base::nullopt,
                true, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                base::nullopt, true, false, &origin_access_list));

  // CORS flag is true, cross-origin request
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kCors, origin, base::nullopt,
                true, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kCors,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                base::nullopt, true, false, &origin_access_list));

  // Origin is not provided.
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNoCors, no_origin, base::nullopt,
                false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNavigate, no_origin,
                base::nullopt, false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kNoCors, no_origin,
                base::nullopt, false, false, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                cross_origin_url, RequestMode::kNavigate, no_origin,
                base::nullopt, false, false, &origin_access_list));

  // Tainted origin.
  EXPECT_EQ(FetchResponseType::kOpaque,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNoCors, origin, base::nullopt,
                false, true, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kCorsWithForcedPreflight, origin,
                base::nullopt, false, true, &origin_access_list));
  EXPECT_EQ(FetchResponseType::kBasic,
            CorsURLLoader::CalculateResponseTainting(
                same_origin_url, RequestMode::kNavigate, origin, base::nullopt,
                false, true, &origin_access_list));
}

TEST_F(CorsURLLoaderTest, RequestWithHostHeaderFails) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("https://example.com/path");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader(net::HttpRequestHeaders::kHost,
                            "other.example.com");
  CreateLoaderAndStart(request);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, RequestWithProxyAuthorizationHeaderFails) {
  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("https://example.com/path");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                            "Basic Zm9vOmJhcg==");
  CreateLoaderAndStart(request);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, NoConcerningRequestHeadersLoggedCorrectly) {
  base::HistogramTester histograms;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader("Not", "Concerning");
  request.headers.SetHeader("Totally", "Fine");

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  histograms.ExpectBucketCount(
      "NetworkService.ConcerningRequestHeader.PresentOnStart", true, 0);
  histograms.ExpectBucketCount(
      "NetworkService.ConcerningRequestHeader.PresentOnStart", false, 1);
}

TEST_F(CorsURLLoaderTest, ConcerningRequestHeadersLoggedCorrectly) {
  using ConcerningHeaderId = URLLoader::ConcerningHeaderId;
  base::HistogramTester histograms;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(GURL("https://example.com"));
  request.headers.SetHeader(net::HttpRequestHeaders::kConnection, "Close");
  request.headers.SetHeader(net::HttpRequestHeaders::kCookie, "BadIdea=true");

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);

  histograms.ExpectBucketCount(
      "NetworkService.ConcerningRequestHeader.PresentOnStart", true, 1);
  histograms.ExpectBucketCount(
      "NetworkService.ConcerningRequestHeader.PresentOnStart", false, 0);
  for (int i = 0; i < static_cast<int>(ConcerningHeaderId::kMaxValue); ++i) {
    if (i == static_cast<int>(ConcerningHeaderId::kConnection) ||
        i == static_cast<int>(ConcerningHeaderId::kCookie)) {
      histograms.ExpectBucketCount(
          "NetworkService.ConcerningRequestHeader.HeaderPresentOnStart", i, 1);
    } else {
      histograms.ExpectBucketCount(
          "NetworkService.ConcerningRequestHeader.HeaderPresentOnStart", i, 0);
    }
  }
}

TEST_F(CorsURLLoaderTest, SetHostHeaderOnRedirectFails) {
  CreateLoaderAndStart(GURL("https://example.com/"),
                       GURL("https://example.com/path"),
                       mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://redirect.test/")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  // This should cause the request to fail.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
  FollowRedirect({} /* removed_headers */, modified_headers);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SetProxyAuthorizationHeaderOnRedirectFails) {
  CreateLoaderAndStart(GURL("https://example.com/"),
                       GURL("https://example.com/path"),
                       mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", GURL("https://redirect.test/")));
  RunUntilRedirectReceived();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_FALSE(client().has_received_completion());

  ClearHasReceivedRedirect();
  // This should cause the request to fail.
  net::HttpRequestHeaders modified_headers;
  modified_headers.SetHeader(net::HttpRequestHeaders::kProxyAuthorization,
                             "Basic Zm9vOmJhcg==");
  FollowRedirect({} /* removed_headers */, modified_headers);

  RunUntilComplete();

  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
}

TEST_F(CorsURLLoaderTest, SameOriginCredentialsModeWithoutInitiator) {
  // This test needs to simulate a factory used from the browser process,
  // because only the browser process may start requests with no
  // |request_initiator|.  A renderer process would have run into NOTREACHED and
  // mojo::ReportBadMessage via InitiatorLockCompatibility::kNoInitiator case in
  // CorsURLLoaderFactory::IsValidRequest.
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kSameOrigin;
  request.url = GURL("https://example.com/");
  request.request_initiator = base::nullopt;

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(bad_message_helper.bad_message_reports(),
              ::testing::ElementsAre("CorsURLLoaderFactory: same-origin "
                                     "credentials mode without initiator"));
}

TEST_F(CorsURLLoaderTest, SameOriginCredentialsModeOnNavigation) {
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kSameOrigin;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre(
          "CorsURLLoaderFactory: unsupported credentials mode on navigation"));
}

TEST_F(CorsURLLoaderTest, OmitCredentialsModeOnNavigation) {
  ResetFactory(base::nullopt /* initiator */, mojom::kBrowserProcessId);

  ResourceRequest request;
  request.mode = mojom::RequestMode::kNavigate;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.url = GURL("https://example.com/");
  request.request_initiator = url::Origin::Create(request.url);

  BadMessageTestHelper bad_message_helper;
  CreateLoaderAndStart(request);
  RunUntilComplete();

  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre(
          "CorsURLLoaderFactory: unsupported credentials mode on navigation"));
}

// Make sure than when a request is failed due to having |trusted_params| set
// and being sent to an untrusted URLLoaderFactory, no CORS request is made.
TEST_F(CorsURLLoaderTest, TrustedParamsWithUntrustedFactoryFailsBeforeCORS) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));

  // Run the test with a trusted URLLoaderFactory as well, to make sure a CORS
  // request is in fact made when using a trusted factory.
  for (bool is_trusted : {false, true}) {
    bool ignore_isolated_world_origin = true;  // This is the default.
    ResetFactory(initiator, kRendererProcessId, is_trusted,
                 ignore_isolated_world_origin,
                 false /* skip_cors_enabled_scheme_check */);

    BadMessageTestHelper bad_message_helper;

    ResourceRequest request;
    request.mode = mojom::RequestMode::kCors;
    request.credentials_mode = mojom::CredentialsMode::kOmit;
    request.method = net::HttpRequestHeaders::kGetMethod;
    request.url = GURL("http://other.example.com/foo.png");
    request.request_initiator = initiator;
    request.trusted_params = ResourceRequest::TrustedParams();
    CreateLoaderAndStart(request);

    if (!is_trusted) {
      RunUntilComplete();
      EXPECT_FALSE(IsNetworkLoaderStarted());
      EXPECT_FALSE(client().has_received_redirect());
      EXPECT_FALSE(client().has_received_response());
      EXPECT_TRUE(client().has_received_completion());
      EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
                client().completion_status().error_code);
      EXPECT_THAT(
          bad_message_helper.bad_message_reports(),
          ::testing::ElementsAre(
              "CorsURLLoaderFactory: Untrusted caller making trusted request"));
    } else {
      RunUntilCreateLoaderAndStartCalled();
      NotifyLoaderClientOnReceiveResponse(
          {{"Access-Control-Allow-Origin", "https://example.com"}});
      NotifyLoaderClientOnComplete(net::OK);

      RunUntilComplete();

      EXPECT_TRUE(IsNetworkLoaderStarted());
      EXPECT_TRUE(client().has_received_response());
      EXPECT_TRUE(client().has_received_completion());
      EXPECT_EQ(net::OK, client().completion_status().error_code);
      EXPECT_TRUE(
          GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
    }
  }
}

// Test that when a request has LOAD_RESTRICTED_PREFETCH and a
// NetworkIsolationKey, CorsURLLoaderFactory does not reject the request.
TEST_F(CorsURLLoaderTest, RestrictedPrefetchSucceedsWithNIK) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));

  ResetFactory(initiator, kRendererProcessId, true /* is_trusted */,
               true /* ignore_isolated_world_origin */,
               false /* skip_cors_enabled_scheme_check */);

  BadMessageTestHelper bad_message_helper;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("http://other.example.com/foo.png");
  request.request_initiator = initiator;
  request.load_flags |= net::LOAD_RESTRICTED_PREFETCH;
  request.trusted_params = ResourceRequest::TrustedParams();

  // Fill up the |trusted_params| NetworkIsolationKey member.
  url::Origin request_origin = url::Origin::Create(request.url);
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RedirectMode::kUpdateNothing, request_origin,
      request_origin, net::SiteForCookies());

  CreateLoaderAndStart(request);
  RunUntilCreateLoaderAndStartCalled();

  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);

  RunUntilComplete();

  EXPECT_TRUE(IsNetworkLoaderStarted());
  EXPECT_TRUE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::OK, client().completion_status().error_code);
  EXPECT_TRUE(GetRequest().headers.HasHeader(net::HttpRequestHeaders::kOrigin));
}

// Test that when a request has LOAD_RESTRICTED_PREFETCH but no
// NetworkIsolationKey, CorsURLLoaderFactory rejects the request. This is
// because the LOAD_RESTRICTED_PREFETCH flag must only appear on requests that
// make use of their TrustedParams' |isolation_info|.
TEST_F(CorsURLLoaderTest, RestrictedPrefetchFailsWithoutNIK) {
  url::Origin initiator = url::Origin::Create(GURL("https://example.com"));
  ResetFactory(initiator, kRendererProcessId, true /* is_trusted */,
               true /* ignore_isolated_world_origin */,
               false /* skip_cors_enabled_scheme_check */);

  BadMessageTestHelper bad_message_helper;

  ResourceRequest request;
  request.mode = mojom::RequestMode::kCors;
  request.credentials_mode = mojom::CredentialsMode::kOmit;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = GURL("http://other.example.com/foo.png");
  request.request_initiator = initiator;
  request.load_flags |= net::LOAD_RESTRICTED_PREFETCH;
  request.trusted_params = ResourceRequest::TrustedParams();

  CreateLoaderAndStart(request);

  RunUntilComplete();
  EXPECT_FALSE(IsNetworkLoaderStarted());
  EXPECT_FALSE(client().has_received_redirect());
  EXPECT_FALSE(client().has_received_response());
  EXPECT_TRUE(client().has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client().completion_status().error_code);
  EXPECT_THAT(
      bad_message_helper.bad_message_reports(),
      ::testing::ElementsAre("CorsURLLoaderFactory: Request with "
                             "LOAD_RESTRICTED_PREFETCH flag is not trusted"));
}

// Test that Timing-Allow-Origin check passes when a same-origin redirect
// occurs. The redirect is as follows: [Origin] A -> A -> A.
TEST_F(CorsURLLoaderTest, TAOCheckPassOnSameOriginRedirect) {
  const GURL origin("https://example.com");
  const GURL url("https://example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse();
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTest, TAOCheckFailOnCrossOriginResource1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  // Note: this testcase will change when we change to the model in which TAO
  // passes whenever CORS is used.
  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTest, TAOCheckFailOnCrossOriginResource2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  // null does not work in this case since the tainted origin flag won't be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Access-Control-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

TEST_F(CorsURLLoaderTest, TAOCheckPassOnCrossOriginResource) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin", "https://example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where final redirect does not pass the check.
TEST_F(CorsURLLoaderTest, TAOCheckFailRedirect1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  // This is insufficient: tainted origin flag will be set.
  NotifyLoaderClientOnReceiveResponse(
      {{"Timing-Allow-Origin",
        "https://example.com, https://other.example.com"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A where B does not pass the check.
TEST_F(CorsURLLoaderTest, TAOCheckFailRedirect2) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(CreateRedirectInfo(301, "GET", new_url));
  RunUntilRedirectReceived();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "*"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_FALSE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> A
TEST_F(CorsURLLoaderTest, TAOCheckPassRedirect1) {
  const GURL origin("https://example.com");
  const GURL url("https://other.example.com/foo.png");
  const GURL new_url("https://example.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

// [Origin] A -> B -> C
TEST_F(CorsURLLoaderTest, TAOCheckPassRedirect2) {
  const GURL origin("https://example.com");
  const GURL url("https://other1.com/foo.png");
  const GURL new_url("https://other2.com/bar.png");

  CreateLoaderAndStart(origin, url, mojom::RequestMode::kNoCors);
  RunUntilCreateLoaderAndStartCalled();
  NotifyLoaderClientOnReceiveRedirect(
      CreateRedirectInfo(301, "GET", new_url),
      {{"Timing-Allow-Origin", "https://example.com"}});
  RunUntilRedirectReceived();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);

  ClearHasReceivedRedirect();
  FollowRedirect();

  NotifyLoaderClientOnReceiveResponse({{"Timing-Allow-Origin", "null"}});
  NotifyLoaderClientOnComplete(net::OK);
  RunUntilComplete();

  EXPECT_TRUE(client().response_head()->timing_allow_passed);
}

}  // namespace

}  // namespace cors

}  // namespace network
