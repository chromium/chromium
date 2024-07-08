// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_body_drainer.h"

#include <stdint.h>

#include <cstring>
#include <set>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMagicChunkSize = 1024;
static_assert((HttpResponseBodyDrainer::kDrainBodyBufferSize %
               kMagicChunkSize) == 0,
              "chunk size needs to divide evenly into buffer size");

class CloseResultWaiter {
 public:
  CloseResultWaiter() = default;

  CloseResultWaiter(const CloseResultWaiter&) = delete;
  CloseResultWaiter& operator=(const CloseResultWaiter&) = delete;

  int WaitForResult() {
    CHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      loop_.Run();
      waiting_for_result_ = false;
    }
    return result_;
  }

  void set_result(bool result) {
    result_ = result;
    have_result_ = true;
    if (waiting_for_result_) {
      loop_.Quit();
    }
  }

 private:
  int result_ = false;
  bool have_result_ = false;
  bool waiting_for_result_ = false;
  base::RunLoop loop_;
};

class MockHttpStream : public HttpStream {
 public:
  explicit MockHttpStream(CloseResultWaiter* result_waiter)
      : result_waiter_(result_waiter) {}

  MockHttpStream(const MockHttpStream&) = delete;
  MockHttpStream& operator=(const MockHttpStream&) = delete;

  ~MockHttpStream() override = default;

  // HttpStream implementation.
  void RegisterRequest(const HttpRequestInfo* request_info) override {}
  int InitializeStream(bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override {
    return ERR_UNEXPECTED;
  }
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override {
    return ERR_UNEXPECTED;
  }
  int ReadResponseHeaders(CompletionOnceCallback callback) override {
    return ERR_UNEXPECTED;
  }

  bool IsConnectionReused() const override { return false; }
  void SetConnectionReused() override {}
  bool CanReuseConnection() const override { return can_reuse_connection_; }
  int64_t GetTotalReceivedBytes() const override { return 0; }
  int64_t GetTotalSentBytes() const override { return 0; }
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override {
    return false;
  }
  void GetSSLInfo(SSLInfo* ssl_info) override {}
  int GetRemoteEndpoint(IPEndPoint* endpoint) override {
    return ERR_UNEXPECTED;
  }

  // Mocked API
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override;
  void Close(bool not_reusable) override {
    CHECK(!closed_);
    closed_ = true;
    result_waiter_->set_result(not_reusable);
  }

  std::unique_ptr<HttpStream> RenewStreamForAuth() override { return nullptr; }

  bool IsResponseBodyComplete() const override { return is_complete_; }

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override {
    return false;
  }

  void Drain(HttpNetworkSession*) override {}

  void PopulateNetErrorDetails(NetErrorDetails* details) override { return; }

  void SetPriority(RequestPriority priority) override {}

  const std::set<std::string>& GetDnsAliases() const override {
    static const base::NoDestructor<std::set<std::string>> nullset_result;
    return *nullset_result;
  }

  std::string_view GetAcceptChViaAlps() const override { return {}; }

  // Methods to tweak/observer mock behavior:
  void set_stall_reads_forever() { stall_reads_forever_ = true; }

  void set_num_chunks(int num_chunks) { num_chunks_ = num_chunks; }

  void set_sync() { is_sync_ = true; }

  void set_is_last_chunk_zero_size() { is_last_chunk_zero_size_ = true; }

  // Sets result value of CanReuseConnection. Defaults to true.
  void set_can_reuse_connection(bool can_reuse_connection) {
    can_reuse_connection_ = can_reuse_connection;
  }

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override {}

 private:
  int ReadResponseBodyImpl(IOBuffer* buf, int buf_len);
  void CompleteRead();

  bool closed() const { return closed_; }

  const raw_ptr<CloseResultWaiter> result_waiter_;
  scoped_refptr<IOBuffer> user_buf_;
  CompletionOnceCallback callback_;
  int buf_len_ = 0;
  bool closed_ = false;
  bool stall_reads_forever_ = false;
  int num_chunks_ = 0;
  bool is_sync_ = false;
  bool is_last_chunk_zero_size_ = false;
  bool is_complete_ = false;
  bool can_reuse_connection_ = true;

  base::WeakPtrFactory<MockHttpStream> weak_factory_{this};
};

int MockHttpStream::ReadResponseBody(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  CHECK(!callback.is_null());
  CHECK(callback_.is_null());
  CHECK(buf);

  if (stall_reads_forever_)
    return ERR_IO_PENDING;

  if (is_complete_)
    return ERR_UNEXPECTED;

  if (!is_sync_) {
    user_buf_ = buf;
    buf_len_ = buf_len;
    callback_ = std::move(callback);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockHttpStream::CompleteRead,
                                  weak_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  } else {
    return ReadResponseBodyImpl(buf, buf_len);
  }
}

int MockHttpStream::ReadResponseBodyImpl(IOBuffer* buf, int buf_len) {
  if (is_last_chunk_zero_size_ && num_chunks_ == 1) {
    buf_len = 0;
  } else {
    if (buf_len > kMagicChunkSize)
      buf_len = kMagicChunkSize;
    std::memset(buf->data(), 1, buf_len);
  }
  num_chunks_--;
  if (!num_chunks_)
    is_complete_ = true;

  return buf_len;
}

void MockHttpStream::CompleteRead() {
  int result = ReadResponseBodyImpl(user_buf_.get(), buf_len_);
  user_buf_ = nullptr;
  std::move(callback_).Run(result);
}

class HttpResponseBodyDrainerTest : public TestWithTaskEnvironment {
 protected:
  HttpResponseBodyDrainerTest()
      : proxy_resolution_service_(
            ConfiguredProxyResolutionService::CreateDirect()),
        ssl_config_service_(std::make_unique<SSLConfigServiceDefaults>()),
        http_server_properties_(std::make_unique<HttpServerProperties>()),
        session_(CreateNetworkSession()),
        mock_stream_(new MockHttpStream(&result_waiter_)) {
    drainer_ = std::make_unique<HttpResponseBodyDrainer>(mock_stream_);
  }

