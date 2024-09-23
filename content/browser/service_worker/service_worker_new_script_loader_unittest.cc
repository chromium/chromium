// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_new_script_loader.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {
namespace service_worker_new_script_loader_unittest {

const char kNormalScriptURL[] = "https://example.com/normal.js";
const char kNormalImportedScriptURL[] =
    "https://my-awesome-cdn.com/import_script.js";
const char kHistogramWriteResponseResult[] =
    "ServiceWorker.DiskCache.WriteResponseResult";

// MockHTTPServer is a utility to provide mocked responses for
// ServiceWorkerNewScriptLoader.
class MockHTTPServer {
 public:
  struct Response {
    Response(const std::string& headers, const std::string& body)
        : headers(headers), body(body) {}

    const std::string headers;
    const std::string body;
    bool has_certificate_error = false;
    net::CertStatus cert_status = 0;
  };

  void Set(const GURL& url, const Response& response) {
    responses_.erase(url);
    responses_.emplace(url, response);
  }

  const Response& Get(const GURL& url) {
    auto found = responses_.find(url);
    EXPECT_TRUE(found != responses_.end());
    return found->second;
  }

 private:
  std::map<GURL, Response> responses_;
};

// Mocks network activity. Used by URLLoaderInterceptor.
class MockNetwork {
 public:
  explicit MockNetwork(MockHTTPServer* mock_server)
      : mock_server_(mock_server) {}

  MockNetwork(const MockNetwork&) = delete;
  MockNetwork& operator=(const MockNetwork&) = delete;

  void set_to_access_network(bool access_network) {
    access_network_ = access_network;
  }

  network::ResourceRequest last_request() const { return last_request_; }

  bool InterceptNetworkRequest(URLLoaderInterceptor::RequestParams* params) {
    const network::ResourceRequest& url_request = params->url_request;
    last_request_ = url_request;
    const MockHTTPServer::Response& response =
        mock_server_->Get(url_request.url);

    // Pass the response header to the client.
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(response.headers));
    response_head->headers->GetMimeType(&response_head->mime_type);
    response_head->network_accessed = access_network_;
    if (response.has_certificate_error) {
      response_head->cert_status = response.cert_status;
    }
    response_head->parsed_headers = network::mojom::ParsedHeaders::New();

    mojo::Remote<network::mojom::URLLoaderClient>& client = params->client;
    if (response_head->headers->response_code() == 307) {
      client->OnReceiveRedirect(net::RedirectInfo(), std::move(response_head));
      return true;
    }

    mojo::ScopedDataPipeConsumerHandle consumer;
    mojo::ScopedDataPipeProducerHandle producer;
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, producer, consumer));
    MojoResult result =
        producer->WriteAllData(base::as_byte_span(response.body));
    CHECK_EQ(MOJO_RESULT_OK, result);
    client->OnReceiveResponse(std::move(response_head), std::move(consumer),
                              std::nullopt);

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
    return true;
  }

 private:
  // |mock_server_| is owned by ServiceWorkerNewScriptLoaderTest.
  const raw_ptr<MockHTTPServer> mock_server_;

  // The most recent request received.
  network::ResourceRequest last_request_;

  // Controls whether a load simulates accessing network or cache.
  bool access_network_ = false;
};

// ServiceWorkerNewScriptLoaderTest is for testing the handling of requests for
// installing service worker scripts via ServiceWorkerNewScriptLoader.
class ServiceWorkerNewScriptLoaderTest : public testing::Test {
 public:
  ServiceWorkerNewScriptLoaderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        mock_network_(&mock_server_),
        interceptor_(base::BindRepeating(&MockNetwork::InterceptNetworkRequest,
                                         base::Unretained(&mock_network_))) {}
  ~ServiceWorkerNewScriptLoaderTest() override = default;

  ServiceWorkerContextCore* context() { return helper_->context(); }

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

