// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/constants.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager_impl.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/fetch_later.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

using attribution_reporting::kAttributionReportingRegisterSourceHeader;
using attribution_reporting::kAttributionReportingRegisterTriggerHeader;

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using testing::WithArg;

constexpr char kTestRequestUrl[] = "https://example.test";
constexpr char kTestResponseHeaderName[] = "My-Test-Header";
constexpr char kTestResponseHeaderValue[] = "my-test-value";
constexpr char kTestRedirectRequestUrl[] = "https://redirect.test";
constexpr char kTestUnSafeRedirectRequestUrl[] = "about:blank";
constexpr char kTestViolatingCSPRedirectRequestUrl[] =
    "https://violate-csp.test";

// Mock a receiver URLLoaderClient that may exist in renderer.
class MockReceiverURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  MockReceiverURLLoaderClient() = default;
  MockReceiverURLLoaderClient(const MockReceiverURLLoaderClient&) = delete;
  MockReceiverURLLoaderClient& operator=(const MockReceiverURLLoaderClient&) =
      delete;
  ~MockReceiverURLLoaderClient() override {
    if (receiver_.is_bound()) {
      // Flush the pipe to make sure there aren't any lingering events.
      receiver_.FlushForTesting();
    }
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Note that this also unbinds the receiver.
  void ResetReceiver() { receiver_.reset(); }

  // `network::mojom::URLLoaderClient` overrides:
  MOCK_METHOD1(OnReceiveEarlyHints, void(network::mojom::EarlyHintsPtr));
  MOCK_METHOD3(OnReceiveResponse,
               void(network::mojom::URLResponseHeadPtr,
                    mojo::ScopedDataPipeConsumerHandle,
                    std::optional<mojo_base::BigBuffer>));
  MOCK_METHOD2(OnReceiveRedirect,
               void(const net::RedirectInfo&,
                    network::mojom::URLResponseHeadPtr));
  MOCK_METHOD3(OnUploadProgress,
               void(int64_t, int64_t, base::OnceCallback<void()>));
  MOCK_METHOD1(OnTransferSizeUpdated, void(int32_t));
  MOCK_METHOD1(OnComplete, void(const network::URLLoaderCompletionStatus&));

 private:
  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
};

// Fakes a URLLoaderFactory that may exist in renderer, which only delegates to
// `remote_url_loader_factory`.
class FakeRemoteURLLoaderFactory {
 public:
  FakeRemoteURLLoaderFactory() = default;
  FakeRemoteURLLoaderFactory(const FakeRemoteURLLoaderFactory&) = delete;
  FakeRemoteURLLoaderFactory& operator=(const FakeRemoteURLLoaderFactory&) =
      delete;
  ~FakeRemoteURLLoaderFactory() = default;

  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
  BindNewPipeAndPassReceiver() {
    return remote_url_loader_factory.BindNewPipeAndPassReceiver();
  }

  // Binds `remote_url_loader` to a new URLLoader.
  void CreateLoaderAndStart(
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool expect_success = true) {
    remote_url_loader_factory->CreateLoaderAndStart(
        remote_url_loader.BindNewPipeAndPassReceiver(),
        /*request_id=*/1, /*options=*/0, request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    remote_url_loader_factory.FlushForTesting();
    ASSERT_EQ(remote_url_loader.is_connected(), expect_success);
  }

  bool is_remote_url_loader_connected() {
    return remote_url_loader.is_connected();
  }
  void reset_remote_url_loader() { remote_url_loader.reset(); }

 private:
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory;
  mojo::Remote<network::mojom::URLLoader> remote_url_loader;
};

// Fakes a FetchLaterLoaderFactory that may exist in renderer, which only
// delegates to `remote_fetch_later_loader_factory`.
class FakeRemoteFetchLaterLoaderFactory {
 public:
  FakeRemoteFetchLaterLoaderFactory() = default;
  FakeRemoteFetchLaterLoaderFactory(const FakeRemoteFetchLaterLoaderFactory&) =
      delete;
  FakeRemoteFetchLaterLoaderFactory& operator=(
      const FakeRemoteFetchLaterLoaderFactory&) = delete;
  ~FakeRemoteFetchLaterLoaderFactory() = default;

  mojo::PendingAssociatedReceiver<blink::mojom::FetchLaterLoaderFactory>
  BindNewEndpointAndPassDedicatedReceiver() {
    return remote_fetch_later_loader_factory_
        .BindNewEndpointAndPassDedicatedReceiver();
  }