  ~HttpResponseBodyDrainerTest() override = default;

  std::unique_ptr<HttpNetworkSession> CreateNetworkSession() {
    HttpNetworkSessionContext context;
    context.client_socket_factory = &socket_factory_;
    context.proxy_resolution_service = proxy_resolution_service_.get();
    context.ssl_config_service = ssl_config_service_.get();
    context.http_user_agent_settings = &http_user_agent_settings_;
    context.http_server_properties = http_server_properties_.get();
    context.cert_verifier = &cert_verifier_;
    context.transport_security_state = &transport_security_state_;
    context.quic_context = &quic_context_;
    return std::make_unique<HttpNetworkSession>(HttpNetworkSessionParams(),
                                                context);
  }

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<SSLConfigService> ssl_config_service_;
  StaticHttpUserAgentSettings http_user_agent_settings_ = {"*", "test-ua"};
  std::unique_ptr<HttpServerProperties> http_server_properties_;
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  QuicContext quic_context_;
  MockClientSocketFactory socket_factory_;
  const std::unique_ptr<HttpNetworkSession> session_;
  CloseResultWaiter result_waiter_;
  const raw_ptr<MockHttpStream, AcrossTasksDanglingUntriaged>
      mock_stream_;  // Owned by |drainer_|.
  std::unique_ptr<HttpResponseBodyDrainer> drainer_;
};

TEST_F(HttpResponseBodyDrainerTest, DrainBodySyncSingleOK) {
  mock_stream_->set_num_chunks(1);
  mock_stream_->set_sync();
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodySyncOK) {
  mock_stream_->set_num_chunks(3);
  mock_stream_->set_sync();
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyAsyncOK) {
  mock_stream_->set_num_chunks(3);
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

// Test the case when the final chunk is 0 bytes. This can happen when
// the final 0-byte chunk of a chunk-encoded http response is read in a last
// call to ReadResponseBody, after all data were returned from HttpStream.
TEST_F(HttpResponseBodyDrainerTest, DrainBodyAsyncEmptyChunk) {
  mock_stream_->set_num_chunks(4);
  mock_stream_->set_is_last_chunk_zero_size();
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodySyncEmptyChunk) {
  mock_stream_->set_num_chunks(4);
  mock_stream_->set_sync();
  mock_stream_->set_is_last_chunk_zero_size();
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodySizeEqualsDrainBuffer) {
  mock_stream_->set_num_chunks(
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize);
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_FALSE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyTimeOut) {
  mock_stream_->set_num_chunks(2);
  mock_stream_->set_stall_reads_forever();
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, CancelledBySession) {
  mock_stream_->set_num_chunks(2);
  mock_stream_->set_stall_reads_forever();
  session_->StartResponseDrainer(std::move(drainer_));
  // HttpNetworkSession should delete |drainer_|.
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyTooLarge) {
  int too_many_chunks =
      HttpResponseBodyDrainer::kDrainBodyBufferSize / kMagicChunkSize;
  too_many_chunks += 1;  // Now it's too large.

  mock_stream_->set_num_chunks(too_many_chunks);
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

TEST_F(HttpResponseBodyDrainerTest, DrainBodyCantReuse) {
  mock_stream_->set_num_chunks(1);
  mock_stream_->set_can_reuse_connection(false);
  session_->StartResponseDrainer(std::move(drainer_));
  EXPECT_TRUE(result_waiter_.WaitForResult());
}

}  // namespace

}  // namespace net