    mock_server_.Set(GURL(kNormalScriptURL),
                     MockHTTPServer::Response(
                         std::string("HTTP/1.1 200 OK\n"
                                     "Content-Type: text/javascript\n\n"),
                         std::string("this body came from the network")));
    mock_server_.Set(
        GURL(kNormalImportedScriptURL),
        MockHTTPServer::Response(
            std::string("HTTP/1.1 200 OK\n"
                        "Content-Type: text/javascript\n\n"),
            std::string(
                "this is an import script response body from the network")));
  }

  // Sets up ServiceWorkerRegistration and ServiceWorkerVersion. This should be
  // called before DoRequest().
  void SetUpRegistration(const GURL& script_url) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = script_url.GetWithoutFilename();
    SetUpRegistrationWithOptions(script_url, options,
                                 blink::StorageKey::CreateFirstParty(
                                     url::Origin::Create(options.scope)));
  }
  void SetUpRegistrationWithOptions(
      const GURL& script_url,
      blink::mojom::ServiceWorkerRegistrationOptions options,
      const blink::StorageKey& key) {
    registration_ =
        CreateNewServiceWorkerRegistration(context()->registry(), options, key);
    SetUpVersion(script_url);
  }

  // Promotes |version_| to |registration_|'s active version, and then resets
  // |version_| to null (as subsequent DoRequest() calls should not attempt to
  // install or update |version_|).
  void ActivateVersion() {
    version_->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNoHandler);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);
    version_ = nullptr;
  }

  // After this is called, |version_| will be a new, uninstalled version. The
  // next time DoRequest() is called, |version_| will attempt to install,
  // possibly updating if registration has an installed worker.
  void SetUpVersion(const GURL& script_url) {
    version_ = CreateNewServiceWorkerVersion(
        context()->registry(), registration_.get(), script_url,
        blink::mojom::ScriptType::kClassic);
    version_->SetStatus(ServiceWorkerVersion::NEW);
  }

  void DoRequest(const GURL& url,
                 std::unique_ptr<network::TestURLLoaderClient>* out_client,
                 std::unique_ptr<ServiceWorkerNewScriptLoader>* out_loader) {
    DCHECK(registration_);
    DCHECK(version_);

    // Dummy values.
    int request_id = 10;
    uint32_t options = 0;
    int64_t resource_id = GetNewResourceIdSync(context()->GetStorageControl());

    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.destination =
        (url == version_->script_url())
            ? network::mojom::RequestDestination::kServiceWorker
            : network::mojom::RequestDestination::kScript;

    request.mode = (url == version_->script_url())
                       ? network::mojom::RequestMode::kSameOrigin
                       : network::mojom::RequestMode::kNoCors;

    *out_client = std::make_unique<network::TestURLLoaderClient>();
    *out_loader = ServiceWorkerNewScriptLoader::CreateAndStart(
        request_id, options, request, (*out_client)->CreateRemote(), version_,
        helper_->GetNetworkFactory(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        resource_id, /*is_throttle_needed=*/false,
        /*requesting_frame_id=*/GlobalRenderFrameHostId());
  }

  // Returns false if the entry for |url| doesn't exist in the storage.
  bool VerifyStoredResponse(const GURL& url) {
    return ServiceWorkerUpdateCheckTestUtils::VerifyStoredResponse(
        LookupResourceId(url), context()->GetStorageControl(),
        mock_server_.Get(url).body);
  }

  int64_t LookupResourceId(const GURL& url) {
    return version_->script_cache_map()->LookupResourceId(url);
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  MockHTTPServer mock_server_;
  MockNetwork mock_network_;
  URLLoaderInterceptor interceptor_;

  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

TEST_F(ServiceWorkerNewScriptLoaderTest, Success) {
  base::HistogramTester histogram_tester;

  const GURL kScriptURL(kNormalScriptURL);
  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->response_body().is_valid());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(mock_server_.Get(kScriptURL).body, response);

  // WRITE_OK should be recorded once plus one as we record a single write
  // success and the end of the body.
  EXPECT_TRUE(VerifyStoredResponse(kScriptURL));
  histogram_tester.ExpectUniqueSample(kHistogramWriteResponseResult,
                                      ServiceWorkerMetrics::WRITE_OK, 2);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Success_EmptyBody) {
  base::HistogramTester histogram_tester;

  const GURL kScriptURL("https://example.com/empty.js");
  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;
  mock_server_.Set(
      kScriptURL,
      MockHTTPServer::Response(std::string("HTTP/1.1 200 OK\n"
                                           "Content-Type: text/javascript\n\n"),
                               std::string()));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->response_body().is_valid());

  // The response should also be stored in the storage.
  EXPECT_TRUE(VerifyStoredResponse(kScriptURL));
  // WRITE_OK should be recorded once as we record the end of the body.
  histogram_tester.ExpectUniqueSample(kHistogramWriteResponseResult,
                                      ServiceWorkerMetrics::WRITE_OK, 1);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Success_LargeBody) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  // Create a response that has a larger body than the script loader's buffer
  // to test chunked data write. We chose this multiplier to avoid hitting the
  // limit of mojo's data pipe buffer (it's about kReadBufferSize * 2 as of
  // now).
  const uint32_t kBodySize =
      ServiceWorkerNewScriptLoader::kReadBufferSize * 1.6;
  const GURL kScriptURL("https://example.com/large-body.js");
  mock_server_.Set(
      kScriptURL,
      MockHTTPServer::Response(std::string("HTTP/1.1 200 OK\n"
                                           "Content-Type: text/javascript\n\n"),
                               std::string(kBodySize, 'a')));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->response_body().is_valid());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(mock_server_.Get(kScriptURL).body, response);

  // The response should also be stored in the storage.
  EXPECT_TRUE(VerifyStoredResponse(kScriptURL));
  // WRITE_OK should be recorded twice plus one as we record every single write
  // success and the end of the body.
  histogram_tester.ExpectUniqueSample(kHistogramWriteResponseResult,
                                      ServiceWorkerMetrics::WRITE_OK, 3);
}