  // Binds `remote_fetch_later_loader_` to a new URLLoader.
  void CreateLoader(const network::ResourceRequest& request,
                    bool expect_success = true) {
    remote_fetch_later_loader_factory_->CreateLoader(
        remote_fetch_later_loader_.BindNewEndpointAndPassReceiver(),
        /*request_id=*/1, /*options=*/0, request,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    remote_fetch_later_loader_factory_.FlushForTesting();
    ASSERT_EQ(remote_fetch_later_loader_.is_connected(), expect_success);
  }

  bool is_remote_fetch_later_loader_connected() {
    return remote_fetch_later_loader_.is_connected();
  }
  void reset_remote_fetch_later_loader() { remote_fetch_later_loader_.reset(); }

 private:
  mojo::AssociatedRemote<blink::mojom::FetchLaterLoaderFactory>
      remote_fetch_later_loader_factory_;
  mojo::AssociatedRemote<blink::mojom::FetchLaterLoader>
      remote_fetch_later_loader_;
};

class ConfigurableURLLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  explicit ConfigurableURLLoaderThrottle(bool deferring = false,
                                         bool canceling_before_start = false,
                                         bool canceling_before_redirect = false)
      : deferring_(deferring),
        canceling_before_start_(canceling_before_start),
        canceling_before_redirect_(canceling_before_redirect) {}

  ~ConfigurableURLLoaderThrottle() override = default;
  // Not copyable.
  ConfigurableURLLoaderThrottle(const ConfigurableURLLoaderThrottle&) = delete;
  ConfigurableURLLoaderThrottle& operator=(
      const ConfigurableURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle overrides:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    will_start_request_called_ = true;
    *defer = deferring_;
    if (canceling_before_start_) {
      delegate()->CancelWithError(net::ERR_ABORTED);
    }
  }
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& /* response_head */,
      bool* defer,
      std::vector<std::string>* /* to_be_removed_headers */,
      net::HttpRequestHeaders* /* modified_headers */,
      net::HttpRequestHeaders* /* modified_cors_exempt_headers */) override {
    will_redirect_request_called_ = true;
    *defer = deferring_;
    if (canceling_before_redirect_) {
      delegate()->CancelWithError(net::ERR_ABORTED);
    }
  }
  void WillProcessResponse(const GURL& response_url_,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override {
    will_process_response_called_ = true;
    *defer = deferring_;
  }

  bool will_start_request_called() const { return will_start_request_called_; }
  bool will_redirect_request_called() const {
    return will_redirect_request_called_;
  }
  bool will_process_response_called() const {
    return will_process_response_called_;
  }

  Delegate* delegate() { return delegate_; }

 private:
  bool will_start_request_called_ = false;
  bool will_redirect_request_called_ = false;
  bool will_process_response_called_ = false;

  const bool deferring_;
  const bool canceling_before_start_;
  const bool canceling_before_redirect_;
};

// Returns true if `arg` has a header of the given `name` and `value`.
// `arg` is an `network::mojom::URLResponseHeadPtr`.
MATCHER_P2(ResponseHasHeader,
           name,
           value,
           base::StringPrintf("Response has %sheader[%s=%s]",
                              negation ? "no " : "",
                              name,
                              value)) {
  return arg->headers->HasHeaderValue(name, value);
}

network::ResourceRequest CreateFetchLaterResourceRequest(const GURL& url) {
  network::ResourceRequest request;
  request.url = url;
  request.keepalive = true;
  request.is_fetch_later_api = true;
  request.resource_type = static_cast<int>(blink::mojom::ResourceType::kXhr);
  return request;
}

network::ResourceRequest CreateResourceRequest(
    const GURL& url,
    bool keepalive = true,
    bool is_trusted = false,
    std::optional<network::mojom::RedirectMode> redirect_mode = std::nullopt) {
  network::ResourceRequest request;
  request.url = url;
  request.keepalive = keepalive;
  request.resource_type = static_cast<int>(blink::mojom::ResourceType::kXhr);
  if (is_trusted) {
    request.trusted_params = network::ResourceRequest::TrustedParams();
  }
  if (redirect_mode) {
    request.redirect_mode = *redirect_mode;
  }
  return request;
}

network::mojom::URLResponseHeadPtr CreateResponseHead(
    const std::vector<std::pair<std::string, std::string>>& extra_headers =
        {}) {
  auto response = network::mojom::URLResponseHead::New();
  net::HttpResponseHeaders::Builder builder({1, 1}, "200 OK");
  for (const auto& [name, value] : extra_headers) {
    builder.AddHeader(name, value);
  }
  response->headers = builder.Build();
  return response;
}

net::RedirectInfo CreateRedirectInfo(const GURL& new_url) {
  net::RedirectInfo redirect_info;
  redirect_info.new_method = "GET";
  redirect_info.new_url = new_url;
  redirect_info.status_code = 301;
  return redirect_info;
}

network::mojom::EarlyHintsPtr CreateEarlyHints(
    const GURL& url,
    const std::vector<std::pair<std::string, std::string>>& extra_headers =
        {}) {
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  for (const auto& header : extra_headers) {
    response_headers->SetHeader(header.first, header.second);
  }
  return network::mojom::EarlyHints::New(
      network::PopulateParsedHeaders(response_headers.get(), url),
      network::mojom::ReferrerPolicy::kDefault,
      network::mojom::IPAddressSpace::kPublic);
}

}  // namespace

