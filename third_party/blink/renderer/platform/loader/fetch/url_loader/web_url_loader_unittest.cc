// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader_completion_status.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_loader_freeze_mode.h"
#include "third_party/blink/public/platform/web_request_peer.h"
#include "third_party/blink/public/platform/web_resource_request_sender.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/sync_load_response.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
namespace {

const char kTestURL[] = "http://foo";
const char kTestHTTPSURL[] = "https://foo";
const char kTestData[] = "blah!";

class MockResourceRequestSender : public WebResourceRequestSender {
 public:
  MockResourceRequestSender() = default;
  MockResourceRequestSender(const MockResourceRequestSender&) = delete;
  MockResourceRequestSender& operator=(const MockResourceRequestSender&) =
      delete;
  ~MockResourceRequestSender() override = default;

  // WebResourceRequestSender implementation:
  void SendSync(
      std::unique_ptr<network::ResourceRequest> request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      uint32_t loader_options,
      SyncLoadResponse* response,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
      base::TimeDelta timeout,
      const WebVector<WebString>& cors_exempt_header_list,
      base::WaitableEvent* terminate_sync_load_event,
      mojo::PendingRemote<mojom::BlobRegistry> download_to_blob_registry,
      scoped_refptr<WebRequestPeer> peer,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    *response = std::move(sync_load_response_);
  }

  int SendAsync(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      uint32_t loader_options,
      const WebVector<WebString>& cors_exempt_header_list,
      scoped_refptr<WebRequestPeer> peer,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      WebVector<std::unique_ptr<URLLoaderThrottle>> throttles,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper)
      override {
    EXPECT_FALSE(peer_);
    if (sync_load_response_.head->encoded_body_length != -1)
      EXPECT_TRUE(loader_options & network::mojom::kURLLoadOptionSynchronous);
    peer_ = std::move(peer);
    return 1;
  }

  void Cancel(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    EXPECT_FALSE(canceled_);
    canceled_ = true;

    task_runner->ReleaseSoon(FROM_HERE, std::move(peer_));
  }

  WebRequestPeer* peer() { return peer_.get(); }

  bool canceled() { return canceled_; }

  void Freeze(WebLoaderFreezeMode mode) override { freeze_mode_ = mode; }
  WebLoaderFreezeMode freeze_mode() const { return freeze_mode_; }

  void set_sync_load_response(SyncLoadResponse&& sync_load_response) {
    sync_load_response_ = std::move(sync_load_response);
  }

 private:
  scoped_refptr<WebRequestPeer> peer_;
  bool canceled_ = false;
  WebLoaderFreezeMode freeze_mode_ = WebLoaderFreezeMode::kNone;
  SyncLoadResponse sync_load_response_;
};

class FakeURLLoaderFactory final : public network::mojom::URLLoaderFactory {
 public:
  FakeURLLoaderFactory() = default;
  FakeURLLoaderFactory(const FakeURLLoaderFactory&) = delete;
  FakeURLLoaderFactory& operator=(const FakeURLLoaderFactory&) = delete;
  ~FakeURLLoaderFactory() override = default;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    NOTREACHED();
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    NOTREACHED();
  }
};