namespace {

// A URLLoaderFactory that provides access to a mojo data pipe for sending a
// response body. Can only handle one URLLoader, i.e, CreateLoaderAndStart()
// can be called only once.
class BodyDataPipeTestURLLoaderFactory final
    : public network::mojom::URLLoaderFactory {
 public:
  BodyDataPipeTestURLLoaderFactory() = default;

  mojo::ScopedDataPipeProducerHandle TakeBody() {
    DCHECK(body_producer_);
    return std::move(body_producer_);
  }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n"
                                          "Content-Type: text/javascript\r\n"));
    response_head->headers->GetMimeType(&response_head->mime_type);
    response_head->parsed_headers = network::mojom::ParsedHeaders::New();

    mojo::ScopedDataPipeConsumerHandle body_consumer;
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::CreateDataPipe(nullptr, body_producer_, body_consumer));

    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(pending_client));

    client->OnReceiveResponse(std::move(response_head),
                              std::move(body_consumer),
                              /*cached_metadata=*/std::nullopt);

    network::URLLoaderCompletionStatus status;
    status.error_code = net::OK;
    client->OnComplete(status);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  mojo::ScopedDataPipeProducerHandle body_producer_;
};

}  // namespace

// Regression test for https://crbug.com/1312995.
TEST_F(ServiceWorkerNewScriptLoaderTest, Success_ClientConsumeBodyLater) {
  const GURL kScriptURL("https://example.com/large-body.js");
  const std::string kBody(ServiceWorkerNewScriptLoader::kReadBufferSize, 'a');

  SetUpRegistration(kScriptURL);
  ASSERT_TRUE(registration_);
  ASSERT_TRUE(version_);

  network::ResourceRequest request;
  request.url = kScriptURL;
  request.method = "GET";
  request.destination = network::mojom::RequestDestination::kServiceWorker;
  request.mode = network::mojom::RequestMode::kSameOrigin;

  BodyDataPipeTestURLLoaderFactory loader_factory;
  auto shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory);

  network::TestURLLoaderClient client = network::TestURLLoaderClient();
  auto loader = ServiceWorkerNewScriptLoader::CreateAndStart(
      /*request_id=*/10, /*options=*/0, request, client.CreateRemote(),
      version_, shared_loader_factory,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      /*cache_resource_id=*/5,
      /*is_throttle_needed=*/false,
      /*requesting_frame_id=*/GlobalRenderFrameHostId());

  client.RunUntilResponseReceived();
  ASSERT_TRUE(client.has_received_response());
  ASSERT_TRUE(client.response_body().is_valid());

  // Keep writing body until ServiceWorkerNewScriptLoader's client producer
  // data pipe becomes full.
  mojo::ScopedDataPipeProducerHandle body_producer = loader_factory.TakeBody();
  size_t total_bytes_written = 0;
  while (true) {
    size_t actually_written_bytes = 0;
    MojoResult result = body_producer->WriteData(base::as_byte_span(kBody),
                                                 MOJO_WRITE_DATA_FLAG_NONE,
                                                 actually_written_bytes);
    if (result != MOJO_RESULT_OK) {
      ASSERT_EQ(result, MOJO_RESULT_SHOULD_WAIT);
      break;
    }
    total_bytes_written += actually_written_bytes;
    // Make sure ServiceWorkerNewScriptLoader have a chance to write data to the
    // client's producer data pipe. This should not enter an infinite loop.
    base::RunLoop().RunUntilIdle();
  }

  // Close the body data pipe so that ReadDataPipe() can finish.
  body_producer.reset();

  std::string response = ReadDataPipe(client.response_body_release());
  ASSERT_EQ(response.size(), total_bytes_written);

  client.RunUntilComplete();
  ASSERT_EQ(net::OK, client.completion_status().error_code);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_404) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  const GURL kScriptURL("https://example.com/nonexistent.js");
  mock_server_.Set(kScriptURL, MockHTTPServer::Response(
                                   std::string("HTTP/1.1 404 Not Found\n\n"),
                                   std::string()));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because of the 404 response.
  EXPECT_EQ(net::ERR_INVALID_RESPONSE, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_Redirect) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  const GURL kScriptURL("https://example.com/redirect.js");
  mock_server_.Set(
      kScriptURL,
      MockHTTPServer::Response(
          std::string("HTTP/1.1 307 Temporary Redirect\n\n"), std::string()));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because of the redirected response.
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_CertificateError) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  // Serve a response with a certificate error.
  const GURL kScriptURL("https://example.com/certificate-error.js");
  MockHTTPServer::Response response(std::string("HTTP/1.1 200 OK\n\n"),
                                    std::string("body"));
  response.has_certificate_error = true;
  response.cert_status = net::CERT_STATUS_DATE_INVALID;
  mock_server_.Set(kScriptURL, response);
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because of the response with the certificate
  // error.
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_NoMimeType) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  const GURL kScriptURL("https://example.com/no-mime-type.js");
  mock_server_.Set(kScriptURL, MockHTTPServer::Response(
                                   std::string("HTTP/1.1 200 OK\n\n"),
                                   std::string("body with no MIME type")));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because of the response with no MIME type.
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_BadMimeType) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  const GURL kScriptURL("https://example.com/bad-mime-type.js");
  mock_server_.Set(kScriptURL, MockHTTPServer::Response(
                                   std::string("HTTP/1.1 200 OK\n"
                                               "Content-Type: text/css\n\n"),
                                   std::string("body with bad MIME type")));
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because of the response with the bad MIME
  // type.
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Success_PathRestriction) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  // |kScope| is not under the default scope ("/out-of-scope/"), but the
  // Service-Worker-Allowed header allows it.
  const GURL kScriptURL("https://example.com/out-of-scope/normal.js");
  const GURL kScope("https://example.com/in-scope/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  mock_server_.Set(kScriptURL,
                   MockHTTPServer::Response(
                       std::string("HTTP/1.1 200 OK\n"
                                   "Content-Type: text/javascript\n"
                                   "Service-Worker-Allowed: /in-scope/\n\n"),
                       std::string("٩( ’ω’ )و I'm body!")));
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  SetUpRegistrationWithOptions(kScriptURL, options, kKey);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->response_body().is_valid());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(mock_server_.Get(kScriptURL).body, response);

  // WRITE_OK should be recorded once plus one as we record a single write
  // success and the end of the body.
  EXPECT_TRUE(VerifyStoredResponse(kScriptURL));
  histogram_tester.ExpectUniqueSample(kHistogramWriteResponseResult,
                                      ServiceWorkerMetrics::WRITE_OK, 2);
}