class KeepAliveURLLoaderServiceTestBase : public RenderViewHostTestHarness {
 public:
  KeepAliveURLLoaderServiceTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    network_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>(
            /*observe_loader_requests=*/true);
    // Intercepts Mojo bad-message error.
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          ASSERT_FALSE(mojo_bad_message_.has_value());
          mojo_bad_message_ = error;
        }));
    RenderViewHostTestHarness::SetUp();

    AddConnectSrcCSPToRFH(kTestRedirectRequestUrl);
  }

  void TearDown() override {
    network_url_loader_factory_ = nullptr;
    loader_service_ = nullptr;
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    mojo_bad_message_ = std::nullopt;
    RenderViewHostTestHarness::TearDown();
  }

  void ExpectMojoBadMessage(const std::string& message) {
    EXPECT_EQ(mojo_bad_message_, message);
  }

  // Asks KeepAliveURLLoaderService to bind a KeepAliveURLLoaderFactory to the
  // given `remote_url_loader_factory`.
  // More than one factory can be bound to the same service.
  void BindKeepAliveURLLoaderFactory(
      FakeRemoteURLLoaderFactory& remote_url_loader_factory) {
    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    network_url_loader_factory().Clone(factory.BindNewPipeAndPassReceiver());
    auto pending_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            factory.Unbind());

    // Remote: `remote_url_loader_factory`
    // Receiver: Held in `loader_service_`.
    auto context = loader_service().BindFactory(
        remote_url_loader_factory.BindNewPipeAndPassReceiver(),
        network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->policy_container_host()
            ->Clone());
    context->OnDidCommitNavigation(
        static_cast<RenderFrameHostImpl*>(main_rfh())->GetWeakDocumentPtr());
  }

  network::TestURLLoaderFactory::PendingRequest* GetLastPendingRequest() {
    return &network_url_loader_factory_->pending_requests()->back();
  }

  const std::vector<network::TestURLLoaderFactory::PendingRequest>&
  GetPendingRequests() const {
    return *network_url_loader_factory_->pending_requests();
  }

  void AddConnectSrcCSPToRFH(const std::string& allowed_url) {
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->policy_container_host()
        ->AddContentSecurityPolicies(network::ParseContentSecurityPolicies(
            "connect-src " + allowed_url,
            network::mojom::ContentSecurityPolicyType::kEnforce,
            network::mojom::ContentSecurityPolicySource::kMeta,
            GURL(kTestRequestUrl)));
  }

  network::TestURLLoaderFactory& network_url_loader_factory() {
    return *network_url_loader_factory_;
  }
  KeepAliveURLLoaderService& loader_service() {
    if (!loader_service_) {
      loader_service_ = std::make_unique<KeepAliveURLLoaderService>(
          main_rfh()->GetBrowserContext());
    }
    return *loader_service_;
  }
  base::test::ScopedFeatureList& feature_list() { return scoped_feature_list_; }

  TestWebContents* test_web_contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Intercepts network facotry requests instead of using production factory.
  std::unique_ptr<network::TestURLLoaderFactory> network_url_loader_factory_ =
      nullptr;
  // The test target.
  std::unique_ptr<KeepAliveURLLoaderService> loader_service_ = nullptr;
  std::optional<std::string> mojo_bad_message_;
};

