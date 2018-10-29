// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/web_url_loader_impl.h"

#include <stdint.h>
#include <string.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/fixed_received_data.h"
#include "content/public/renderer/request_peer.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/sync_load_response.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const char kTestURL[] = "http://foo";
const char kTestHTTPSURL[] = "https://foo";
const char kTestData[] = "blah!";

const char kFtpDirMimeType[] = "text/vnd.chromium.ftp-dir";
// Simple FTP directory listing.  Tests are not concerned with correct parsing,
// but rather correct cleanup when deleted while parsing.  Important details of
// this list are that it contains more than one entry that are not "." or "..".
const char kFtpDirListing[] =
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 goat\n"
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 hat";

class TestResourceDispatcher : public ResourceDispatcher {
 public:
  TestResourceDispatcher() : canceled_(false), defers_loading_(false) {}

  ~TestResourceDispatcher() override {}

  // TestDispatcher implementation:

  void StartSync(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      SyncLoadResponse* response,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      base::TimeDelta timeout,
      blink::mojom::BlobRegistryPtrInfo download_to_blob_registry,
      std::unique_ptr<RequestPeer> peer) override {
    *response = std::move(sync_load_response_);
  }

  int StartAsync(
      std::unique_ptr<network::ResourceRequest> request,
      int routing_id,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      bool is_sync,
      bool pass_response_pipe_to_peer,
      std::unique_ptr<RequestPeer> peer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      std::unique_ptr<NavigationResponseOverrideParameters>
          navigation_response_override_params,
      base::OnceClosure* continue_navigation_function) override {
    EXPECT_FALSE(peer_);
    if (sync_load_response_.info.encoded_body_length != -1)
      EXPECT_TRUE(is_sync);
    peer_ = std::move(peer);
    url_ = request->url;
    navigation_response_override_params_ =
        std::move(navigation_response_override_params);
    return 1;
  }

  void Cancel(
      int request_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    EXPECT_FALSE(canceled_);
    canceled_ = true;
  }

  RequestPeer* peer() { return peer_.get(); }

  bool canceled() { return canceled_; }

  const GURL& url() { return url_; }
  const GURL& stream_url() { return stream_url_; }

  void SetDefersLoading(int request_id, bool value) override {
    defers_loading_ = value;
  }
  bool defers_loading() const { return defers_loading_; }

  void set_sync_load_response(SyncLoadResponse&& sync_load_response) {
    sync_load_response_ = std::move(sync_load_response);
  }

  std::unique_ptr<NavigationResponseOverrideParameters>
  TakeNavigationResponseOverrideParams() {
    return std::move(navigation_response_override_params_);
  }

 private:
  std::unique_ptr<RequestPeer> peer_;
  bool canceled_;
  bool defers_loading_;
  GURL url_;
  GURL stream_url_;
  SyncLoadResponse sync_load_response_;
  std::unique_ptr<NavigationResponseOverrideParameters>
      navigation_response_override_params_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcher);
};

class FakeURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;
  ~FakeURLLoaderFactory() override = default;
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    NOTREACHED();
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeURLLoaderFactory);
};

class TestWebURLLoaderClient : public blink::WebURLLoaderClient {
 public:
  TestWebURLLoaderClient(ResourceDispatcher* dispatcher)
      : loader_(new WebURLLoaderImpl(
            dispatcher,
            blink::scheduler::WebResourceLoadingTaskRunnerHandle::
                CreateUnprioritized(
                    blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &fake_url_loader_factory_))),
        delete_on_receive_redirect_(false),
        delete_on_receive_response_(false),
        delete_on_receive_data_(false),
        delete_on_finish_(false),
        delete_on_fail_(false),
        did_receive_redirect_(false),
        did_receive_response_(false),
        did_finish_(false) {}

  ~TestWebURLLoaderClient() override {}

  // blink::WebURLLoaderClient implementation:
  bool WillFollowRedirect(const blink::WebURL& new_url,
                          const blink::WebURL& new_site_for_cookies,
                          const blink::WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const blink::WebString& new_method,
                          const blink::WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers) override {
    EXPECT_TRUE(loader_);

    // No test currently simulates mutiple redirects.
    EXPECT_FALSE(did_receive_redirect_);
    did_receive_redirect_ = true;

    if (delete_on_receive_redirect_)
      loader_.reset();

    return true;
  }