TEST_F(ServiceWorkerNewScriptLoaderTest,
       Fail_ModuleServiceWorker_PathRestriction) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  // |kScope| is not under the default scope ("/out-of-scope/"), but the
  // Service-Worker-Allowed header allows it.
  const GURL kImportedScriptURL(kNormalImportedScriptURL);
  const GURL kScope("https://example.com/in-scope/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  mock_server_.Set(
      kImportedScriptURL,
      MockHTTPServer::Response(std::string("HTTP/1.1 200 OK\n"
                                           "Content-Type: text/javascript\n\n"),
                               std::string("٩( ’ω’ )و I'm body!")));
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  options.type = blink::mojom::ScriptType::kModule;
  SetUpRegistrationWithOptions(kImportedScriptURL, options, kKey);
  DoRequest(kImportedScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);

  // The client should have received the response.
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->response_body().is_valid());
  std::string response;
  EXPECT_TRUE(
      mojo::BlockingCopyToString(client->response_body_release(), &response));
  EXPECT_EQ(mock_server_.Get(kImportedScriptURL).body, response);

  // WRITE_OK should be recorded once plus one as we record a single write
  // success and the end of the body.
  EXPECT_TRUE(VerifyStoredResponse(kImportedScriptURL));
  histogram_tester.ExpectUniqueSample(kHistogramWriteResponseResult,
                                      ServiceWorkerMetrics::WRITE_OK, 2);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_PathRestriction) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  // |kScope| is not under the default scope ("/out-of-scope/") and the
  // Service-Worker-Allowed header is not specified.
  const GURL kScriptURL("https://example.com/out-of-scope/normal.js");
  const GURL kScope("https://example.com/in-scope/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  mock_server_.Set(
      kScriptURL,
      MockHTTPServer::Response(std::string("HTTP/1.1 200 OK\n"
                                           "Content-Type: text/javascript\n\n"),
                               std::string()));
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  SetUpRegistrationWithOptions(kScriptURL, options, kKey);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();

  // The request should be failed because the scope is not allowed.
  EXPECT_EQ(net::ERR_INSECURE_RESPONSE, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

TEST_F(ServiceWorkerNewScriptLoaderTest, Error_RedundantWorker) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  const GURL kScriptURL(kNormalScriptURL);
  SetUpRegistration(kScriptURL);
  DoRequest(kScriptURL, &client, &loader);

  // Make the service worker redundant.
  version_->Doom();
  ASSERT_TRUE(version_->is_redundant());

  client->RunUntilComplete();

  // The request should be aborted.
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
  EXPECT_FALSE(client->has_received_response());

  // The response shouldn't be stored in the storage.
  EXPECT_FALSE(VerifyStoredResponse(kScriptURL));
  // No sample should be recorded since a write didn't occur.
  histogram_tester.ExpectTotalCount(kHistogramWriteResponseResult, 0);
}