class KeepAliveURLLoaderServiceTest : public KeepAliveURLLoaderServiceTestBase {
 protected:
  void SetUp() override {
    feature_list().InitWithFeatures(
        {blink::features::kKeepAliveInBrowserMigration,
         blink::features::kAttributionReportingInBrowserMigration},
        {});
    KeepAliveURLLoaderServiceTestBase::SetUp();
  }
};

TEST_F(KeepAliveURLLoaderServiceTest,
       LoadKeepAliveRequestWithInvalidFeatureAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads a keepalive request with invalid feature config:
  feature_list().Reset();
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected call to "
      "KeepAliveURLLoaderFactories::CreateLoaderAndStart()");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadFetchLaterRequestAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request), but is not
  // allowed with URLLoaderFactory.
  renderer_loader_factory.CreateLoaderAndStart(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request.is_fetch_later_api` in "
      "KeepAliveURLLoaderFactories::CreateLoaderAndStart(): must not be set");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadNonKeepaliveRequestAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads non-keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/false),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request` in "
      "KeepAliveURLLoaderFactoriesBase::CreateLoaderAndStart(): "
      "resource_request.keepalive must be true");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadTrustedRequestAndTerminate) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads trusted keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/true),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(renderer_loader_factory.is_remote_url_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request` in "
      "KeepAliveURLLoaderFactoriesBase::CreateLoaderAndStart(): "
      "resource_request.trusted_params must not be set");
}

TEST_F(KeepAliveURLLoaderServiceTest, LoadRequestAfterPageIsUnloaded) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Deletes the current RenderFrameHost and then loads a keepalive request.
  DeleteContents();
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote(),
      /*expect_success=*/true);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 1);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
}

// This test initially provides an unbind factory to KeepAliveURLLoaderService.
// After that, provides a bound factory via UpdateFactory.
TEST_F(KeepAliveURLLoaderServiceTest, LoadRequestAfterUpdateFactory) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;

  // First, bind the service with a PendingSharedURLLoaderFactory that connects
  // to nothing.
  auto unbound_factory =
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>();
  auto context = loader_service().BindFactory(
      renderer_loader_factory.BindNewPipeAndPassReceiver(),
      network::SharedURLLoaderFactory::Create(std::move(unbound_factory)),
      static_cast<RenderFrameHostImpl*>(main_rfh())
          ->policy_container_host()
          ->Clone());
  context->OnDidCommitNavigation(
      static_cast<RenderFrameHostImpl*>(main_rfh())->GetWeakDocumentPtr());
  {
    // Load a keepalive request. There should be no network loader created.
    MockReceiverURLLoaderClient renderer_loader_client;
    renderer_loader_factory.CreateLoaderAndStart(
        CreateResourceRequest(GURL(kTestRequestUrl)),
        renderer_loader_client.BindNewPipeAndPassRemote(),
        /*expect_success=*/true);
    EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  }

  // Second, update the service with a PendingSharedURLLoaderFactory that
  // connects to network loader factory as usual.
  renderer_loader_factory.reset_remote_url_loader();
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  network_url_loader_factory().Clone(factory.BindNewPipeAndPassReceiver());
  auto pending_factory = std::make_unique<blink::PendingURLLoaderFactoryBundle>(
      factory.Unbind(), blink::PendingURLLoaderFactoryBundle::SchemeMap(),
      blink::PendingURLLoaderFactoryBundle::OriginMap(),
      /*bypass_redirect_checks=*/false);
  context->UpdateFactory(
      network::SharedURLLoaderFactory::Create(std::move(pending_factory)));
  {
    MockReceiverURLLoaderClient renderer_loader_client;
    renderer_loader_factory.CreateLoaderAndStart(
        CreateResourceRequest(GURL(kTestRequestUrl)),
        renderer_loader_client.BindNewPipeAndPassRemote(),
        /*expect_success=*/true);
    EXPECT_EQ(network_url_loader_factory().NumPending(), 1);
  }
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnReceiveResponse) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveResponse:
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client,
              OnReceiveResponse(ResponseHasHeader(kTestResponseHeaderName,
                                                  kTestResponseHeaderValue),
                                _, Eq(std::nullopt)))
      .Times(1);
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       ForwardRedirectsAndResponseToAttributionRequestHelper) {
  // The Attribution Manager uses the DataDecoder service, which, when an
  // InProcessDataDecoer object exists, will route to an internal in-process
  // instance.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  // Set up the Attribution Manager.
  test_web_contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto mock_manager = std::make_unique<MockAttributionManager>();
  mock_manager->SetDataHostManager(
      std::make_unique<AttributionDataHostManagerImpl>(mock_manager.get()));
  MockAttributionManager* mock_attribution_manager = mock_manager.get();
  static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition())
      ->OverrideAttributionManagerForTesting(std::move(mock_manager));

  // Loads keepalive request.
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);
  network::ResourceRequest request =
      CreateResourceRequest(GURL(kTestRequestUrl));
  request.attribution_reporting_eligibility =
      network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger;
  renderer_loader_factory.CreateLoaderAndStart(
      std::move(request), renderer_loader_client.BindNewPipeAndPassRemote());

  // Simluates receiving a redirect in the network service.
  EXPECT_CALL(*mock_attribution_manager, HandleTrigger).Times(1);
  constexpr char kRegisterTriggerJson[] = R"json({ })json";
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead({{kAttributionReportingRegisterTriggerHeader,
                           kRegisterTriggerJson}}));

  // Simluates receiving response in the network service.
  EXPECT_CALL(*mock_attribution_manager, HandleSource).Times(1);
  constexpr char kRegisterSourceJson[] =
      R"json({"destination":"https://destination.example"})json";
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kAttributionReportingRegisterSourceHeader,
                           kRegisterSourceJson}}),
      /*body=*/{}, /*cached_metadata=*/std::nullopt);

  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveResponseAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnReceiveResponse:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveResponse(_, _, _)).Times(0);
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, std::nullopt);
  base::RunLoop().RunUntilIdle();
  // The loader should have been deleted by the service.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, DoNotForwardOnReceiveRedirect) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveRedirect:
  // Expects underlying KeepAliveURLLoader NOT forwards to
  // `renderer_loader_client`: all redirects are processed in browser, and will
  // only be forwarded after request completes/fails.
  EXPECT_CALL(renderer_loader_client,
              OnReceiveRedirect(_, ResponseHasHeader(kTestResponseHeaderName,
                                                     kTestResponseHeaderValue)))
      .Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, SizeIs(1));
  EXPECT_EQ(params[0].new_url, std::nullopt);
  EXPECT_THAT(params[0].removed_headers, IsEmpty());
  EXPECT_TRUE(params[0].modified_headers.IsEmpty());
  EXPECT_TRUE(params[0].modified_cors_exempt_headers.IsEmpty());
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectToUnSafeTargetAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving unsafe redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestUnSafeRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectWithErrorRedirectModeAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first, with error redirect_mode:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kError),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveRedirectViolatingCSPAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request that redirects first:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl), /*keepalive=*/true,
                            /*is_trusted=*/false,
                            network::mojom::RedirectMode::kFollow),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestViolatingCSPRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // Verifies URLLoader::FollowRedirect() is NOT sent to network service.
  const auto& params =
      GetLastPendingRequest()->test_url_loader->follow_redirect_params();
  EXPECT_THAT(params, IsEmpty());
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnReceiveEarlyHints) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveEarlyHints:
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnReceiveEarlyHints(_)).Times(1);
  // Simluates receiving early hints in the network service.
  GetLastPendingRequest()->client->OnReceiveEarlyHints(
      CreateEarlyHints(GURL(kTestRequestUrl)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnReceiveEarlyHintsAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnReceiveEarlyHints:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveEarlyHints(_)).Times(0);
  // Simluates receiving early hints in the network service.
  GetLastPendingRequest()->client->OnReceiveEarlyHints(
      CreateEarlyHints(GURL(kTestRequestUrl)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnUploadProgress) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnUploadProgress:
  const int64_t current_position = 5;
  const int64_t total_size = 100;
  base::OnceCallback<void()> callback;
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client,
              OnUploadProgress(Eq(current_position), Eq(total_size), _))
      .Times(1)
      .WillOnce(WithArg<2>([](base::OnceCallback<void()> callback) {
        // must be consumed.
        std::move(callback).Run();
      }));
  // Simluates receiving upload progress in the network service.
  GetLastPendingRequest()->client->OnUploadProgress(
      current_position, total_size, std::move(callback));
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnTransferSizeUpdated) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnTransferSizeUpdated:
  const int32_t size_diff = 5;
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnTransferSizeUpdated(Eq(size_diff)))
      .Times(1);
  // Simluates receiving transfer size update in the network service.
  GetLastPendingRequest()->client->OnTransferSizeUpdated(size_diff);
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest,
       OnTransferSizeUpdatedAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnTransferSizeUpdated:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnTransferSizeUpdated(_)).Times(0);
  // Simluates receiving transfer size update in the network service.
  const int32_t size_diff = 5;
  GetLastPendingRequest()->client->OnTransferSizeUpdated(size_diff);
  base::RunLoop().RunUntilIdle();
}

TEST_F(KeepAliveURLLoaderServiceTest, ForwardOnComplete) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnComplete:
  const network::URLLoaderCompletionStatus status{net::OK};
  // Expects underlying KeepAliveURLLoader forwards to `renderer_loader_client`.
  EXPECT_CALL(renderer_loader_client, OnComplete(Eq(status))).Times(1);
  // Simluates receiving completion status in the network service.
  GetLastPendingRequest()->client->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  // The KeepAliveURLLoader should have been deleted.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, OnCompleteAfterRendererIsDisconnected) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // Disconnects and unbinds the receiver client & remote loader.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // OnComplete:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  // Simluates receiving completion status in the network service.
  const network::URLLoaderCompletionStatus status{net::OK};
  GetLastPendingRequest()->client->OnComplete(status);
  base::RunLoop().RunUntilIdle();
  // The KeepAliveURLLoader should have been deleted.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest, RendererDisconnectedBeforeOnComplete) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  ASSERT_EQ(network_url_loader_factory().NumPending(), 1);
  ASSERT_EQ(loader_service().NumLoadersForTesting(), 1u);

  // OnReceiveResponse
  // Simluates receiving response in the network service.
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead({{kTestResponseHeaderName, kTestResponseHeaderValue}}),
      /*body=*/{}, std::nullopt);

  // Disconnects and unbinds the receiver client & remote loader before
  // OnComplete is triggered.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  base::RunLoop().RunUntilIdle();

  // The KeepAliveURLLoader should have been deleted, even if OnComplete is not
  // triggered.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       RendererConnectedAndThrottleCancelLoaderBeforeStartRequest) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  loader_service().SetURLLoaderThrottlesGetterForTesting(
      base::BindRepeating([]() {
        std::vector<std::unique_ptr<blink::URLLoaderThrottle>> ret;
        ret.emplace_back(std::make_unique<ConfigurableURLLoaderThrottle>(
            /*deferring=*/false, /*canceling_before_start=*/true,
            /*canceling_before_redirect=*/false));
        return ret;
      }));

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // The KeepAliveURLLoader should NOT be cancelled by the in-browser throttle,
  // as the loader is still connected to the renderer and thus should respect
  // in-renderer throttles.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       RendererDisconnectedAndThrottleCancelLoaderBeforeStartRedirect) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  loader_service().SetURLLoaderThrottlesGetterForTesting(
      base::BindRepeating([]() {
        std::vector<std::unique_ptr<blink::URLLoaderThrottle>> ret;
        ret.emplace_back(std::make_unique<ConfigurableURLLoaderThrottle>(
            /*deferring=*/false, /*canceling_before_start=*/false,
            /*canceling_before_redirect=*/true));
        return ret;
      }));

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  // Disconnects and unbinds the receiver client & remote loader to simulate
  // the renderer gets disconnected before redirect.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);
  // Simluates receiving redirect in the network service.
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead(
          {{kTestResponseHeaderName, kTestResponseHeaderValue}}));
  base::RunLoop().RunUntilIdle();

  // The KeepAliveURLLoader should be cancelled by the in-browser throttle.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
}

TEST_F(KeepAliveURLLoaderServiceTest,
       RendererDisconnectedAndThrottleDeferLoaderBeforeStartRedirect) {
  FakeRemoteURLLoaderFactory renderer_loader_factory;
  MockReceiverURLLoaderClient renderer_loader_client;
  BindKeepAliveURLLoaderFactory(renderer_loader_factory);

  loader_service().SetURLLoaderThrottlesGetterForTesting(
      base::BindRepeating([]() {
        std::vector<std::unique_ptr<blink::URLLoaderThrottle>> ret;
        ret.emplace_back(std::make_unique<ConfigurableURLLoaderThrottle>(
            /*deferring=*/true, /*canceling_before_start=*/false,
            /*canceling_before_redirect=*/false));
        return ret;
      }));

  // Loads keepalive request:
  renderer_loader_factory.CreateLoaderAndStart(
      CreateResourceRequest(GURL(kTestRequestUrl)),
      renderer_loader_client.BindNewPipeAndPassRemote());
  // Disconnects and unbinds the receiver client & remote loader to simulate
  // the renderer gets disconnected before redirect.
  renderer_loader_client.ResetReceiver();
  renderer_loader_factory.reset_remote_url_loader();
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // OnReceiveRedirect:
  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // Expects no forwarding.
  EXPECT_CALL(renderer_loader_client, OnReceiveRedirect(_, _)).Times(0);
  EXPECT_CALL(renderer_loader_client, OnComplete(_)).Times(0);

  // As the request loading is deferred by `ConfigurableURLLoaderThrottle` from
  // the beginning, there should be no requests to the network service.
  EXPECT_THAT(GetPendingRequests(), IsEmpty());
}

class FetchLaterKeepAliveURLLoaderServiceTest
    : public KeepAliveURLLoaderServiceTestBase {
 protected:
  void SetUp() override {
    feature_list().InitWithFeatures(
        {blink::features::kFetchLaterAPI,
         blink::features::kAttributionReportingInBrowserMigration},
        {});
    KeepAliveURLLoaderServiceTestBase::SetUp();
  }

  // Asks KeepAliveURLLoaderService to bind a FetchLaterLoaderFactory to the
  // given `remote_fetch_later_loader_factory`.
  // More than one factory can be bound to the same service.
  void BindFetchLaterLoaderFactory(
      FakeRemoteFetchLaterLoaderFactory& remote_fetch_later_loader_factory) {
    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    network_url_loader_factory().Clone(factory.BindNewPipeAndPassReceiver());
    auto pending_factory =
        std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
            factory.Unbind());

    // Remote: `remote_fetch_later_loader_factory`
    // Receiver: Held in `loader_service_`.
    auto context = loader_service().BindFetchLaterLoaderFactory(
        remote_fetch_later_loader_factory
            .BindNewEndpointAndPassDedicatedReceiver(),
        network::SharedURLLoaderFactory::Create(std::move(pending_factory)),
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->policy_container_host()
            ->Clone());
    context->OnDidCommitNavigation(
        static_cast<RenderFrameHostImpl*>(main_rfh())->GetWeakDocumentPtr());
  }
};

TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       LoadFetchLaterRequestWithInvalidFeatureAndTerminate) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request) under invalid
  // configuration:
  feature_list().Reset();
  feature_list().InitAndDisableFeature(blink::features::kFetchLaterAPI);
  renderer_loader_factory.CreateLoader(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)),
      /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(
      renderer_loader_factory.is_remote_fetch_later_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected call to FetchLaterLoaderFactories::CreateLoader()");
}

TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       LoadNonFetchLaterRequestAndTerminate) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads non-FetchLater keepalive request.
  renderer_loader_factory.CreateLoader(
      CreateResourceRequest(GURL(kTestRequestUrl)), /*expect_success=*/false);

  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 0u);
  EXPECT_FALSE(
      renderer_loader_factory.is_remote_fetch_later_loader_connected());
  ExpectMojoBadMessage(
      "Unexpected `resource_request.is_fetch_later_api` in "
      "FetchLaterLoaderFactories::CreateLoader(): must be set");
}

TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       LoadFetchLaterRequestAndDeferred) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request):
  renderer_loader_factory.CreateLoader(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)));

  // The KeepAliveURLLoaderService holds a deferred KeepAliveURLLoader.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // As the request is deferred, the pending URLoader in network is 0.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
}

// Creates a fetchLater request which is deferred by default. The mojo endpoints
// in renderer then gets disconnected, which should start the fetchLater
// request.
TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       LoadFetchLaterRequestAndLoaderStayAliveAfterRendererIsDisconnected) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request):
  renderer_loader_factory.CreateLoader(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)));
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // As the request is deferred, the pending URLoader in network is 0.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);

  // Simulates a renderer disconnection:
  // Disconnects and unbinds the remote loader, which should start all deferred
  // KeepAliveURLLoader.
  renderer_loader_factory.reset_remote_fetch_later_loader();
  base::RunLoop().RunUntilIdle();

  // Disconnected KeepAliveURLLoader is still alive.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 1u);
  // The network should now have created pending URLLoader.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 1);
}

// Creates a fetchLater request which is deferred by default. The mojo endpoints
// in renderer then gets disconnected, and then the loader gets dropped by
// browser due to exceeding internal timeout.
TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       LoadFetchLaterRequestAndLoaderKilledAfterRendererIsDisconnected) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request):
  renderer_loader_factory.CreateLoader(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)));
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // As the request is deferred, the pending URLoader in network is 0.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);

  // Simulates a renderer disconnection:
  // Disconnects and unbinds the remote loader, which should start all deferred
  // KeepAliveURLLoader.
  renderer_loader_factory.reset_remote_fetch_later_loader();
  base::RunLoop().RunUntilIdle();
  // Fast forwards `kDefaultDisconnectedKeepAliveURLLoaderTimeout` (30s).
  task_environment()->FastForwardBy(base::Seconds(30));

  // Disconnected KeepAliveURLLoader should be killed.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
  // The network should not create pending URLLoader.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
}

// Notifying KeepAliveURLLoaderService about shutdown should start any pending
// loaders.
TEST_F(FetchLaterKeepAliveURLLoaderServiceTest, Shutdown) {
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);

  // Loads FetchLater request (which is also keepalive request):
  renderer_loader_factory.CreateLoader(
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl)));
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // As the request is deferred, the pending URLoader in network is 0.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);

  loader_service().Shutdown();

  // The pending loader should still exist.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // There should be no disconnected loader.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
  // The network should now have created pending URLLoader.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 1);
}

// TODO(https://crbug.com/368570340)
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ForwardRedirectsAndResponseToAttributionRequestHelper \
  DISABLED_ForwardRedirectsAndResponseToAttributionRequestHelper
#else
#define MAYBE_ForwardRedirectsAndResponseToAttributionRequestHelper \
  ForwardRedirectsAndResponseToAttributionRequestHelper
#endif
TEST_F(FetchLaterKeepAliveURLLoaderServiceTest,
       MAYBE_ForwardRedirectsAndResponseToAttributionRequestHelper) {
  // The Attribution Manager uses the DataDecoder service, which, when an
  // InProcessDataDecoer object exists, will route to an internal in-process
  // instance.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;

  // Set up the Attribution Manager.
  test_web_contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto mock_manager = std::make_unique<MockAttributionManager>();
  mock_manager->SetDataHostManager(
      std::make_unique<AttributionDataHostManagerImpl>(mock_manager.get()));
  MockAttributionManager* mock_attribution_manager = mock_manager.get();
  static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition())
      ->OverrideAttributionManagerForTesting(std::move(mock_manager));

  // Loads FetchLater request (which is also keepalive request):
  FakeRemoteFetchLaterLoaderFactory renderer_loader_factory;
  BindFetchLaterLoaderFactory(renderer_loader_factory);
  network::ResourceRequest request =
      CreateFetchLaterResourceRequest(GURL(kTestRequestUrl));
  request.attribution_reporting_eligibility =
      network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger;
  renderer_loader_factory.CreateLoader(std::move(request));
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // As the request is deferred, the pending URLoader in network is 0.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 0);
  // Simulate a shutdown to start the pending request.
  loader_service().Shutdown();
  // The pending loader should still exist.
  EXPECT_EQ(loader_service().NumLoadersForTesting(), 1u);
  // There should be no disconnected loader.
  EXPECT_EQ(loader_service().NumDisconnectedLoadersForTesting(), 0u);
  // The network should now have created pending URLLoader.
  EXPECT_EQ(network_url_loader_factory().NumPending(), 1);

  // Simluates receiving a redirect in the network service.
  EXPECT_CALL(*mock_attribution_manager, HandleTrigger).Times(1);
  constexpr char kRegisterTriggerJson[] = R"json({ })json";
  GetLastPendingRequest()->client->OnReceiveRedirect(
      CreateRedirectInfo(GURL(kTestRedirectRequestUrl)),
      CreateResponseHead({{kAttributionReportingRegisterTriggerHeader,
                           kRegisterTriggerJson}}));

  // Simluates receiving response in the network service.
  EXPECT_CALL(*mock_attribution_manager, HandleSource).Times(1);
  constexpr char kRegisterSourceJson[] =
      R"json({"destination":"https://destination.example"})json";
  GetLastPendingRequest()->client->OnReceiveResponse(
      CreateResponseHead(
          {{kAttributionReportingRegisterSourceHeader, kRegisterSourceJson}}),
      /*body=*/{}, /*cached_metadata=*/std::nullopt);

  base::RunLoop().RunUntilIdle();
}

}  // namespace content