  void DidSendData(unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent) override {
    EXPECT_TRUE(loader_);
  }

  void DidReceiveResponse(const blink::WebURLResponse& response) override {
    EXPECT_TRUE(loader_);
    EXPECT_FALSE(did_receive_response_);

    did_receive_response_ = true;
    response_ = response;
    if (delete_on_receive_response_)
      loader_.reset();
  }

  void DidReceiveData(const char* data, int dataLength) override {
    EXPECT_TRUE(loader_);
    // The response should have started, but must not have finished, or failed.
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    EXPECT_FALSE(error_);

    received_data_.append(data, dataLength);

    if (delete_on_receive_data_)
      loader_.reset();
  }

  void DidFinishLoading(
      base::TimeTicks finishTime,
      int64_t totalEncodedDataLength,
      int64_t totalEncodedBodyLength,
      int64_t totalDecodedBodyLength,
      bool should_report_corb_blocking,
      const std::vector<network::cors::PreflightTimingInfo>&) override {
    EXPECT_TRUE(loader_);
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    did_finish_ = true;

    if (delete_on_finish_)
      loader_.reset();
  }

  void DidFail(const blink::WebURLError& error,
               int64_t totalEncodedDataLength,
               int64_t totalEncodedBodyLength,
               int64_t totalDecodedBodyLength) override {
    EXPECT_TRUE(loader_);
    EXPECT_FALSE(did_finish_);
    error_ = error;

    if (delete_on_fail_)
      loader_.reset();
  }

  WebURLLoaderImpl* loader() { return loader_.get(); }
  void DeleteLoader() {
    loader_.reset();
  }

  void set_delete_on_receive_redirect() { delete_on_receive_redirect_ = true; }
  void set_delete_on_receive_response() { delete_on_receive_response_ = true; }
  void set_delete_on_receive_data() { delete_on_receive_data_ = true; }
  void set_delete_on_finish() { delete_on_finish_ = true; }
  void set_delete_on_fail() { delete_on_fail_ = true; }

  bool did_receive_redirect() const { return did_receive_redirect_; }
  bool did_receive_response() const { return did_receive_response_; }
  const std::string& received_data() const { return received_data_; }
  bool did_finish() const { return did_finish_; }
  const base::Optional<blink::WebURLError>& error() const { return error_; }
  const blink::WebURLResponse& response() const { return response_; }

 private:
  FakeURLLoaderFactory fake_url_loader_factory_;
  std::unique_ptr<WebURLLoaderImpl> loader_;

  bool delete_on_receive_redirect_;
  bool delete_on_receive_response_;
  bool delete_on_receive_data_;
  bool delete_on_finish_;
  bool delete_on_fail_;

  bool did_receive_redirect_;
  bool did_receive_response_;
  std::string received_data_;
  bool did_finish_;
  base::Optional<blink::WebURLError> error_;
  blink::WebURLResponse response_;

  DISALLOW_COPY_AND_ASSIGN(TestWebURLLoaderClient);
};

class WebURLLoaderImplTest : public testing::Test {
 public:
  WebURLLoaderImplTest() {
    client_.reset(new TestWebURLLoaderClient(&dispatcher_));
  }

  ~WebURLLoaderImplTest() override {}

  void DoStartAsyncRequest() {
    blink::WebURLRequest request{GURL(kTestURL)};
    request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
    client()->loader()->LoadAsynchronously(request, client());
    ASSERT_TRUE(peer());
  }

  void DoStartAsyncRequestWithPriority(
      blink::WebURLRequest::Priority priority) {
    blink::WebURLRequest request{GURL(kTestURL)};
    request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
    request.SetPriority(priority);
    client()->loader()->LoadAsynchronously(request, client());
    ASSERT_TRUE(peer());
  }