class TestWebURLLoaderClient : public WebURLLoaderClient {
 public:
  TestWebURLLoaderClient()
      : loader_(new WebURLLoader(
            /*cors_exempt_header_list=*/WebVector<WebString>(),
            /*terminate_sync_load_event=*/nullptr,
            scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
                scheduler::GetSingleThreadTaskRunnerForTesting()),
            scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
                scheduler::GetSingleThreadTaskRunnerForTesting()),
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &fake_url_loader_factory_),
            /*keep_alive_handle=*/mojo::NullRemote(),
            WebBackForwardCacheLoaderHelper())),
        delete_on_receive_redirect_(false),
        delete_on_receive_response_(false),
        delete_on_receive_data_(false),
        delete_on_finish_(false),
        delete_on_fail_(false),
        did_receive_redirect_(false),
        did_receive_response_(false),
        did_finish_(false) {}

  TestWebURLLoaderClient(const TestWebURLLoaderClient&) = delete;
  TestWebURLLoaderClient& operator=(const TestWebURLLoaderClient&) = delete;

  ~TestWebURLLoaderClient() override {
    // During the deconstruction of the `loader_`, the request context will be
    // released asynchronously and we must ensure that the request context has
    // been deleted practically before the test quits, thus, memory leak will
    // not be reported on the ASAN build. So, we call 'reset()' to trigger the
    // deconstruction, and then execute `RunUntilIdle()` to empty the task queue
    // to achieve that.
    if (loader_)
      loader_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // WebURLLoaderClient implementation:
  bool WillFollowRedirect(const WebURL& new_url,
                          const net::SiteForCookies& new_site_for_cookies,
                          const WebString& new_referrer,
                          network::mojom::ReferrerPolicy new_referrer_policy,
                          const WebString& new_method,
                          const WebURLResponse& passed_redirect_response,
                          bool& report_raw_headers,
                          std::vector<std::string>*,
                          bool insecure_scheme_was_upgraded) override {
    EXPECT_TRUE(loader_);

    // No test currently simulates mutiple redirects.
    EXPECT_FALSE(did_receive_redirect_);
    did_receive_redirect_ = true;

    if (delete_on_receive_redirect_)
      loader_.reset();

    return true;
  }

  void DidSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) override {
    EXPECT_TRUE(loader_);
  }

  void DidReceiveResponse(const WebURLResponse& response) override {
    EXPECT_TRUE(loader_);
    EXPECT_FALSE(did_receive_response_);

    did_receive_response_ = true;
    response_ = response;
    if (delete_on_receive_response_)
      loader_.reset();
  }

  void DidStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle response_body) override {
    DCHECK(!response_body_);
    DCHECK(response_body);
    response_body_ = std::move(response_body);
  }

  void DidReceiveData(const char* data, int dataLength) override {
    NOTREACHED();
  }

  void DidFinishLoading(
      base::TimeTicks finishTime,
      int64_t totalEncodedDataLength,
      int64_t totalEncodedBodyLength,
      int64_t totalDecodedBodyLength,
      bool should_report_corb_blocking,
      absl::optional<bool> pervasive_payload_requested) override {
    EXPECT_TRUE(loader_);
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    did_finish_ = true;

    if (delete_on_finish_)
      loader_.reset();
  }

  void DidFail(const WebURLError& error,
               base::TimeTicks finishTime,
               int64_t totalEncodedDataLength,
               int64_t totalEncodedBodyLength,
               int64_t totalDecodedBodyLength) override {
    EXPECT_TRUE(loader_);
    EXPECT_FALSE(did_finish_);
    error_ = error;

    if (delete_on_fail_)
      loader_.reset();
  }

  WebURLLoader* loader() { return loader_.get(); }
  void DeleteLoader() { loader_.reset(); }

  void set_delete_on_receive_redirect() { delete_on_receive_redirect_ = true; }
  void set_delete_on_receive_response() { delete_on_receive_response_ = true; }
  void set_delete_on_receive_data() { delete_on_receive_data_ = true; }
  void set_delete_on_finish() { delete_on_finish_ = true; }
  void set_delete_on_fail() { delete_on_fail_ = true; }

  bool did_receive_redirect() const { return did_receive_redirect_; }
  bool did_receive_response() const { return did_receive_response_; }
  bool did_receive_response_body() const { return !!response_body_; }
  bool did_finish() const { return did_finish_; }
  const absl::optional<WebURLError>& error() const { return error_; }
  const WebURLResponse& response() const { return response_; }

 private:
  FakeURLLoaderFactory fake_url_loader_factory_;
  std::unique_ptr<WebURLLoader> loader_;

  bool delete_on_receive_redirect_;
  bool delete_on_receive_response_;
  bool delete_on_receive_data_;
  bool delete_on_finish_;
  bool delete_on_fail_;

  bool did_receive_redirect_;
  bool did_receive_response_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  bool did_finish_;
  absl::optional<WebURLError> error_;
  WebURLResponse response_;
};

class WebURLLoaderTest : public testing::Test {
 public:
  WebURLLoaderTest() : client_(std::make_unique<TestWebURLLoaderClient>()) {
    auto sender = std::make_unique<MockResourceRequestSender>();
    sender_ = sender.get();
    client_->loader()->SetResourceRequestSenderForTesting(std::move(sender));
  }

  ~WebURLLoaderTest() override = default;

  void DoStartAsyncRequest() {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GURL(kTestURL);
    request->destination = network::mojom::RequestDestination::kEmpty;
    request->priority = net::IDLE;
    client()->loader()->LoadAsynchronously(
        std::move(request), /*url_request_extra_data=*/nullptr,
        /*no_mime_sniffing=*/false,
        std::make_unique<ResourceLoadInfoNotifierWrapper>(
            /*resource_load_info_notifier=*/nullptr),
        client());
    ASSERT_TRUE(peer());
  }