// Tests that EmbeddedWorkerInstance's |network_accessed_for_script_| flag is
// set when the script loader accesses network. This flag is used to enforce the
// 24 hour cache validation.
TEST_F(ServiceWorkerNewScriptLoaderTest, AccessedNetwork) {
  const GURL kScriptURL(kNormalScriptURL);
  const GURL kImportedScriptURL(kNormalImportedScriptURL);
  std::unique_ptr<network::TestURLLoaderClient> client;
  std::unique_ptr<ServiceWorkerNewScriptLoader> loader;

  SetUpRegistration(kScriptURL);

  // Install the main script. The network accessed flag should be flipped on.
  version_->embedded_worker()->network_accessed_for_script_ = false;
  mock_network_.set_to_access_network(true);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_TRUE(version_->embedded_worker()->network_accessed_for_script());

  // Install the imported script. The network accessed flag should be unchanged,
  // as it's only meant for main scripts.
  version_->embedded_worker()->network_accessed_for_script_ = false;
  mock_network_.set_to_access_network(true);
  DoRequest(kImportedScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_FALSE(version_->embedded_worker()->network_accessed_for_script());

  // Install a new main script, this time simulating coming from cache. The
  // network accessed flag should be off.
  SetUpRegistration(kScriptURL);
  version_->embedded_worker()->network_accessed_for_script_ = false;
  mock_network_.set_to_access_network(false);
  DoRequest(kScriptURL, &client, &loader);
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_FALSE(version_->embedded_worker()->network_accessed_for_script());
}

}  // namespace service_worker_new_script_loader_unittest
}  // namespace content