  void DoReceiveRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestURL);
    redirect_info.new_site_for_cookies = GURL(kTestURL);
    peer()->OnReceivedRedirect(redirect_info, network::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveHTTPSRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestHTTPSURL);
    redirect_info.new_site_for_cookies = GURL(kTestHTTPSURL);
    peer()->OnReceivedRedirect(redirect_info, network::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveResponse() {
    EXPECT_FALSE(client()->did_receive_response());
    peer()->OnReceivedResponse(network::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_response());
  }

  // Assumes it is called only once for a request.
  void DoReceiveData() {
    EXPECT_EQ("", client()->received_data());
    auto size = strlen(kTestData);
    peer()->OnReceivedData(
        std::make_unique<FixedReceivedData>(kTestData, size));
    EXPECT_EQ(kTestData, client()->received_data());
  }

  void DoCompleteRequest() {
    EXPECT_FALSE(client()->did_finish());
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = arraysize(kTestData);
    status.encoded_body_length = arraysize(kTestData);
    status.decoded_body_length = arraysize(kTestData);
    peer()->OnCompletedRequest(status);
    EXPECT_TRUE(client()->did_finish());
    // There should be no error.
    EXPECT_FALSE(client()->error());
  }

  void DoFailRequest() {
    EXPECT_FALSE(client()->did_finish());
    network::URLLoaderCompletionStatus status(net::ERR_FAILED);
    status.encoded_data_length = arraysize(kTestData);
    status.encoded_body_length = arraysize(kTestData);
    status.decoded_body_length = arraysize(kTestData);
    peer()->OnCompletedRequest(status);
    EXPECT_FALSE(client()->did_finish());
    ASSERT_TRUE(client()->error());
    EXPECT_EQ(net::ERR_FAILED, client()->error()->reason());
  }

  void DoReceiveResponseFtp() {
    EXPECT_FALSE(client()->did_receive_response());
    network::ResourceResponseInfo response_info;
    response_info.mime_type = kFtpDirMimeType;
    peer()->OnReceivedResponse(response_info);
    EXPECT_TRUE(client()->did_receive_response());
  }

  void DoReceiveDataFtp() {
    auto size = strlen(kFtpDirListing);
    peer()->OnReceivedData(
        std::make_unique<FixedReceivedData>(kFtpDirListing, size));
    // The FTP delegate should modify the data the client sees.
    EXPECT_NE(kFtpDirListing, client()->received_data());
  }

  TestWebURLLoaderClient* client() { return client_.get(); }
  TestResourceDispatcher* dispatcher() { return &dispatcher_; }
  RequestPeer* peer() { return dispatcher()->peer(); }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  TestResourceDispatcher dispatcher_;
  std::unique_ptr<TestWebURLLoaderClient> client_;
};

TEST_F(WebURLLoaderImplTest, Success) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Redirect) {
  DoStartAsyncRequest();
  DoReceiveRedirect();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Failure) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
  EXPECT_FALSE(dispatcher()->canceled());
}

// The client may delete the WebURLLoader during any callback from the loader.
// These tests make sure that doesn't result in a crash.
TEST_F(WebURLLoaderImplTest, DeleteOnReceiveRedirect) {
  client()->set_delete_on_receive_redirect();
  DoStartAsyncRequest();
  DoReceiveRedirect();
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponse();
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
}

TEST_F(WebURLLoaderImplTest, DeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
}

TEST_F(WebURLLoaderImplTest, DeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
}

TEST_F(WebURLLoaderImplTest, DeleteBeforeResponseDataURL) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->LoadAsynchronously(request, client());
  client()->DeleteLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client()->did_receive_response());
}

// Data URL tests.