  void DoReceiveRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestURL);
    redirect_info.new_site_for_cookies =
        net::SiteForCookies::FromUrl(GURL(kTestURL));
    std::vector<std::string> removed_headers;
    peer()->OnReceivedRedirect(redirect_info,
                               network::mojom::URLResponseHead::New(),
                               &removed_headers);
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveHTTPSRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestHTTPSURL);
    redirect_info.new_site_for_cookies =
        net::SiteForCookies::FromUrl(GURL(kTestHTTPSURL));
    peer()->OnReceivedRedirect(redirect_info,
                               network::mojom::URLResponseHead::New(), nullptr);
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveResponse() {
    EXPECT_FALSE(client()->did_receive_response());
    peer()->OnReceivedResponse(network::mojom::URLResponseHead::New());
    EXPECT_TRUE(client()->did_receive_response());
  }

  void DoStartLoadingResponseBody() {
    mojo::ScopedDataPipeConsumerHandle handle_to_pass;
    MojoResult rv = mojo::CreateDataPipe(nullptr, body_handle_, handle_to_pass);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    peer()->OnStartLoadingResponseBody(std::move(handle_to_pass));
  }

  void DoCompleteRequest() {
    EXPECT_FALSE(client()->did_finish());
    DCHECK(body_handle_);
    body_handle_.reset();
    base::RunLoop().RunUntilIdle();
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = std::size(kTestData);
    status.encoded_body_length = std::size(kTestData);
    status.decoded_body_length = std::size(kTestData);
    peer()->OnCompletedRequest(status);
    EXPECT_TRUE(client()->did_finish());
    // There should be no error.
    EXPECT_FALSE(client()->error());
  }

  void DoFailRequest() {
    EXPECT_FALSE(client()->did_finish());
    DCHECK(body_handle_);
    body_handle_.reset();
    base::RunLoop().RunUntilIdle();
    network::URLLoaderCompletionStatus status(net::ERR_FAILED);
    status.encoded_data_length = std::size(kTestData);
    status.encoded_body_length = std::size(kTestData);
    status.decoded_body_length = std::size(kTestData);
    peer()->OnCompletedRequest(status);
    EXPECT_FALSE(client()->did_finish());
    ASSERT_TRUE(client()->error());
    EXPECT_EQ(net::ERR_FAILED, client()->error()->reason());
  }

  TestWebURLLoaderClient* client() { return client_.get(); }
  MockResourceRequestSender* sender() { return sender_; }
  WebRequestPeer* peer() { return sender_->peer(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  mojo::ScopedDataPipeProducerHandle body_handle_;
  std::unique_ptr<TestWebURLLoaderClient> client_;
  MockResourceRequestSender* sender_ = nullptr;
};

TEST_F(WebURLLoaderTest, Success) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoStartLoadingResponseBody();
  DoCompleteRequest();
  EXPECT_FALSE(sender()->canceled());
  EXPECT_TRUE(client()->did_receive_response_body());
}

TEST_F(WebURLLoaderTest, Redirect) {
  DoStartAsyncRequest();
  DoReceiveRedirect();
  DoReceiveResponse();
  DoStartLoadingResponseBody();
  DoCompleteRequest();
  EXPECT_FALSE(sender()->canceled());
  EXPECT_TRUE(client()->did_receive_response_body());
}

TEST_F(WebURLLoaderTest, Failure) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoStartLoadingResponseBody();
  DoFailRequest();
  EXPECT_FALSE(sender()->canceled());
}

// The client may delete the WebURLLoader during any callback from the loader.
// These tests make sure that doesn't result in a crash.
TEST_F(WebURLLoaderTest, DeleteOnReceiveRedirect) {
  client()->set_delete_on_receive_redirect();
  DoStartAsyncRequest();
  DoReceiveRedirect();
}

TEST_F(WebURLLoaderTest, DeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponse();
}

TEST_F(WebURLLoaderTest, DeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoStartLoadingResponseBody();
  DoCompleteRequest();
}

TEST_F(WebURLLoaderTest, DeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoStartLoadingResponseBody();
  DoFailRequest();
}

TEST_F(WebURLLoaderTest, DefersLoadingBeforeStart) {
  client()->loader()->Freeze(WebLoaderFreezeMode::kStrict);
  EXPECT_EQ(sender()->freeze_mode(), WebLoaderFreezeMode::kNone);
  DoStartAsyncRequest();
  EXPECT_EQ(sender()->freeze_mode(), WebLoaderFreezeMode::kStrict);
}