TEST_F(WebURLLoaderImplTest, DataURL) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->LoadAsynchronously(request, client());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
  EXPECT_FALSE(client()->error());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveResponse) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_response();
  client()->loader()->LoadAsynchronously(request, client());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveData) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_data();
  client()->loader()->LoadAsynchronously(request, client());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnFinish) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_finish();
  client()->loader()->LoadAsynchronously(request, client());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, DataURLDefersLoading) {
  blink::WebURLRequest request(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->LoadAsynchronously(request, client());

  // setDefersLoading() might be called with either false or true in no
  // specific order. The user of the API will not have sufficient information
  // about the WebURLLoader's internal state, so the latter gracefully needs to
  // handle calling setDefersLoading any number of times with any values from
  // any point in time.

  client()->loader()->SetDefersLoading(false);
  client()->loader()->SetDefersLoading(true);
  client()->loader()->SetDefersLoading(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client()->did_finish());

  client()->loader()->SetDefersLoading(false);
  client()->loader()->SetDefersLoading(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(client()->did_finish());

  client()->loader()->SetDefersLoading(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client()->did_finish());

  client()->loader()->SetDefersLoading(true);
  client()->loader()->SetDefersLoading(false);
  client()->loader()->SetDefersLoading(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client()->did_finish());

  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_FALSE(client()->error());
}

TEST_F(WebURLLoaderImplTest, DefersLoadingBeforeStart) {
  client()->loader()->SetDefersLoading(true);
  EXPECT_FALSE(dispatcher()->defers_loading());
  DoStartAsyncRequest();
  EXPECT_TRUE(dispatcher()->defers_loading());
}

// FTP integration tests.  These are focused more on safe deletion than correct
// parsing of FTP responses.

TEST_F(WebURLLoaderImplTest, Ftp) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();

  // No data should have been received.
  EXPECT_EQ("", client()->received_data());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveFirstData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();

  EXPECT_NE("", client()->received_data());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveMoreData) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();

  // Directory listings are only parsed once the request completes, so this will
  // cancel in DoReceiveDataFtp, before the request finishes.
  client()->set_delete_on_receive_data();

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = arraysize(kTestData);
  status.encoded_body_length = arraysize(kTestData);
  status.decoded_body_length = arraysize(kTestData);
  peer()->OnCompletedRequest(status);
  EXPECT_FALSE(client()->did_finish());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoFailRequest();
}

// Checks that the navigation response override parameters provided on
// navigation commit are properly applied.
TEST_F(WebURLLoaderImplTest, BrowserSideNavigationCommit) {
  // Initialize the request and the stream override.
  const GURL kNavigationURL = GURL(kTestURL);
  const std::string kMimeType = "text/html";
  blink::WebURLRequest request(kNavigationURL);
  request.SetFrameType(network::mojom::RequestContextFrameType::kTopLevel);
  request.SetRequestContext(blink::mojom::RequestContextType::FRAME);
  std::unique_ptr<NavigationResponseOverrideParameters> response_override(
      new NavigationResponseOverrideParameters());
  response_override->response.mime_type = kMimeType;
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_navigation_response_override(std::move(response_override));
  request.SetExtraData(std::move(extra_data));

  client()->loader()->LoadAsynchronously(request, client());

  ASSERT_TRUE(peer());
  EXPECT_EQ(kNavigationURL, dispatcher()->url());
  EXPECT_FALSE(client()->did_receive_response());

  response_override = dispatcher()->TakeNavigationResponseOverrideParams();
  ASSERT_TRUE(response_override);
  peer()->OnReceivedResponse(response_override->response);

  EXPECT_TRUE(client()->did_receive_response());

  // The response info should have been overriden.
  ASSERT_FALSE(client()->response().IsNull());
  EXPECT_EQ(kMimeType, client()->response().MimeType().Latin1());

  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(dispatcher()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, ResponseIPAddress) {
  GURL url("http://example.test/");

  struct TestCase {
    const char* ip;
    const char* expected;
  } cases[] = {
      {"127.0.0.1", "127.0.0.1"},
      {"123.123.123.123", "123.123.123.123"},
      {"::1", "[::1]"},
      {"2001:0db8:85a3:0000:0000:8a2e:0370:7334",
       "[2001:0db8:85a3:0000:0000:8a2e:0370:7334]"},
      {"2001:db8:85a3:0:0:8a2e:370:7334", "[2001:db8:85a3:0:0:8a2e:370:7334]"},
      {"2001:db8:85a3::8a2e:370:7334", "[2001:db8:85a3::8a2e:370:7334]"},
      {"::ffff:192.0.2.128", "[::ffff:192.0.2.128]"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(test.ip);
    network::ResourceResponseInfo info;
    info.socket_address = net::HostPortPair(test.ip, 443);
    blink::WebURLResponse response;
    WebURLLoaderImpl::PopulateURLResponse(url, info, &response, true, -1);
    EXPECT_EQ(test.expected, response.RemoteIPAddress().Utf8());
  };
}

TEST_F(WebURLLoaderImplTest, ResponseCert) {
  GURL url("https://test.example/");

  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles(
      {"subjectAltName_sanity_check.pem", "root_ca_cert.pem"}, &certs));
  ASSERT_EQ(2U, certs.size());

  base::StringPiece cert0_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());
  base::StringPiece cert1_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer());

  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::X509Certificate::CreateFromDERCertChain({cert0_der, cert1_der});
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl_info.connection_status);

  network::ResourceResponseInfo info;
  info.ssl_info = ssl_info;
  blink::WebURLResponse web_url_response;
  WebURLLoaderImpl::PopulateURLResponse(url, info, &web_url_response, true, -1);

  blink::WebURLResponse::WebSecurityDetails security_details =
      web_url_response.SecurityDetailsForTesting();
  EXPECT_EQ("TLS 1.2", security_details.protocol);
  EXPECT_EQ("127.0.0.1", security_details.subject_name);
  EXPECT_EQ("127.0.0.1", security_details.issuer);
  ASSERT_EQ(3U, security_details.san_list.size());
  EXPECT_EQ("test.example", security_details.san_list[0]);
  EXPECT_EQ("127.0.0.2", security_details.san_list[1]);
  EXPECT_EQ("fe80::1", security_details.san_list[2]);
  EXPECT_EQ(certs[0]->valid_start().ToTimeT(), security_details.valid_from);
  EXPECT_EQ(certs[0]->valid_expiry().ToTimeT(), security_details.valid_to);
  ASSERT_EQ(2U, security_details.certificate.size());
  EXPECT_EQ(blink::WebString::FromLatin1(std::string(cert0_der)),
            security_details.certificate[0]);
  EXPECT_EQ(blink::WebString::FromLatin1(std::string(cert1_der)),
            security_details.certificate[1]);
}