TEST_F(WebURLLoaderTest, ResponseIPEndpoint) {
  KURL url("http://example.test/");

  struct TestCase {
    const char* ip;
    uint16_t port;
  } cases[] = {
      {"127.0.0.1", 443},
      {"123.123.123.123", 80},
      {"::1", 22},
      {"2001:0db8:85a3:0000:0000:8a2e:0370:7334", 1337},
      {"2001:db8:85a3:0:0:8a2e:370:7334", 12345},
      {"2001:db8:85a3::8a2e:370:7334", 8080},
      {"::ffff:192.0.2.128", 8443},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.ip);

    net::IPAddress address;
    ASSERT_TRUE(address.AssignFromIPLiteral(test.ip));

    network::mojom::URLResponseHead head;
    head.remote_endpoint = net::IPEndPoint(address, test.port);

    WebURLResponse response = WebURLResponse::Create(url, head, true, -1);
    EXPECT_EQ(head.remote_endpoint, response.RemoteIPEndpoint());
  };
}

TEST_F(WebURLLoaderTest, ResponseAddressSpace) {
  KURL url("http://foo.example");

  network::mojom::URLResponseHead head;
  head.response_address_space = network::mojom::IPAddressSpace::kPrivate;

  WebURLResponse response = WebURLResponse::Create(url, head, true, -1);

  EXPECT_EQ(network::mojom::IPAddressSpace::kPrivate, response.AddressSpace());
}

TEST_F(WebURLLoaderTest, ClientAddressSpace) {
  KURL url("http://foo.example");

  network::mojom::URLResponseHead head;
  head.client_address_space = network::mojom::IPAddressSpace::kPublic;

  WebURLResponse response = WebURLResponse::Create(url, head, true, -1);

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            response.ClientAddressSpace());
}

TEST_F(WebURLLoaderTest, SSLInfo) {
  KURL url("https://test.example/");

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

  network::mojom::URLResponseHead head;
  head.ssl_info = ssl_info;
  WebURLResponse web_url_response = WebURLResponse::Create(url, head, true, -1);

  const absl::optional<net::SSLInfo>& got_ssl_info =
      web_url_response.ToResourceResponse().GetSSLInfo();
  ASSERT_TRUE(got_ssl_info.has_value());
  EXPECT_EQ(ssl_info.connection_status, got_ssl_info->connection_status);
  EXPECT_TRUE(ssl_info.cert->EqualsIncludingChain(got_ssl_info->cert.get()));
}

// Verifies that the lengths used by the PerformanceResourceTiming API are
// correctly assigned for sync XHR.
TEST_F(WebURLLoaderTest, SyncLengths) {
  static const char kBodyData[] = "Today is Thursday";
  const int kEncodedBodyLength = 30;
  const int kEncodedDataLength = 130;
  const KURL url(kTestURL);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->destination = network::mojom::RequestDestination::kEmpty;
  request->priority = net::HIGHEST;

  // Prepare a mock response
  SyncLoadResponse sync_load_response;
  sync_load_response.error_code = net::OK;
  sync_load_response.url = GURL(url);
  sync_load_response.data.Assign(WebData(kBodyData));
  ASSERT_EQ(17u, sync_load_response.data.size());
  sync_load_response.head->encoded_body_length = kEncodedBodyLength;
  sync_load_response.head->encoded_data_length = kEncodedDataLength;
  sender()->set_sync_load_response(std::move(sync_load_response));

  WebURLResponse response;
  absl::optional<WebURLError> error;
  WebData data;
  int64_t encoded_data_length = 0;
  int64_t encoded_body_length = 0;
  WebBlobInfo downloaded_blob;

  client()->loader()->LoadSynchronously(
      std::move(request), /*url_request_extra_data=*/nullptr,
      /*pass_response_pipe_to_client=*/false, /*no_mime_sniffing=*/false,
      base::TimeDelta(), nullptr, response, error, data, encoded_data_length,
      encoded_body_length, downloaded_blob,
      std::make_unique<ResourceLoadInfoNotifierWrapper>(
          /*resource_load_info_notifier=*/nullptr));

  EXPECT_EQ(kEncodedBodyLength, encoded_body_length);
  EXPECT_EQ(kEncodedDataLength, encoded_data_length);
  EXPECT_TRUE(downloaded_blob.Uuid().IsNull());
}

// Verifies that WebURLResponse::Create() copies AuthChallengeInfo to the
// response.
TEST_F(WebURLLoaderTest, AuthChallengeInfo) {
  network::mojom::URLResponseHead head;
  net::AuthChallengeInfo auth_challenge_info;
  auth_challenge_info.is_proxy = true;
  auth_challenge_info.challenge = "foobar";
  head.auth_challenge_info = auth_challenge_info;

  blink::WebURLResponse response =
      WebURLResponse::Create(KURL(), head, true, -1);
  ASSERT_TRUE(response.AuthChallengeInfo().has_value());
  EXPECT_TRUE(response.AuthChallengeInfo()->is_proxy);
  EXPECT_EQ("foobar", response.AuthChallengeInfo()->challenge);
}

}  // namespace
}  // namespace blink