TEST_F(WebURLLoaderImplTest, ResponseCertWithNoSANs) {
  GURL url("https://test.example/");

  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles({"multi-root-B-by-C.pem"}, &certs));
  ASSERT_EQ(1U, certs.size());

  base::StringPiece cert0_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());

  net::SSLInfo ssl_info;
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl_info.connection_status);
  ssl_info.cert = certs[0];
  network::ResourceResponseInfo info;
  info.ssl_info = ssl_info;
  blink::WebURLResponse web_url_response;
  WebURLLoaderImpl::PopulateURLResponse(url, info, &web_url_response, true, -1);

  blink::WebURLResponse::WebSecurityDetails security_details =
      web_url_response.SecurityDetailsForTesting();
  EXPECT_EQ("TLS 1.2", security_details.protocol);
  EXPECT_EQ("B CA - Multi-root", security_details.subject_name);
  EXPECT_EQ("C CA - Multi-root", security_details.issuer);
  EXPECT_EQ(0U, security_details.san_list.size());
  EXPECT_EQ(certs[0]->valid_start().ToTimeT(), security_details.valid_from);
  EXPECT_EQ(certs[0]->valid_expiry().ToTimeT(), security_details.valid_to);
  ASSERT_EQ(1U, security_details.certificate.size());
  EXPECT_EQ(blink::WebString::FromLatin1(std::string(cert0_der)),
            security_details.certificate[0]);
}

// Verifies that the lengths used by the PerformanceResourceTiming API are
// correctly assigned for sync XHR.
TEST_F(WebURLLoaderImplTest, SyncLengths) {
  static const char kBodyData[] =  "Today is Thursday";
  const int kEncodedBodyLength = 30;
  const int kEncodedDataLength = 130;
  const GURL url(kTestURL);
  blink::WebURLRequest request(url);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);

  // Prepare a mock response
  SyncLoadResponse sync_load_response;
  sync_load_response.error_code = net::OK;
  sync_load_response.url = url;
  sync_load_response.data = kBodyData;
  ASSERT_EQ(17u, sync_load_response.data.size());
  sync_load_response.info.encoded_body_length = kEncodedBodyLength;
  sync_load_response.info.encoded_data_length = kEncodedDataLength;
  dispatcher()->set_sync_load_response(std::move(sync_load_response));

  blink::WebURLResponse response;
  base::Optional<blink::WebURLError> error;
  blink::WebData data;
  int64_t encoded_data_length = 0;
  int64_t encoded_body_length = 0;
  blink::WebBlobInfo downloaded_blob;
  client()->loader()->LoadSynchronously(request, nullptr, response, error, data,
                                        encoded_data_length,
                                        encoded_body_length, downloaded_blob);

  EXPECT_EQ(kEncodedBodyLength, encoded_body_length);
  EXPECT_EQ(kEncodedDataLength, encoded_data_length);
  EXPECT_TRUE(downloaded_blob.Uuid().IsNull());
}

}  // namespace
}  // namespace content
