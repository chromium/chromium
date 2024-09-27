// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/ssl_client_socket.h"

#include <errno.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/rsa_private_key.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_database.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/mock_client_cert_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/read_buffering_stream_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_handshake_details.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/ssl/test_ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/key_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/gurl.h"

using net::test::IsError;
using net::test::IsOk;

using testing::_;
using testing::Bool;
using testing::Combine;
using testing::Return;
using testing::Values;
using testing::ValuesIn;

namespace net {

class NetLogWithSource;

namespace {

// When passed to |MakeHashValueVector|, this will generate a key pin that is
// sha256/AA...=, and hence will cause pin validation success with the TestSPKI
// pin from transport_security_state_static.pins. ("A" is the 0th element of the
// base-64 alphabet.)
const uint8_t kGoodHashValueVectorInput = 0;

// When passed to |MakeHashValueVector|, this will generate a key pin that is
// not sha256/AA...=, and hence will cause pin validation failure with the
// TestSPKI pin.
const uint8_t kBadHashValueVectorInput = 3;

// TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
constexpr uint16_t kModernTLS12Cipher = 0xc02f;
// TLS_RSA_WITH_AES_128_GCM_SHA256
constexpr uint16_t kRSACipher = 0x009c;
// TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
constexpr uint16_t kCBCCipher = 0xc013;
// TLS_RSA_WITH_3DES_EDE_CBC_SHA
constexpr uint16_t k3DESCipher = 0x000a;

// Simulates synchronously receiving an error during Read() or Write()
class SynchronousErrorStreamSocket : public WrappedStreamSocket {
 public:
  explicit SynchronousErrorStreamSocket(std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}

  SynchronousErrorStreamSocket(const SynchronousErrorStreamSocket&) = delete;
  SynchronousErrorStreamSocket& operator=(const SynchronousErrorStreamSocket&) =
      delete;

  ~SynchronousErrorStreamSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  // Sets the next Read() call and all future calls to return |error|.
  // If there is already a pending asynchronous read, the configured error
  // will not be returned until that asynchronous read has completed and Read()
  // is called again.
  void SetNextReadError(int error) {
    DCHECK_GE(0, error);
    have_read_error_ = true;
    pending_read_error_ = error;
  }

  // Sets the next Write() call and all future calls to return |error|.
  // If there is already a pending asynchronous write, the configured error
  // will not be returned until that asynchronous write has completed and
  // Write() is called again.
  void SetNextWriteError(int error) {
    DCHECK_GE(0, error);
    have_write_error_ = true;
    pending_write_error_ = error;
  }

 private:
  bool have_read_error_ = false;
  int pending_read_error_ = OK;

  bool have_write_error_ = false;
  int pending_write_error_ = OK;
};

int SynchronousErrorStreamSocket::Read(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  if (have_read_error_)
    return pending_read_error_;
  return transport_->Read(buf, buf_len, std::move(callback));
}

int SynchronousErrorStreamSocket::ReadIfReady(IOBuffer* buf,
                                              int buf_len,
                                              CompletionOnceCallback callback) {
  if (have_read_error_)
    return pending_read_error_;
  return transport_->ReadIfReady(buf, buf_len, std::move(callback));
}

int SynchronousErrorStreamSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  if (have_write_error_)
    return pending_write_error_;
  return transport_->Write(buf, buf_len, std::move(callback),
                           traffic_annotation);
}

// FakeBlockingStreamSocket wraps an existing StreamSocket and simulates the
// underlying transport needing to complete things asynchronously in a
// deterministic manner (e.g.: independent of the TestServer and the OS's
// semantics).
class FakeBlockingStreamSocket : public WrappedStreamSocket {
 public:
  explicit FakeBlockingStreamSocket(std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}
  ~FakeBlockingStreamSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  int pending_read_result() const { return pending_read_result_; }
  IOBuffer* pending_read_buf() const { return pending_read_buf_.get(); }

  // Blocks read results on the socket. Reads will not complete until
  // UnblockReadResult() has been called and a result is ready from the
  // underlying transport. Note: if BlockReadResult() is called while there is a
  // hanging asynchronous Read(), that Read is blocked.
  void BlockReadResult();
  void UnblockReadResult();

  // Replaces the pending read with |data|. Returns true on success or false if
  // the caller's reads were too small.
  bool ReplaceReadResult(const std::string& data);

  // Waits for the blocked Read() call to be complete at the underlying
  // transport.
  void WaitForReadResult();

  // Causes the next call to Write() to return ERR_IO_PENDING, not beginning the
  // underlying transport until UnblockWrite() has been called. Note: if there
  // is a pending asynchronous write, it is NOT blocked. For purposes of
  // blocking writes, data is considered to have reached the underlying
  // transport as soon as Write() is called.
  void BlockWrite();
  void UnblockWrite();

  // Waits for the blocked Write() call to be scheduled.
  void WaitForWrite();

 private:
  // Handles completion from the underlying transport read.
  void OnReadCompleted(int result);

  // Handles async completion of ReadIfReady().
  void CompleteReadIfReady(scoped_refptr<IOBuffer> buffer, int rv);

  // Finishes the current read.
  void ReturnReadResult();

  // Callback for writes.
  void CallPendingWriteCallback(int result);

  // True if read callbacks are blocked.
  bool should_block_read_ = false;

  // Used to buffer result returned by a completed ReadIfReady().
  std::string read_if_ready_buf_;

  // Non-null if there is a pending ReadIfReady().
  CompletionOnceCallback read_if_ready_callback_;

  // The buffer for the pending read, or NULL if not consumed.
  scoped_refptr<IOBuffer> pending_read_buf_;

  // The size of the pending read buffer, or -1 if not set.
  int pending_read_buf_len_ = -1;

  // The user callback for the pending read call.
  CompletionOnceCallback pending_read_callback_;

  // The result for the blocked read callback, or ERR_IO_PENDING if not
  // completed.
  int pending_read_result_ = ERR_IO_PENDING;

  // WaitForReadResult() wait loop.
  std::unique_ptr<base::RunLoop> read_loop_;

  // True if write calls are blocked.
  bool should_block_write_ = false;

  // The buffer for the pending write, or NULL if not scheduled.
  scoped_refptr<IOBuffer> pending_write_buf_;

  // The callback for the pending write call.
  CompletionOnceCallback pending_write_callback_;

  // The length for the pending write, or -1 if not scheduled.
  int pending_write_len_ = -1;

  // WaitForWrite() wait loop.
  std::unique_ptr<base::RunLoop> write_loop_;
};

int FakeBlockingStreamSocket::Read(IOBuffer* buf,
                                   int len,
                                   CompletionOnceCallback callback) {
  DCHECK(!pending_read_buf_);
  DCHECK(pending_read_callback_.is_null());
  DCHECK_EQ(ERR_IO_PENDING, pending_read_result_);
  DCHECK(!callback.is_null());

  int rv = transport_->Read(
      buf, len,
      base::BindOnce(&FakeBlockingStreamSocket::OnReadCompleted,
                     base::Unretained(this)));
  if (rv == ERR_IO_PENDING || should_block_read_) {
    // Save the callback to be called later.
    pending_read_buf_ = buf;
    pending_read_buf_len_ = len;
    pending_read_callback_ = std::move(callback);
    // Save the read result.
    if (rv != ERR_IO_PENDING) {
      OnReadCompleted(rv);
      rv = ERR_IO_PENDING;
    }
  }
  return rv;
}

int FakeBlockingStreamSocket::ReadIfReady(IOBuffer* buf,
                                          int len,
                                          CompletionOnceCallback callback) {
  if (!read_if_ready_buf_.empty()) {
    // If ReadIfReady() is used, asynchronous reads with a large enough buffer
    // and no BlockReadResult() are supported by this class. Explicitly check
    // that |should_block_read_| doesn't apply and |len| is greater than the
    // size of the buffered data.
    CHECK(!should_block_read_);
    CHECK_GE(len, static_cast<int>(read_if_ready_buf_.size()));
    int rv = read_if_ready_buf_.size();
    memcpy(buf->data(), read_if_ready_buf_.data(), rv);
    read_if_ready_buf_.clear();
    return rv;
  }
  auto buf_copy = base::MakeRefCounted<IOBufferWithSize>(len);
  int rv = Read(buf_copy.get(), len,
                base::BindOnce(&FakeBlockingStreamSocket::CompleteReadIfReady,
                               base::Unretained(this), buf_copy));
  if (rv > 0)
    memcpy(buf->data(), buf_copy->data(), rv);
  if (rv == ERR_IO_PENDING)
    read_if_ready_callback_ = std::move(callback);
  return rv;
}

int FakeBlockingStreamSocket::CancelReadIfReady() {
  DCHECK(!read_if_ready_callback_.is_null());
  read_if_ready_callback_.Reset();
  return OK;
}

int FakeBlockingStreamSocket::Write(
    IOBuffer* buf,
    int len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(buf);
  DCHECK_LE(0, len);

  if (!should_block_write_)
    return transport_->Write(buf, len, std::move(callback), traffic_annotation);

  // Schedule the write, but do nothing.
  DCHECK(!pending_write_buf_.get());
  DCHECK_EQ(-1, pending_write_len_);
  DCHECK(pending_write_callback_.is_null());
  DCHECK(!callback.is_null());
  pending_write_buf_ = buf;
  pending_write_len_ = len;
  pending_write_callback_ = std::move(callback);

  // Stop the write loop, if any.
  if (write_loop_)
    write_loop_->Quit();
  return ERR_IO_PENDING;
}

void FakeBlockingStreamSocket::BlockReadResult() {
  DCHECK(!should_block_read_);
  should_block_read_ = true;
}

void FakeBlockingStreamSocket::UnblockReadResult() {
  DCHECK(should_block_read_);
  should_block_read_ = false;

  // If the operation has since completed, return the result to the caller.
  if (pending_read_result_ != ERR_IO_PENDING)
    ReturnReadResult();
}

bool FakeBlockingStreamSocket::ReplaceReadResult(const std::string& data) {
  DCHECK(should_block_read_);
  DCHECK_NE(ERR_IO_PENDING, pending_read_result_);
  DCHECK(pending_read_buf_);
  DCHECK_NE(-1, pending_read_buf_len_);

  if (static_cast<size_t>(pending_read_buf_len_) < data.size())
    return false;

  memcpy(pending_read_buf_->data(), data.data(), data.size());
  pending_read_result_ = data.size();
  return true;
}

void FakeBlockingStreamSocket::WaitForReadResult() {
  DCHECK(should_block_read_);
  DCHECK(!read_loop_);

  if (pending_read_result_ != ERR_IO_PENDING)
    return;
  read_loop_ = std::make_unique<base::RunLoop>();
  read_loop_->Run();
  read_loop_.reset();
  DCHECK_NE(ERR_IO_PENDING, pending_read_result_);
}

void FakeBlockingStreamSocket::BlockWrite() {
  DCHECK(!should_block_write_);
  should_block_write_ = true;
}

void FakeBlockingStreamSocket::CallPendingWriteCallback(int rv) {
  std::move(pending_write_callback_).Run(rv);
}

void FakeBlockingStreamSocket::UnblockWrite() {
  DCHECK(should_block_write_);
  should_block_write_ = false;

  // Do nothing if UnblockWrite() was called after BlockWrite(),
  // without a Write() in between.
  if (!pending_write_buf_.get())
    return;

  int rv = transport_->Write(
      pending_write_buf_.get(), pending_write_len_,
      base::BindOnce(&FakeBlockingStreamSocket::CallPendingWriteCallback,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  pending_write_buf_ = nullptr;
  pending_write_len_ = -1;
  if (rv != ERR_IO_PENDING) {
    std::move(pending_write_callback_).Run(rv);
  }
}

void FakeBlockingStreamSocket::WaitForWrite() {
  DCHECK(should_block_write_);
  DCHECK(!write_loop_);

  if (pending_write_buf_.get())
    return;
  write_loop_ = std::make_unique<base::RunLoop>();
  write_loop_->Run();
  write_loop_.reset();
  DCHECK(pending_write_buf_.get());
}

void FakeBlockingStreamSocket::OnReadCompleted(int result) {
  DCHECK_EQ(ERR_IO_PENDING, pending_read_result_);
  DCHECK(!pending_read_callback_.is_null());

  pending_read_result_ = result;

  if (should_block_read_) {
    // Defer the result until UnblockReadResult is called.
    if (read_loop_)
      read_loop_->Quit();
    return;
  }

  ReturnReadResult();
}

void FakeBlockingStreamSocket::CompleteReadIfReady(scoped_refptr<IOBuffer> buf,
                                                   int rv) {
  DCHECK(read_if_ready_buf_.empty());
  DCHECK(!should_block_read_);
  if (rv > 0)
    read_if_ready_buf_ = std::string(buf->data(), buf->data() + rv);
  // The callback may be null if CancelReadIfReady() was called.
  if (!read_if_ready_callback_.is_null())
    std::move(read_if_ready_callback_).Run(rv > 0 ? OK : rv);
}

void FakeBlockingStreamSocket::ReturnReadResult() {
  int result = pending_read_result_;
  pending_read_result_ = ERR_IO_PENDING;
  pending_read_buf_ = nullptr;
  pending_read_buf_len_ = -1;
  std::move(pending_read_callback_).Run(result);
}

// CountingStreamSocket wraps an existing StreamSocket and maintains a count of
// reads and writes on the socket.
class CountingStreamSocket : public WrappedStreamSocket {
 public:
  explicit CountingStreamSocket(std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}
  ~CountingStreamSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    read_count_++;
    return transport_->Read(buf, buf_len, std::move(callback));
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    write_count_++;
    return transport_->Write(buf, buf_len, std::move(callback),
                             traffic_annotation);
  }

  int read_count() const { return read_count_; }
  int write_count() const { return write_count_; }

 private:
  int read_count_ = 0;
  int write_count_ = 0;
};

// A helper class that will delete |socket| when the callback is invoked.
class DeleteSocketCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSocketCallback(StreamSocket* socket) : socket_(socket) {}

  DeleteSocketCallback(const DeleteSocketCallback&) = delete;
  DeleteSocketCallback& operator=(const DeleteSocketCallback&) = delete;

  ~DeleteSocketCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeleteSocketCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    if (socket_) {
      delete socket_;
      socket_ = nullptr;
    } else {
      ADD_FAILURE() << "Deleting socket twice";
    }
    SetResult(result);
  }

  raw_ptr<StreamSocket, DanglingUntriaged> socket_;
};

class MockRequireCTDelegate : public TransportSecurityState::RequireCTDelegate {
 public:
  MOCK_METHOD3(IsCTRequiredForHost,
               CTRequirementLevel(std::string_view host,
                                  const X509Certificate* chain,
                                  const HashValueVector& hashes));
};

class MockSCTAuditingDelegate : public SCTAuditingDelegate {
 public:
  MOCK_METHOD(bool, IsSCTAuditingEnabled, ());
  MOCK_METHOD(void,
              MaybeEnqueueReport,
              (const net::HostPortPair&,
               const net::X509Certificate*,
               const net::SignedCertificateTimestampAndStatusList&));
};

class ManySmallRecordsHttpResponse : public test_server::HttpResponse {
 public:
  static std::unique_ptr<test_server::HttpResponse> HandleRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url != "/ssl-many-small-records") {
      return nullptr;
    }

    // Write ~26K of data, in 1350 byte chunks
    return std::make_unique<ManySmallRecordsHttpResponse>(/*chunk_size=*/1350,
                                                          /*chunk_count=*/20);
  }

  ManySmallRecordsHttpResponse(size_t chunk_size, size_t chunk_count)
      : chunk_size_(chunk_size), chunk_count_(chunk_count) {}

  void SendResponse(
      base::WeakPtr<test_server::HttpResponseDelegate> delegate) override {
    base::StringPairs headers = {
        {"Connection", "close"},
        {"Content-Length", base::NumberToString(chunk_size_ * chunk_count_)},
        {"Content-Type", "text/plain"}};
    delegate->SendResponseHeaders(HTTP_OK, "OK", headers);
    SendChunks(chunk_size_, chunk_count_, delegate);
  }

 private:
  static void SendChunks(
      size_t chunk_size,
      size_t chunk_count,
      base::WeakPtr<test_server::HttpResponseDelegate> delegate) {
    if (!delegate)
      return;

    if (chunk_count == 0) {
      delegate->FinishResponse();
      return;
    }

    std::string chunk(chunk_size, '*');
    // This assumes that splitting output into separate |send| calls will
    // produce separate TLS records.
    delegate->SendContents(chunk, base::BindOnce(&SendChunks, chunk_size,
                                                 chunk_count - 1, delegate));
  }

  size_t chunk_size_;
  size_t chunk_count_;
};

class SSLClientSocketTest : public PlatformTest, public WithTaskEnvironment {
 public:
  SSLClientSocketTest()
      : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        ssl_config_service_(
            std::make_unique<TestSSLConfigService>(SSLContextConfig())),
        cert_verifier_(std::make_unique<ParamRecordingMockCertVerifier>()),
        transport_security_state_(std::make_unique<TransportSecurityState>()),
        ssl_client_session_cache_(std::make_unique<SSLClientSessionCache>(
            SSLClientSessionCache::Config())),
        context_(
            std::make_unique<SSLClientContext>(ssl_config_service_.get(),
                                               cert_verifier_.get(),
                                               transport_security_state_.get(),
                                               ssl_client_session_cache_.get(),
                                               nullptr)) {
    cert_verifier_->set_default_result(OK);
    cert_verifier_->set_async(true);
  }

 protected:
  // The address of the test server, after calling StartEmbeddedTestServer().
  const AddressList& addr() const { return addr_; }

  // The hostname of the test server, after calling StartEmbeddedTestServer().
  const HostPortPair& host_port_pair() const { return host_port_pair_; }

  // The EmbeddedTestServer object, after calling StartEmbeddedTestServer().
  EmbeddedTestServer* embedded_test_server() {
    return embedded_test_server_.get();
  }

  // Starts the embedded test server with the specified parameters. Returns true
  // on success.
  bool StartEmbeddedTestServer(EmbeddedTestServer::ServerCertificate cert,
                               const SSLServerConfig& server_config) {
    embedded_test_server_ =
        std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
    embedded_test_server_->SetSSLConfig(cert, server_config);
    return FinishStartingEmbeddedTestServer();
  }

  // Starts the embedded test server with the specified parameters. Returns true
  // on success.
  bool StartEmbeddedTestServer(
      const EmbeddedTestServer::ServerCertificateConfig& cert_config,
      const SSLServerConfig& server_config) {
    embedded_test_server_ =
        std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTPS);
    embedded_test_server_->SetSSLConfig(cert_config, server_config);
    return FinishStartingEmbeddedTestServer();
  }

  bool FinishStartingEmbeddedTestServer() {
    RegisterEmbeddedTestServerHandlers(embedded_test_server_.get());
    if (!embedded_test_server_->Start()) {
      LOG(ERROR) << "Could not start EmbeddedTestServer";
      return false;
    }

    if (!embedded_test_server_->GetAddressList(&addr_)) {
      LOG(ERROR) << "Could not get EmbeddedTestServer address list";
      return false;
    }
    host_port_pair_ = embedded_test_server_->host_port_pair();
    return true;
  }

  // May be overridden by the subclass to customize the EmbeddedTestServer.
  virtual void RegisterEmbeddedTestServerHandlers(EmbeddedTestServer* server) {
    server->AddDefaultHandlers(base::FilePath());
    server->RegisterRequestHandler(
        base::BindRepeating(&ManySmallRecordsHttpResponse::HandleRequest));
    server->RegisterRequestHandler(
        base::BindRepeating(&HandleSSLInfoRequest, base::Unretained(this)));
  }

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      std::unique_ptr<StreamSocket> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) {
    return socket_factory_->CreateSSLClientSocket(
        context_.get(), std::move(transport_socket), host_and_port, ssl_config);
  }

  // Create an SSLClientSocket object and use it to connect to a test server,
  // then wait for connection results. This must be called after a successful
  // StartEmbeddedTestServer() call.
  //
  // |ssl_config| The SSL configuration to use.
  // |host_port_pair| The hostname and port to use at the SSL layer. (The
  //     socket connection will still be made to |embedded_test_server_|.)
  // |result| will retrieve the ::Connect() result value.
  //
  // Returns true on success, false otherwise. Success means that the SSL
  // socket could be created and its Connect() was called, not that the
  // connection itself was a success.
  bool CreateAndConnectSSLClientSocketWithHost(
      const SSLConfig& ssl_config,
      const HostPortPair& host_port_pair,
      int* result) {
    auto transport = std::make_unique<TCPClientSocket>(
        addr_, nullptr, nullptr, NetLog::Get(), NetLogSource());
    int rv = callback_.GetResult(transport->Connect(callback_.callback()));
    if (rv != OK) {
      LOG(ERROR) << "Could not connect to test server";
      return false;
    }

    sock_ =
        CreateSSLClientSocket(std::move(transport), host_port_pair, ssl_config);
    EXPECT_FALSE(sock_->IsConnected());

    *result = callback_.GetResult(sock_->Connect(callback_.callback()));
    return true;
  }

  bool CreateAndConnectSSLClientSocket(const SSLConfig& ssl_config,
                                       int* result) {
    return CreateAndConnectSSLClientSocketWithHost(ssl_config, host_port_pair(),
                                                   result);
  }

  std::optional<SSLInfo> LastSSLInfoFromServer() {
    // EmbeddedTestServer callbacks run on another thread, so protect this
    // with a lock.
    base::AutoLock lock(server_ssl_info_lock_);
    return std::exchange(server_ssl_info_, std::nullopt);
  }

  RecordingNetLogObserver log_observer_;
  raw_ptr<ClientSocketFactory, DanglingUntriaged> socket_factory_;
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  std::unique_ptr<ParamRecordingMockCertVerifier> cert_verifier_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  std::unique_ptr<SSLClientSessionCache> ssl_client_session_cache_;
  std::unique_ptr<SSLClientContext> context_;
  std::unique_ptr<SSLClientSocket> sock_;

 private:
  static std::unique_ptr<test_server::HttpResponse> HandleSSLInfoRequest(
      SSLClientSocketTest* test,
      const test_server::HttpRequest& request) {
    if (request.relative_url != "/ssl-info") {
      return nullptr;
    }
    {
      // EmbeddedTestServer callbacks run on another thread, so protect this
      // with a lock.
      base::AutoLock lock(test->server_ssl_info_lock_);
      test->server_ssl_info_ = request.ssl_info;
    }
    return std::make_unique<test_server::BasicHttpResponse>();
  }

  std::unique_ptr<EmbeddedTestServer> embedded_test_server_;
  base::Lock server_ssl_info_lock_;
  std::optional<SSLInfo> server_ssl_info_ GUARDED_BY(server_ssl_info_lock_);
  TestCompletionCallback callback_;
  AddressList addr_;
  HostPortPair host_port_pair_;
};

enum ReadIfReadyTransport {
  // ReadIfReady() is implemented by the underlying transport.
  READ_IF_READY_SUPPORTED,
  // ReadIfReady() is not implemented by the underlying transport.
  READ_IF_READY_NOT_SUPPORTED,
};

enum ReadIfReadySSL {
  // Test reads by calling ReadIfReady() on the SSL socket.
  TEST_SSL_READ_IF_READY,
  // Test reads by calling Read() on the SSL socket.
  TEST_SSL_READ,
};

class StreamSocketWithoutReadIfReady : public WrappedStreamSocket {
 public:
  explicit StreamSocketWithoutReadIfReady(
      std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}

  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override {
    return ERR_READ_IF_READY_NOT_IMPLEMENTED;
  }

  int CancelReadIfReady() override { return ERR_READ_IF_READY_NOT_IMPLEMENTED; }
};

class ClientSocketFactoryWithoutReadIfReady : public ClientSocketFactory {
 public:
  explicit ClientSocketFactoryWithoutReadIfReady(ClientSocketFactory* factory)
      : factory_(factory) {}

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    return factory_->CreateDatagramClientSocket(bind_type, net_log, source);
  }

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override {
    return factory_->CreateTransportClientSocket(
        addresses, std::move(socket_performance_watcher),
        network_quality_estimator, net_log, source);
  }

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override {
    stream_socket = std::make_unique<StreamSocketWithoutReadIfReady>(
        std::move(stream_socket));
    return factory_->CreateSSLClientSocket(context, std::move(stream_socket),
                                           host_and_port, ssl_config);
  }

 private:
  const raw_ptr<ClientSocketFactory> factory_;
};

std::vector<uint16_t> GetTLSVersions() {
  return {SSL_PROTOCOL_VERSION_TLS1_2, SSL_PROTOCOL_VERSION_TLS1_3};
}

class SSLClientSocketVersionTest
    : public SSLClientSocketTest,
      public ::testing::WithParamInterface<uint16_t> {
 protected:
  SSLClientSocketVersionTest() = default;

  uint16_t version() const { return GetParam(); }

  SSLServerConfig GetServerConfig() {
    SSLServerConfig config;
    config.version_max = version();
    config.version_min = version();
    return config;
  }
};

// If GetParam(), try ReadIfReady() and fall back to Read() if needed.
class SSLClientSocketReadTest
    : public SSLClientSocketTest,
      public ::testing::WithParamInterface<
          std::tuple<ReadIfReadyTransport, ReadIfReadySSL, uint16_t>> {
 protected:
  SSLClientSocketReadTest() : SSLClientSocketTest() {
    if (!read_if_ready_supported()) {
      wrapped_socket_factory_ =
          std::make_unique<ClientSocketFactoryWithoutReadIfReady>(
              socket_factory_);
      socket_factory_ = wrapped_socket_factory_.get();
    }
  }

  // Convienient wrapper to call Read()/ReadIfReady() depending on whether
  // ReadyIfReady() is enabled.
  int Read(StreamSocket* socket,
           IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) {
    if (test_ssl_read_if_ready())
      return socket->ReadIfReady(buf, buf_len, std::move(callback));
    return socket->Read(buf, buf_len, std::move(callback));
  }

  // Wait for Read()/ReadIfReady() to complete.
  int WaitForReadCompletion(StreamSocket* socket,
                            IOBuffer* buf,
                            int buf_len,
                            TestCompletionCallback* callback,
                            int rv) {
    if (!test_ssl_read_if_ready())
      return callback->GetResult(rv);
    while (rv == ERR_IO_PENDING) {
      rv = callback->GetResult(rv);
      if (rv != OK)
        return rv;
      rv = socket->ReadIfReady(buf, buf_len, callback->callback());
    }
    return rv;
  }

  // Calls Read()/ReadIfReady() and waits for it to return data.
  int ReadAndWaitForCompletion(StreamSocket* socket,
                               IOBuffer* buf,
                               int buf_len) {
    TestCompletionCallback callback;
    int rv = Read(socket, buf, buf_len, callback.callback());
    return WaitForReadCompletion(socket, buf, buf_len, &callback, rv);
  }

  SSLServerConfig GetServerConfig() {
    SSLServerConfig config;
    config.version_max = version();
    config.version_min = version();
    return config;
  }

  bool test_ssl_read_if_ready() const {
    return std::get<1>(GetParam()) == TEST_SSL_READ_IF_READY;
  }

  bool read_if_ready_supported() const {
    return std::get<0>(GetParam()) == READ_IF_READY_SUPPORTED;
  }

  uint16_t version() const { return std::get<2>(GetParam()); }

 private:
  std::unique_ptr<ClientSocketFactory> wrapped_socket_factory_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SSLClientSocketReadTest,
                         Combine(Values(READ_IF_READY_SUPPORTED,
                                        READ_IF_READY_NOT_SUPPORTED),
                                 Values(TEST_SSL_READ_IF_READY, TEST_SSL_READ),
                                 ValuesIn(GetTLSVersions())));

// Verifies the correctness of GetSSLCertRequestInfo.
class SSLClientSocketCertRequestInfoTest : public SSLClientSocketVersionTest {
 protected:
  // Connects to the test server and returns the SSLCertRequestInfo reported by
  // the socket.
  scoped_refptr<SSLCertRequestInfo> GetCertRequest() {
    int rv;
    if (!CreateAndConnectSSLClientSocket(SSLConfig(), &rv)) {
      return nullptr;
    }
    EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

    auto request_info = base::MakeRefCounted<SSLCertRequestInfo>();
    sock_->GetSSLCertRequestInfo(request_info.get());
    sock_->Disconnect();
    EXPECT_FALSE(sock_->IsConnected());
    EXPECT_TRUE(host_port_pair().Equals(request_info->host_and_port));

    return request_info;
  }
};

class SSLClientSocketFalseStartTest : public SSLClientSocketTest {
 protected:
  // Creates an SSLClientSocket with |client_config| attached to a
  // FakeBlockingStreamSocket, returning both in |*out_raw_transport| and
  // |*out_sock|. The FakeBlockingStreamSocket is owned by the SSLClientSocket,
  // so |*out_raw_transport| is a raw pointer.
  //
  // The client socket will begin a connect using |callback| but stop before the
  // server's finished message is received. The finished message will be blocked
  // in |*out_raw_transport|. To complete the handshake and successfully read
  // data, the caller must unblock reads on |*out_raw_transport|. (Note that, if
  // the client successfully false started, |callback.WaitForResult()| will
  // return OK without unblocking transport reads. But Read() will still block.)
  //
  // Must be called after StartEmbeddedTestServer is called.
  void CreateAndConnectUntilServerFinishedReceived(
      const SSLConfig& client_config,
      TestCompletionCallback* callback,
      FakeBlockingStreamSocket** out_raw_transport,
      std::unique_ptr<SSLClientSocket>* out_sock) {
    CHECK(embedded_test_server());

    auto real_transport = std::make_unique<TCPClientSocket>(
        addr(), nullptr, nullptr, nullptr, NetLogSource());
    auto transport =
        std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
    int rv = callback->GetResult(transport->Connect(callback->callback()));
    EXPECT_THAT(rv, IsOk());

    FakeBlockingStreamSocket* raw_transport = transport.get();
    std::unique_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
        std::move(transport), host_port_pair(), client_config);

    // Connect. Stop before the client processes the first server leg
    // (ServerHello, etc.)
    raw_transport->BlockReadResult();
    rv = sock->Connect(callback->callback());
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    raw_transport->WaitForReadResult();

    // Release the ServerHello and wait for the client to write
    // ClientKeyExchange, etc. (A proxy for waiting for the entirety of the
    // server's leg to complete, since it may span multiple reads.)
    EXPECT_FALSE(callback->have_result());
    raw_transport->BlockWrite();
    raw_transport->UnblockReadResult();
    raw_transport->WaitForWrite();

    // And, finally, release that and block the next server leg
    // (ChangeCipherSpec, Finished).
    raw_transport->BlockReadResult();
    raw_transport->UnblockWrite();

    *out_raw_transport = raw_transport;
    *out_sock = std::move(sock);
  }

  void TestFalseStart(const SSLServerConfig& server_config,
                      const SSLConfig& client_config,
                      bool expect_false_start) {
    ASSERT_TRUE(
        StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

    TestCompletionCallback callback;
    FakeBlockingStreamSocket* raw_transport = nullptr;
    std::unique_ptr<SSLClientSocket> sock;
    ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
        client_config, &callback, &raw_transport, &sock));

    if (expect_false_start) {
      // When False Starting, the handshake should complete before receiving the
      // Change Cipher Spec and Finished messages.
      //
      // Note: callback.have_result() may not be true without waiting. The NSS
      // state machine sometimes lives on a separate thread, so this thread may
      // not yet have processed the signal that the handshake has completed.
      int rv = callback.WaitForResult();
      EXPECT_THAT(rv, IsOk());
      EXPECT_TRUE(sock->IsConnected());

      const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
      static const int kRequestTextSize =
          static_cast<int>(std::size(request_text) - 1);
      auto request_buffer =
          base::MakeRefCounted<IOBufferWithSize>(kRequestTextSize);
      memcpy(request_buffer->data(), request_text, kRequestTextSize);

      // Write the request.
      rv = callback.GetResult(sock->Write(request_buffer.get(),
                                          kRequestTextSize, callback.callback(),
                                          TRAFFIC_ANNOTATION_FOR_TESTS));
      EXPECT_EQ(kRequestTextSize, rv);

      // The read will hang; it's waiting for the peer to complete the
      // handshake, and the handshake is still blocked.
      auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
      rv = sock->Read(buf.get(), 4096, callback.callback());

      // After releasing reads, the connection proceeds.
      raw_transport->UnblockReadResult();
      rv = callback.GetResult(rv);
      EXPECT_LT(0, rv);
    } else {
      // False Start is not enabled, so the handshake will not complete because
      // the server second leg is blocked.
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(callback.have_result());
    }
  }
};

// Sends an HTTP request on the socket and reads the response. This may be used
// to ensure some data has been consumed from the server.
int MakeHTTPRequest(StreamSocket* socket, const char* path = "/") {
  std::string request = base::StringPrintf("GET %s HTTP/1.0\r\n\r\n", path);
  TestCompletionCallback callback;
  while (!request.empty()) {
    auto request_buffer =
        base::MakeRefCounted<StringIOBuffer>(std::string(request));
    int rv = callback.GetResult(
        socket->Write(request_buffer.get(), request_buffer->size(),
                      callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
    if (rv < 0) {
      return rv;
    }
    request = request.substr(rv);
  }

  auto response_buffer = base::MakeRefCounted<IOBufferWithSize>(1024);
  int rv = callback.GetResult(
      socket->Read(response_buffer.get(), 1024, callback.callback()));
  if (rv < 0) {
    return rv;
  }
  return OK;
}

// Provides a response to the 0RTT request indicating whether it was received
// as early data.
class ZeroRTTResponse : public test_server::HttpResponse {
 public:
  explicit ZeroRTTResponse(bool zero_rtt) : zero_rtt_(zero_rtt) {}

  ZeroRTTResponse(const ZeroRTTResponse&) = delete;
  ZeroRTTResponse& operator=(const ZeroRTTResponse&) = delete;

  ~ZeroRTTResponse() override = default;

  void SendResponse(
      base::WeakPtr<test_server::HttpResponseDelegate> delegate) override {
    std::string response;
    if (zero_rtt_) {
      response = "1";
    } else {
      response = "0";
    }

    // Since the EmbeddedTestServer doesn't keep the socket open by default, it
    // is explicitly kept alive to allow the remaining leg of the 0RTT handshake
    // to be received after the early data.
    delegate->SendContents(response);
  }

 private:
  bool zero_rtt_;
};

std::unique_ptr<test_server::HttpResponse> HandleZeroRTTRequest(
    const test_server::HttpRequest& request) {
  if (request.GetURL().path() != "/zerortt" || !request.ssl_info)
    return nullptr;

  return std::make_unique<ZeroRTTResponse>(
      request.ssl_info->early_data_received);
}

class SSLClientSocketZeroRTTTest : public SSLClientSocketTest {
 protected:
  SSLClientSocketZeroRTTTest() : SSLClientSocketTest() {
    SSLContextConfig config;
    config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;
    ssl_config_service_->UpdateSSLConfigAndNotify(config);
  }

  bool StartServer() {
    SSLServerConfig server_config;
    server_config.early_data_enabled = true;
    server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;
    return StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config);
  }

  void RegisterEmbeddedTestServerHandlers(EmbeddedTestServer* server) override {
    SSLClientSocketTest::RegisterEmbeddedTestServerHandlers(server);
    server->RegisterRequestHandler(base::BindRepeating(&HandleZeroRTTRequest));
  }

  void SetServerConfig(SSLServerConfig server_config) {
    embedded_test_server()->ResetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                           server_config);
  }

  // Makes a new connection to the test server and returns a
  // FakeBlockingStreamSocket which may be used to block transport I/O.
  //
  // Most tests should call BlockReadResult() before calling Connect(). This
  // avoid race conditions by controlling the order of events. 0-RTT typically
  // races the ServerHello from the server with early data from the client. If
  // the ServerHello arrives before client calls Write(), the data may be sent
  // with 1-RTT keys rather than 0-RTT keys.
  FakeBlockingStreamSocket* MakeClient(bool early_data_enabled) {
    SSLConfig ssl_config;
    ssl_config.early_data_enabled = early_data_enabled;

    real_transport_ = std::make_unique<TCPClientSocket>(
        addr(), nullptr, nullptr, nullptr, NetLogSource());
    auto transport =
        std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport_));
    FakeBlockingStreamSocket* raw_transport = transport.get();

    int rv = callback_.GetResult(transport->Connect(callback_.callback()));
    EXPECT_THAT(rv, IsOk());

    ssl_socket_ = CreateSSLClientSocket(std::move(transport), host_port_pair(),
                                        ssl_config);
    EXPECT_FALSE(ssl_socket_->IsConnected());

    return raw_transport;
  }

  int Connect() {
    return callback_.GetResult(ssl_socket_->Connect(callback_.callback()));
  }

  int WriteAndWait(std::string_view request) {
    auto request_buffer =
        base::MakeRefCounted<IOBufferWithSize>(request.size());
    memcpy(request_buffer->data(), request.data(), request.size());
    return callback_.GetResult(
        ssl_socket_->Write(request_buffer.get(), request.size(),
                           callback_.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  int ReadAndWait(IOBuffer* buf, size_t len) {
    return callback_.GetResult(
        ssl_socket_->Read(buf, len, callback_.callback()));
  }

  bool GetSSLInfo(SSLInfo* ssl_info) {
    return ssl_socket_->GetSSLInfo(ssl_info);
  }

  bool RunInitialConnection() {
    if (MakeClient(true) == nullptr)
      return false;

    EXPECT_THAT(Connect(), IsOk());

    // Use the socket for an HTTP request to ensure we've processed the
    // post-handshake TLS 1.3 ticket.
    EXPECT_THAT(MakeHTTPRequest(ssl_socket_.get()), IsOk());

    SSLInfo ssl_info;
    EXPECT_TRUE(GetSSLInfo(&ssl_info));

    // Make sure all asynchronous histogram logging is complete.
    base::RunLoop().RunUntilIdle();

    return SSLInfo::HANDSHAKE_FULL == ssl_info.handshake_type;
  }

  SSLClientSocket* ssl_socket() { return ssl_socket_.get(); }

 private:
  TestCompletionCallback callback_;
  std::unique_ptr<StreamSocket> real_transport_;
  std::unique_ptr<SSLClientSocket> ssl_socket_;
};

// Returns a serialized unencrypted TLS 1.2 alert record for the given alert
// value.
std::string FormatTLS12Alert(uint8_t alert) {
  std::string ret;
  // ContentType.alert
  ret.push_back(21);
  // Record-layer version. Assume TLS 1.2.
  ret.push_back(0x03);
  ret.push_back(0x03);
  // Record length.
  ret.push_back(0);
  ret.push_back(2);
  // AlertLevel.fatal.
  ret.push_back(2);
  // The alert itself.
  ret.push_back(alert);
  return ret;
}

// A CertVerifier that never returns on any requests.
class HangingCertVerifier : public CertVerifier {
 public:
  int num_active_requests() const { return num_active_requests_; }

  void WaitForRequest() {
    if (!num_active_requests_) {
      run_loop_.Run();
    }
  }

  int Verify(const RequestParams& params,
             CertVerifyResult* verify_result,
             CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const NetLogWithSource& net_log) override {
    *out_req = std::make_unique<HangingRequest>(this);
    return ERR_IO_PENDING;
  }

  void SetConfig(const Config& config) override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

 private:
  class HangingRequest : public Request {
   public:
    explicit HangingRequest(HangingCertVerifier* verifier)
        : verifier_(verifier) {
      verifier_->num_active_requests_++;
      verifier_->run_loop_.Quit();
    }

    ~HangingRequest() override { verifier_->num_active_requests_--; }

   private:
    raw_ptr<HangingCertVerifier> verifier_;
  };

  base::RunLoop run_loop_;
  int num_active_requests_ = 0;
};

class MockSSLClientContextObserver : public SSLClientContext::Observer {
 public:
  MOCK_METHOD1(OnSSLConfigChanged, void(SSLClientContext::SSLConfigChangeType));
  MOCK_METHOD1(OnSSLConfigForServersChanged,
               void(const base::flat_set<HostPortPair>&));
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(TLSVersion,
                         SSLClientSocketVersionTest,
                         ValuesIn(GetTLSVersions()));

TEST_P(SSLClientSocketVersionTest, Connect) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  auto entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLogEventType::SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());
  entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SSL_CONNECT));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_P(SSLClientSocketVersionTest, ConnectSyncVerify) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  cert_verifier_->set_async(false);
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(OK));
}

TEST_P(SSLClientSocketVersionTest, ConnectExpired) {
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_EXPIRED,
                                      GetServerConfig()));

  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_DATE_INVALID));

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  auto entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SSL_CONNECT));
}

TEST_P(SSLClientSocketVersionTest, ConnectExpiredSyncVerify) {
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_EXPIRED,
                                      GetServerConfig()));

  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);
  cert_verifier_->set_async(false);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_DATE_INVALID));
}

// Test that SSLClientSockets may be destroyed while waiting on a certificate
// verification.
TEST_P(SSLClientSocketVersionTest, SocketDestroyedDuringVerify) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  HangingCertVerifier verifier;
  context_ = std::make_unique<SSLClientContext>(
      ssl_config_service_.get(), &verifier, transport_security_state_.get(),
      ssl_client_session_cache_.get(), nullptr);

  TestCompletionCallback callback;
  auto transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig());
  rv = sock->Connect(callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // The socket should attempt a certificate verification.
  verifier.WaitForRequest();
  EXPECT_EQ(1, verifier.num_active_requests());

  // Destroying the socket should cancel it.
  sock = nullptr;
  EXPECT_EQ(0, verifier.num_active_requests());

  context_ = nullptr;
}

TEST_P(SSLClientSocketVersionTest, ConnectMismatched) {
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_MISMATCHED_NAME,
                                      GetServerConfig()));

  cert_verifier_->set_default_result(ERR_CERT_COMMON_NAME_INVALID);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_COMMON_NAME_INVALID));

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  auto entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SSL_CONNECT));
}

// Tests that certificates parsable by SSLClientSocket's internal SSL
// implementation, but not X509Certificate are treated as fatal connection
// errors. This is a regression test for https://crbug.com/91341.
TEST_P(SSLClientSocketVersionTest, ConnectBadValidity) {
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_BAD_VALIDITY,
                                      GetServerConfig()));
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_DATE_INVALID));
}

// Ignoring the certificate error from an invalid certificate should
// allow a complete connection.
TEST_P(SSLClientSocketVersionTest, ConnectBadValidityIgnoreCertErrors) {
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_BAD_VALIDITY,
                                      GetServerConfig()));
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  SSLConfig ssl_config;
  ssl_config.ignore_certificate_errors = true;
  int rv;
  CreateAndConnectSSLClientSocket(ssl_config, &rv);
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
}

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_P(SSLClientSocketVersionTest, ConnectClientAuthCertRequested) {
  SSLServerConfig server_config = GetServerConfig();
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

  auto entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SSL_CONNECT));
  EXPECT_FALSE(sock_->IsConnected());
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
TEST_P(SSLClientSocketVersionTest, ConnectClientAuthSendNullCert) {
  SSLServerConfig server_config = GetServerConfig();
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Our test server accepts certificate-less connections.
  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  // We responded to the server's certificate request with a Certificate
  // message with no client certificate in it.  ssl_info.client_cert_sent
  // should be false in this case.
  SSLInfo ssl_info;
  sock_->GetSSLInfo(&ssl_info);
  EXPECT_FALSE(ssl_info.client_cert_sent);

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

// Tests that the socket can be read from successfully. Also test that a peer's
// close_notify alert is successfully processed without error.
TEST_P(SSLClientSocketReadTest, Read) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto transport = std::make_unique<TCPClientSocket>(addr(), nullptr, nullptr,
                                                     nullptr, NetLogSource());
  EXPECT_EQ(0, transport->GetTotalReceivedBytes());

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));
  EXPECT_EQ(0, sock->GetTotalReceivedBytes());

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  // Number of network bytes received should increase because of SSL socket
  // establishment.
  EXPECT_GT(sock->GetTotalReceivedBytes(), 0);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(std::size(request_text) - 1);
  memcpy(request_buffer->data(), request_text, std::size(request_text) - 1);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), std::size(request_text) - 1,
                  callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(std::size(request_text) - 1), rv);

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int64_t unencrypted_bytes_read = 0;
  int64_t network_bytes_read_during_handshake = sock->GetTotalReceivedBytes();
  do {
    rv = ReadAndWaitForCompletion(sock.get(), buf.get(), 4096);
    EXPECT_GE(rv, 0);
    if (rv >= 0) {
      unencrypted_bytes_read += rv;
    }
  } while (rv > 0);
  EXPECT_GT(unencrypted_bytes_read, 0);
  // Reading the payload should increase the number of bytes on network layer.
  EXPECT_GT(sock->GetTotalReceivedBytes(), network_bytes_read_during_handshake);
  // Number of bytes received on the network after the handshake should be
  // higher than the number of encrypted bytes read.
  EXPECT_GE(sock->GetTotalReceivedBytes() - network_bytes_read_during_handshake,
            unencrypted_bytes_read);

  // The peer should have cleanly closed the connection with a close_notify.
  EXPECT_EQ(0, rv);
}

// Tests that SSLClientSocket properly handles when the underlying transport
// synchronously fails a transport write in during the handshake.
TEST_F(SSLClientSocketTest, Connect_WithSynchronousError) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  raw_transport->SetNextWriteError(ERR_CONNECTION_RESET);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));
  EXPECT_FALSE(sock->IsConnected());
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// synchronously returns an error code - such as if an intermediary terminates
// the socket connection uncleanly.
// This is a regression test for http://crbug.com/238536
TEST_P(SSLClientSocketReadTest, Read_WithSynchronousError) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  SynchronousErrorStreamSocket* raw_transport = transport.get();
  std::unique_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(std::size(request_text) - 1);
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kRequestTextSize);
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(sock->Write(request_buffer.get(), kRequestTextSize,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(kRequestTextSize, rv);

  // Simulate an unclean/forcible shutdown.
  raw_transport->SetNextReadError(ERR_CONNECTION_RESET);

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);

  // Note: This test will hang if this bug has regressed. Simply checking that
  // rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING is a legitimate
  // result when using a dedicated task runner for NSS.
  rv = ReadAndWaitForCompletion(sock.get(), buf.get(), 4096);
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// asynchronously returns an error code while writing data - such as if an
// intermediary terminates the socket connection uncleanly.
// This is a regression test for http://crbug.com/249848
TEST_P(SSLClientSocketVersionTest, Write_WithSynchronousError) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  auto error_socket =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(error_socket));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(std::size(request_text) - 1);
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kRequestTextSize);
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  // Simulate an unclean/forcible shutdown on the underlying socket.
  // However, simulate this error asynchronously.
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  raw_transport->BlockWrite();

  // This write should complete synchronously, because the TLS ciphertext
  // can be created and placed into the outgoing buffers independent of the
  // underlying transport.
  rv = callback.GetResult(sock->Write(request_buffer.get(), kRequestTextSize,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(kRequestTextSize, rv);

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);

  rv = sock->Read(buf.get(), 4096, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Now unblock the outgoing request, having it fail with the connection
  // being reset.
  raw_transport->UnblockWrite();

  // Note: This will cause an inifite loop if this bug has regressed. Simply
  // checking that rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING
  // is a legitimate result when using a dedicated task runner for NSS.
  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));
}

// If there is a Write failure at the transport with no follow-up Read, although
// the write error will not be returned to the client until a future Read or
// Write operation, SSLClientSocket should not spin attempting to re-write on
// the socket. This is a regression test for part of https://crbug.com/381160.
TEST_P(SSLClientSocketVersionTest, Write_WithSynchronousErrorNoRead) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  // Note: intermediate sockets' ownership are handed to |sock|, but a pointer
  // is retained in order to query them.
  auto error_socket =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  auto counting_socket =
      std::make_unique<CountingStreamSocket>(std::move(error_socket));
  CountingStreamSocket* raw_counting_socket = counting_socket.get();
  int rv = callback.GetResult(counting_socket->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(counting_socket), host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock->IsConnected());

  // Simulate an unclean/forcible shutdown on the underlying socket.
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(std::size(request_text) - 1);
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kRequestTextSize);
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  // This write should complete synchronously, because the TLS ciphertext
  // can be created and placed into the outgoing buffers independent of the
  // underlying transport.
  rv = callback.GetResult(sock->Write(request_buffer.get(), kRequestTextSize,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_EQ(kRequestTextSize, rv);

  // Let the event loop spin for a little bit of time. Even on platforms where
  // pumping the state machine involve thread hops, there should be no further
  // writes on the transport socket.
  //
  // TODO(davidben): Avoid the arbitrary timeout?
  int old_write_count = raw_counting_socket->write_count();
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(100));
  loop.Run();
  EXPECT_EQ(old_write_count, raw_counting_socket->write_count());
}

// Test the full duplex mode, with Read and Write pending at the same time.
// This test also serves as a regression test for http://crbug.com/29815.
TEST_P(SSLClientSocketReadTest, Read_FullDuplex) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  // Issue a "hanging" Read first.
  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int read_rv = Read(sock_.get(), buf.get(), 4096, callback.callback());
  // We haven't written the request, so there should be no response yet.
  ASSERT_THAT(read_rv, IsError(ERR_IO_PENDING));

  // Write the request.
  // The request is padded with a User-Agent header to a size that causes the
  // memio circular buffer (4k bytes) in SSLClientSocketNSS to wrap around.
  // This tests the fix for http://crbug.com/29815.
  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  for (int i = 0; i < 3770; ++i)
    request_text.push_back('*');
  request_text.append("\r\n\r\n");
  auto request_buffer = base::MakeRefCounted<StringIOBuffer>(request_text);

  TestCompletionCallback callback2;  // Used for Write only.
  rv = callback2.GetResult(
      sock_->Write(request_buffer.get(), request_text.size(),
                   callback2.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  // Now get the Read result.
  read_rv =
      WaitForReadCompletion(sock_.get(), buf.get(), 4096, &callback, read_rv);
  EXPECT_GT(read_rv, 0);
}

// Attempts to Read() and Write() from an SSLClientSocketNSS in full duplex
// mode when the underlying transport is blocked on sending data. When the
// underlying transport completes due to an error, it should invoke both the
// Read() and Write() callbacks. If the socket is deleted by the Read()
// callback, the Write() callback should not be invoked.
// Regression test for http://crbug.com/232633
TEST_P(SSLClientSocketReadTest, Read_DeleteWhilePendingFullDuplex) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  auto error_socket =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(error_socket));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  std::unique_ptr<SSLClientSocket> sock =
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  request_text.append(20 * 1024, '*');
  request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> request_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<StringIOBuffer>(request_text),
          request_text.size());

  // Simulate errors being returned from the underlying Read() and Write() ...
  raw_error_socket->SetNextReadError(ERR_CONNECTION_RESET);
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  // ... but have those errors returned asynchronously. Because the Write() will
  // return first, this will trigger the error.
  raw_transport->BlockReadResult();
  raw_transport->BlockWrite();

  // Enqueue a Read() before calling Write(), which should "hang" due to
  // the ERR_IO_PENDING caused by SetReadShouldBlock() and thus return.
  SSLClientSocket* raw_sock = sock.get();
  DeleteSocketCallback read_callback(sock.release());
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  rv = Read(raw_sock, read_buf.get(), 4096, read_callback.callback());

  // Ensure things didn't complete synchronously, otherwise |sock| is invalid.
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_FALSE(read_callback.have_result());

  // Attempt to write the remaining data. OpenSSL will return that its blocked
  // because the underlying transport is blocked.
  rv = raw_sock->Write(request_buffer.get(), request_buffer->BytesRemaining(),
                       callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_FALSE(callback.have_result());

  // Now unblock Write(), which will invoke OnSendComplete and (eventually)
  // call the Read() callback, deleting the socket and thus aborting calling
  // the Write() callback.
  raw_transport->UnblockWrite();

  // |read_callback| deletes |sock| so if ReadIfReady() is used, we will get OK
  // asynchronously but can't continue reading because the socket is gone.
  rv = read_callback.WaitForResult();
  if (test_ssl_read_if_ready()) {
    EXPECT_THAT(rv, IsOk());
  } else {
    EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));
  }

  // The Write callback should not have been called.
  EXPECT_FALSE(callback.have_result());
}

// Tests that the SSLClientSocket does not crash if data is received on the
// transport socket after a failing write. This can occur if we have a Write
// error in a SPDY socket.
// Regression test for http://crbug.com/335557
TEST_P(SSLClientSocketReadTest, Read_WithWriteError) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  auto error_socket =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(error_socket));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  // Send a request so there is something to read from the socket.
  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(std::size(request_text) - 1);
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(kRequestTextSize);
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(sock->Write(request_buffer.get(), kRequestTextSize,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(kRequestTextSize, rv);

  // Start a hanging read.
  TestCompletionCallback read_callback;
  raw_transport->BlockReadResult();
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  rv = Read(sock.get(), buf.get(), 4096, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Perform another write, but have it fail. Write a request larger than the
  // internal socket buffers so that the request hits the underlying transport
  // socket and detects the error.
  std::string long_request_text =
      "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  long_request_text.append(20 * 1024, '*');
  long_request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> long_request_buffer =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<StringIOBuffer>(long_request_text),
          long_request_text.size());

  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);

  // Write as much data as possible until hitting an error.
  do {
    rv = callback.GetResult(sock->Write(
        long_request_buffer.get(), long_request_buffer->BytesRemaining(),
        callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
    if (rv > 0) {
      long_request_buffer->DidConsume(rv);
      // Abort if the entire input is ever consumed. The input is larger than
      // the SSLClientSocket's write buffers.
      ASSERT_LT(0, long_request_buffer->BytesRemaining());
    }
  } while (rv > 0);

  EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));

  // At this point the Read result is available. Transport write errors are
  // surfaced through Writes. See https://crbug.com/249848.
  rv = WaitForReadCompletion(sock.get(), buf.get(), 4096, &read_callback, rv);
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_RESET));

  // Release the read. This does not cause a crash.
  raw_transport->UnblockReadResult();
  base::RunLoop().RunUntilIdle();
}

// Tests that SSLClientSocket fails the handshake if the underlying
// transport is cleanly closed.
TEST_F(SSLClientSocketTest, Connect_WithZeroReturn) {
  // There is no need to vary by TLS version because this test never reads a
  // response from the server.
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  raw_transport->SetNextReadError(0);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
  EXPECT_FALSE(sock->IsConnected());
}

// Tests that SSLClientSocket returns a Read of size 0 if the underlying socket
// is cleanly closed, but the peer does not send close_notify.
// This is a regression test for https://crbug.com/422246
TEST_P(SSLClientSocketReadTest, Read_WithZeroReturn) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  SynchronousErrorStreamSocket* raw_transport = transport.get();
  std::unique_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  raw_transport->SetNextReadError(0);
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  rv = ReadAndWaitForCompletion(sock.get(), buf.get(), 4096);
  EXPECT_EQ(0, rv);
}

// Tests that SSLClientSocket cleanly returns a Read of size 0 if the
// underlying socket is cleanly closed asynchronously.
// This is a regression test for https://crbug.com/422246
TEST_P(SSLClientSocketReadTest, Read_WithAsyncZeroReturn) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto error_socket =
      std::make_unique<SynchronousErrorStreamSocket>(std::move(real_transport));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(error_socket));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  std::unique_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  raw_error_socket->SetNextReadError(0);
  raw_transport->BlockReadResult();
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  TestCompletionCallback read_callback;
  rv = Read(sock.get(), buf.get(), 4096, read_callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  raw_transport->UnblockReadResult();
  rv = WaitForReadCompletion(sock.get(), buf.get(), 4096, &read_callback, rv);
  EXPECT_EQ(0, rv);
}

// Tests that fatal alerts from the peer are processed. This is a regression
// test for https://crbug.com/466303.
TEST_P(SSLClientSocketReadTest, Read_WithFatalAlert) {
  SSLServerConfig server_config = GetServerConfig();
  server_config.alert_after_handshake_for_testing = SSL_AD_INTERNAL_ERROR;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  // Receive the fatal alert.
  TestCompletionCallback callback;
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR,
            ReadAndWaitForCompletion(sock_.get(), buf.get(), 4096));
}

TEST_P(SSLClientSocketReadTest, Read_SmallChunks) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(std::size(request_text) - 1);
  memcpy(request_buffer->data(), request_text, std::size(request_text) - 1);

  TestCompletionCallback callback;
  rv = callback.GetResult(
      sock_->Write(request_buffer.get(), std::size(request_text) - 1,
                   callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(std::size(request_text) - 1), rv);

  auto buf = base::MakeRefCounted<IOBufferWithSize>(1);
  do {
    rv = ReadAndWaitForCompletion(sock_.get(), buf.get(), 1);
    EXPECT_GE(rv, 0);
  } while (rv > 0);
}

TEST_P(SSLClientSocketReadTest, Read_ManySmallRecords) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;

  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<ReadBufferingStreamSocket>(std::move(real_transport));
  ReadBufferingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  std::unique_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock->IsConnected());

  const char request_text[] = "GET /ssl-many-small-records HTTP/1.0\r\n\r\n";
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(std::size(request_text) - 1);
  memcpy(request_buffer->data(), request_text, std::size(request_text) - 1);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), std::size(request_text) - 1,
                  callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_GT(rv, 0);
  ASSERT_EQ(static_cast<int>(std::size(request_text) - 1), rv);

  // Note: This relies on SSLClientSocketNSS attempting to read up to 17K of
  // data (the max SSL record size) at a time. Ensure that at least 15K worth
  // of SSL data is buffered first. The 15K of buffered data is made up of
  // many smaller SSL records (the TestServer writes along 1350 byte
  // plaintext boundaries), although there may also be a few records that are
  // smaller or larger, due to timing and SSL False Start.
  // 15K was chosen because 15K is smaller than the 17K (max) read issued by
  // the SSLClientSocket implementation, and larger than the minimum amount
  // of ciphertext necessary to contain the 8K of plaintext requested below.
  raw_transport->BufferNextRead(15000);

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(8192);
  rv = ReadAndWaitForCompletion(sock.get(), buffer.get(), 8192);
  ASSERT_EQ(rv, 8192);
}

TEST_P(SSLClientSocketReadTest, Read_Interrupted) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(std::size(request_text) - 1);
  memcpy(request_buffer->data(), request_text, std::size(request_text) - 1);

  TestCompletionCallback callback;
  rv = callback.GetResult(
      sock_->Write(request_buffer.get(), std::size(request_text) - 1,
                   callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(std::size(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  auto buf = base::MakeRefCounted<IOBufferWithSize>(512);
  rv = ReadAndWaitForCompletion(sock_.get(), buf.get(), 512);
  EXPECT_GT(rv, 0);
}

TEST_P(SSLClientSocketReadTest, Read_FullLogging) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  log_observer_.SetObserverCaptureMode(NetLogCaptureMode::kEverything);
  auto transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  auto request_buffer =
      base::MakeRefCounted<IOBufferWithSize>(std::size(request_text) - 1);
  memcpy(request_buffer->data(), request_text, std::size(request_text) - 1);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), std::size(request_text) - 1,
                  callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(std::size(request_text) - 1), rv);

  auto entries = log_observer_.GetEntries();
  size_t last_index = ExpectLogContainsSomewhereAfter(
      entries, 5, NetLogEventType::SSL_SOCKET_BYTES_SENT,
      NetLogEventPhase::NONE);

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  for (;;) {
    rv = ReadAndWaitForCompletion(sock.get(), buf.get(), 4096);
    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;

    entries = log_observer_.GetEntries();
    last_index = ExpectLogContainsSomewhereAfter(
        entries, last_index + 1, NetLogEventType::SSL_SOCKET_BYTES_RECEIVED,
        NetLogEventPhase::NONE);
  }
}

// Regression test for http://crbug.com/42538
TEST_F(SSLClientSocketTest, PrematureApplicationData) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  static const unsigned char application_data[] = {
      0x17, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00, 0x46, 0x03, 0x01, 0x4b,
      0xc2, 0xf8, 0xb2, 0xc1, 0x56, 0x42, 0xb9, 0x57, 0x7f, 0xde, 0x87, 0x46,
      0xf7, 0xa3, 0x52, 0x42, 0x21, 0xf0, 0x13, 0x1c, 0x9c, 0x83, 0x88, 0xd6,
      0x93, 0x0c, 0xf6, 0x36, 0x30, 0x05, 0x7e, 0x20, 0xb5, 0xb5, 0x73, 0x36,
      0x53, 0x83, 0x0a, 0xfc, 0x17, 0x63, 0xbf, 0xa0, 0xe4, 0x42, 0x90, 0x0d,
      0x2f, 0x18, 0x6d, 0x20, 0xd8, 0x36, 0x3f, 0xfc, 0xe6, 0x01, 0xfa, 0x0f,
      0xa5, 0x75, 0x7f, 0x09, 0x00, 0x04, 0x00, 0x16, 0x03, 0x01, 0x11, 0x57,
      0x0b, 0x00, 0x11, 0x53, 0x00, 0x11, 0x50, 0x00, 0x06, 0x22, 0x30, 0x82,
      0x06, 0x1e, 0x30, 0x82, 0x05, 0x06, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
      0x0a};

  // All reads and writes complete synchronously (async=false).
  MockRead data_reads[] = {
      MockRead(SYNCHRONOUS, reinterpret_cast<const char*>(application_data),
               std::size(application_data)),
      MockRead(SYNCHRONOUS, OK),
  };

  StaticSocketDataProvider data(data_reads, base::span<MockWrite>());

  TestCompletionCallback callback;
  std::unique_ptr<StreamSocket> transport(
      std::make_unique<MockTCPClientSocket>(addr(), nullptr, &data));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

TEST_F(SSLClientSocketTest, CipherSuiteDisables) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLContextConfig ssl_context_config;
  ssl_context_config.disabled_cipher_suites.push_back(kModernTLS12Cipher);
  ssl_config_service_->UpdateSSLConfigAndNotify(ssl_context_config);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

// Test that TLS versions prior to TLS 1.2 cannot be configured in
// SSLClientSocket.
TEST_F(SSLClientSocketTest, LegacyTLSVersions) {
  // Start a server, just so the underlying socket can connect somewhere, but it
  // will fail before talking to the server, so it is fine that the server does
  // not speak these versions.
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  // Although we don't have `SSL_PROTOCOL_VERSION_*` constants for SSL 3.0
  // through TLS 1.1, these values are just passed through to the BoringSSL API,
  // which means the underlying protocol version numbers can be used here.
  //
  // TODO(crbug.com/40893435): Ideally SSLConfig would just take an enum,
  // at which point this test can be removed.
  for (uint16_t version : {SSL3_VERSION, TLS1_VERSION, TLS1_1_VERSION}) {
    SCOPED_TRACE(version);

    SSLConfig config;
    config.version_min_override = version;
    int rv;
    ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
    EXPECT_THAT(rv, IsError(ERR_UNEXPECTED));

    config.version_min_override = std::nullopt;
    config.version_max_override = version;
    ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
    EXPECT_THAT(rv, IsError(ERR_UNEXPECTED));
  }
}

// When creating an SSLClientSocket, it is allowed to pass in a
// ClientSocketHandle that is not obtained from a client socket pool.
// Here we verify that such a simple ClientSocketHandle, not associated with any
// client socket pool, can be destroyed safely.
TEST_F(SSLClientSocketTest, ClientSocketHandleNotFromPool) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  TestCompletionCallback callback;
  auto transport = std::make_unique<TCPClientSocket>(addr(), nullptr, nullptr,
                                                     nullptr, NetLogSource());
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(socket_factory_->CreateSSLClientSocket(
      context_.get(), std::move(transport), host_port_pair(), SSLConfig()));

  EXPECT_FALSE(sock->IsConnected());
  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_THAT(rv, IsOk());
}

// Verifies that SSLClientSocket::ExportKeyingMaterial return a success
// code and different keying label results in different keying material.
TEST_P(SSLClientSocketVersionTest, ExportKeyingMaterial) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  const int kKeyingMaterialSize = 32;
  const char kKeyingLabel1[] = "client-socket-test-1";
  const char kKeyingContext1[] = "";
  unsigned char client_out1[kKeyingMaterialSize];
  memset(client_out1, 0, sizeof(client_out1));
  rv = sock_->ExportKeyingMaterial(kKeyingLabel1, false, kKeyingContext1,
                                   client_out1, sizeof(client_out1));
  EXPECT_EQ(rv, OK);

  const char kKeyingLabel2[] = "client-socket-test-2";
  unsigned char client_out2[kKeyingMaterialSize];
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock_->ExportKeyingMaterial(kKeyingLabel2, false, kKeyingContext1,
                                   client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);

  const char kKeyingContext2[] = "context";
  rv = sock_->ExportKeyingMaterial(kKeyingLabel1, true, kKeyingContext2,
                                   client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);

  // Prior to TLS 1.3, using an empty context should give different key material
  // from not using a context at all. In TLS 1.3, the distinction is deprecated
  // and they are the same.
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock_->ExportKeyingMaterial(kKeyingLabel1, true, kKeyingContext1,
                                   client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  if (version() >= SSL_PROTOCOL_VERSION_TLS1_3) {
    EXPECT_EQ(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);
  } else {
    EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);
  }
}

TEST(SSLClientSocket, SerializeNextProtos) {
  NextProtoVector next_protos;
  next_protos.push_back(kProtoHTTP11);
  next_protos.push_back(kProtoHTTP2);
  static std::vector<uint8_t> serialized =
      SSLClientSocket::SerializeNextProtos(next_protos);
  ASSERT_EQ(12u, serialized.size());
  EXPECT_EQ(8, serialized[0]);  // length("http/1.1")
  EXPECT_EQ('h', serialized[1]);
  EXPECT_EQ('t', serialized[2]);
  EXPECT_EQ('t', serialized[3]);
  EXPECT_EQ('p', serialized[4]);
  EXPECT_EQ('/', serialized[5]);
  EXPECT_EQ('1', serialized[6]);
  EXPECT_EQ('.', serialized[7]);
  EXPECT_EQ('1', serialized[8]);
  EXPECT_EQ(2, serialized[9]);  // length("h2")
  EXPECT_EQ('h', serialized[10]);
  EXPECT_EQ('2', serialized[11]);
}

// Test that the server certificates are properly retrieved from the underlying
// SSL stack.
TEST_P(SSLClientSocketVersionTest, VerifyServerChainProperlyOrdered) {
  // The connection does not have to be successful.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  // This makes the server present redundant-server-chain.pem, which contains
  // intermediate certificates.
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_CHAIN_WRONG_ROOT,
                                      GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_INVALID));
  EXPECT_FALSE(sock_->IsConnected());

  // When given option CERT_CHAIN_WRONG_ROOT, EmbeddedTestServer will present
  // certs from redundant-server-chain.pem.
  CertificateList server_certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-server-chain.pem",
                                    X509Certificate::FORMAT_AUTO);

  // Get the server certificate as received client side.
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  scoped_refptr<X509Certificate> server_certificate = ssl_info.unverified_cert;

  // Get the intermediates as received  client side.
  const auto& server_intermediates = server_certificate->intermediate_buffers();

  // Check that the unverified server certificate chain is properly retrieved
  // from the underlying ssl stack.
  ASSERT_EQ(4U, server_certs.size());

  EXPECT_TRUE(x509_util::CryptoBufferEqual(server_certificate->cert_buffer(),
                                           server_certs[0]->cert_buffer()));

  ASSERT_EQ(3U, server_intermediates.size());

  EXPECT_TRUE(x509_util::CryptoBufferEqual(server_intermediates[0].get(),
                                           server_certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(server_intermediates[1].get(),
                                           server_certs[2]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(server_intermediates[2].get(),
                                           server_certs[3]->cert_buffer()));

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

// This tests that SSLInfo contains a properly re-constructed certificate
// chain. That, in turn, verifies that GetSSLInfo is giving us the chain as
// verified, not the chain as served by the server. (They may be different.)
//
// CERT_CHAIN_WRONG_ROOT is redundant-server-chain.pem. It contains A
// (end-entity) -> B -> C, and C is signed by D. redundant-validated-chain.pem
// contains a chain of A -> B -> C2, where C2 is the same public key as C, but
// a self-signed root. Such a situation can occur when a new root (C2) is
// cross-certified by an old root (D) and has two different versions of its
// floating around. Servers may supply C2 as an intermediate, but the
// SSLClientSocket should return the chain that was verified, from
// verify_result, instead.
TEST_P(SSLClientSocketVersionTest, VerifyReturnChainProperlyOrdered) {
  // By default, cause the CertVerifier to treat all certificates as
  // expired.
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  CertificateList unverified_certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "redundant-server-chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(4u, unverified_certs.size());

  // We will expect SSLInfo to ultimately contain this chain.
  CertificateList certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-validated-chain.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  ASSERT_TRUE(certs[0]->EqualsExcludingChain(unverified_certs[0].get()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> temp_intermediates;
  temp_intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  temp_intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  CertVerifyResult verify_result;
  verify_result.verified_cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(temp_intermediates));
  ASSERT_TRUE(verify_result.verified_cert);

  // Add a rule that maps the server cert (A) to the chain of A->B->C2
  // rather than A->B->C.
  cert_verifier_->AddResultForCert(certs[0].get(), verify_result, OK);

  // Load and install the root for the validated chain.
  scoped_refptr<X509Certificate> root_cert = ImportCertFromFile(
      GetTestCertsDirectory(), "redundant-validated-chain-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), root_cert.get());
  ScopedTestRoot scoped_root(root_cert);

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_CHAIN_WRONG_ROOT,
                                      GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  auto entries = log_observer_.GetEntries();
  EXPECT_TRUE(LogContainsEndEvent(entries, -1, NetLogEventType::SSL_CONNECT));

  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  // Verify that SSLInfo contains the corrected re-constructed chain A -> B
  // -> C2.
  ASSERT_TRUE(ssl_info.cert);
  const auto& intermediates = ssl_info.cert->intermediate_buffers();
  ASSERT_EQ(2U, intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(ssl_info.cert->cert_buffer(),
                                           certs[0]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(intermediates[1].get(),
                                           certs[2]->cert_buffer()));

  // Verify that SSLInfo also contains the chain as received from the server.
  ASSERT_TRUE(ssl_info.unverified_cert);
  const auto& served_intermediates =
      ssl_info.unverified_cert->intermediate_buffers();
  ASSERT_EQ(3U, served_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(ssl_info.cert->cert_buffer(),
                                           unverified_certs[0]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(served_intermediates[0].get(),
                                           unverified_certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(served_intermediates[1].get(),
                                           unverified_certs[2]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(served_intermediates[2].get(),
                                           unverified_certs[3]->cert_buffer()));

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());
}

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
INSTANTIATE_TEST_SUITE_P(TLSVersion,
                         SSLClientSocketCertRequestInfoTest,
                         ValuesIn(GetTLSVersions()));

TEST_P(SSLClientSocketCertRequestInfoTest,
       DontRequestClientCertsIfServerCertInvalid) {
  SSLServerConfig config = GetServerConfig();
  config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_EXPIRED, config));

  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_CERT_DATE_INVALID));
}

TEST_P(SSLClientSocketCertRequestInfoTest, NoAuthorities) {
  SSLServerConfig config = GetServerConfig();
  config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, config));
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest();
  ASSERT_TRUE(request_info.get());
  EXPECT_EQ(0u, request_info->cert_authorities.size());
}

TEST_P(SSLClientSocketCertRequestInfoTest, TwoAuthorities) {
  const unsigned char kThawteDN[] = {
      0x30, 0x4c, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x5a, 0x41, 0x31, 0x25, 0x30, 0x23, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x1c, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20, 0x43, 0x6f, 0x6e,
      0x73, 0x75, 0x6c, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x28, 0x50, 0x74, 0x79,
      0x29, 0x20, 0x4c, 0x74, 0x64, 0x2e, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03,
      0x55, 0x04, 0x03, 0x13, 0x0d, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20,
      0x53, 0x47, 0x43, 0x20, 0x43, 0x41};

  const unsigned char kDiginotarDN[] = {
      0x30, 0x5f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x4e, 0x4c, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x09, 0x44, 0x69, 0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x31,
      0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x11, 0x44, 0x69,
      0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x20, 0x52, 0x6f, 0x6f, 0x74,
      0x20, 0x43, 0x41, 0x31, 0x20, 0x30, 0x1e, 0x06, 0x09, 0x2a, 0x86, 0x48,
      0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x11, 0x69, 0x6e, 0x66, 0x6f,
      0x40, 0x64, 0x69, 0x67, 0x69, 0x6e, 0x6f, 0x74, 0x61, 0x72, 0x2e, 0x6e,
      0x6c};

  SSLServerConfig config = GetServerConfig();
  config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  config.cert_authorities.emplace_back(std::begin(kThawteDN),
                                       std::end(kThawteDN));
  config.cert_authorities.emplace_back(std::begin(kDiginotarDN),
                                       std::end(kDiginotarDN));
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, config));
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest();
  ASSERT_TRUE(request_info.get());
  EXPECT_EQ(config.cert_authorities, request_info->cert_authorities);
}

TEST_P(SSLClientSocketCertRequestInfoTest, CertKeyTypes) {
  SSLServerConfig config = GetServerConfig();
  config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, config));
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest();
  ASSERT_TRUE(request_info);
  // Look for some values we expect BoringSSL to always send.
  EXPECT_THAT(request_info->signature_algorithms,
              testing::Contains(SSL_SIGN_ECDSA_SECP256R1_SHA256));
  EXPECT_THAT(request_info->signature_algorithms,
              testing::Contains(SSL_SIGN_RSA_PSS_RSAE_SHA256));
}
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

// Tests that the Certificate Transparency (RFC 6962) TLS extension is
// supported.
TEST_P(SSLClientSocketVersionTest, ConnectSignedCertTimestampsTLSExtension) {
  // Encoding of SCT List containing 'test'.
  std::string_view sct_ext("\x00\x06\x00\x04test", 8);

  SSLServerConfig server_config = GetServerConfig();
  server_config.signed_cert_timestamp_list =
      std::vector<uint8_t>(sct_ext.begin(), sct_ext.end());
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());

  EXPECT_TRUE(sock_->signed_cert_timestamps_received_);

  ASSERT_EQ(cert_verifier_->GetVerifyParams().size(), 1u);
  const auto& params = cert_verifier_->GetVerifyParams().front();
  EXPECT_TRUE(params.certificate()->EqualsIncludingChain(
      embedded_test_server()->GetCertificate().get()));
  EXPECT_EQ(params.hostname(), embedded_test_server()->host_port_pair().host());
  EXPECT_EQ(params.ocsp_response(), "");
  EXPECT_EQ(params.sct_list(), sct_ext);

  sock_ = nullptr;
  context_ = nullptr;
}

// Tests that OCSP stapling is requested, as per Certificate Transparency (RFC
// 6962).
TEST_P(SSLClientSocketVersionTest, ConnectSignedCertTimestampsEnablesOCSP) {
  // The test server currently only knows how to generate OCSP responses
  // for a freshly minted certificate.
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.stapled_ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  ASSERT_TRUE(StartEmbeddedTestServer(cert_config, GetServerConfig()));

  SSLConfig ssl_config;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  EXPECT_THAT(rv, IsOk());

  EXPECT_TRUE(sock_->stapled_ocsp_response_received_);
}

// Tests that IsConnectedAndIdle and WasEverUsed behave as expected.
TEST_P(SSLClientSocketVersionTest, ReuseStates) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));

  // The socket was just connected. It should be idle because it is speaking
  // HTTP. Although the transport has been used for the handshake, WasEverUsed()
  // returns false.
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_TRUE(sock_->IsConnectedAndIdle());
  EXPECT_FALSE(sock_->WasEverUsed());

  const char kRequestText[] = "GET / HTTP/1.0\r\n\r\n";
  const size_t kRequestLen = std::size(kRequestText) - 1;
  auto request_buffer = base::MakeRefCounted<IOBufferWithSize>(kRequestLen);
  memcpy(request_buffer->data(), kRequestText, kRequestLen);

  TestCompletionCallback callback;
  rv = callback.GetResult(sock_->Write(request_buffer.get(), kRequestLen,
                                       callback.callback(),
                                       TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(static_cast<int>(kRequestLen), rv);

  // The socket has now been used.
  EXPECT_TRUE(sock_->WasEverUsed());

  // TODO(davidben): Read one byte to ensure the test server has responded and
  // then assert IsConnectedAndIdle is false. This currently doesn't work
  // because SSLClientSocketImpl doesn't check the implementation's internal
  // buffer. Call SSL_pending.
}

// Tests that |is_fatal_cert_error| does not get set for a certificate error,
// on a non-HSTS host.
TEST_P(SSLClientSocketVersionTest, IsFatalErrorNotSetOnNonFatalError) {
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_CHAIN_WRONG_ROOT,
                                      GetServerConfig()));
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.is_fatal_cert_error);
}

// Tests that |is_fatal_cert_error| gets set for a certificate error on an
// HSTS host.
TEST_P(SSLClientSocketVersionTest, IsFatalErrorSetOnFatalError) {
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);
  ASSERT_TRUE(StartEmbeddedTestServer(EmbeddedTestServer::CERT_CHAIN_WRONG_ROOT,
                                      GetServerConfig()));
  int rv;
  const base::Time expiry = base::Time::Now() + base::Seconds(1000);
  transport_security_state_->AddHSTS(host_port_pair().host(), expiry, true);
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_fatal_cert_error);
}

// Tests that IsConnectedAndIdle treats a socket as idle even if a Write hasn't
// been flushed completely out of SSLClientSocket's internal buffers. This is a
// regression test for https://crbug.com/466147.
TEST_P(SSLClientSocketVersionTest, ReusableAfterWrite) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  ASSERT_THAT(callback.GetResult(transport->Connect(callback.callback())),
              IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));
  ASSERT_THAT(callback.GetResult(sock->Connect(callback.callback())), IsOk());

  // Block any application data from reaching the network.
  raw_transport->BlockWrite();

  // Write a partial HTTP request.
  const char kRequestText[] = "GET / HTTP/1.0";
  const size_t kRequestLen = std::size(kRequestText) - 1;
  auto request_buffer = base::MakeRefCounted<IOBufferWithSize>(kRequestLen);
  memcpy(request_buffer->data(), kRequestText, kRequestLen);

  // Although transport writes are blocked, SSLClientSocketImpl completes the
  // outer Write operation.
  EXPECT_EQ(static_cast<int>(kRequestLen),
            callback.GetResult(sock->Write(request_buffer.get(), kRequestLen,
                                           callback.callback(),
                                           TRAFFIC_ANNOTATION_FOR_TESTS)));

  // The Write operation is complete, so the socket should be treated as
  // reusable, in case the server returns an HTTP response before completely
  // consuming the request body. In this case, we assume the server will
  // properly drain the request body before trying to read the next request.
  EXPECT_TRUE(sock->IsConnectedAndIdle());
}

// Tests that basic session resumption works.
TEST_P(SSLClientSocketVersionTest, SessionResumption) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  // First, perform a full handshake.
  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // TLS 1.2 with False Start and TLS 1.3 cause the ticket to arrive later, so
  // use the socket to ensure the session ticket has been picked up.
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());

  // The next connection should resume.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  sock_.reset();

  // Using a different HostPortPair uses a different session cache key.
  auto transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());
  TestCompletionCallback callback;
  ASSERT_THAT(callback.GetResult(transport->Connect(callback.callback())),
              IsOk());
  std::unique_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
      std::move(transport), HostPortPair("example.com", 443), ssl_config);
  ASSERT_THAT(callback.GetResult(sock->Connect(callback.callback())), IsOk());
  ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  sock.reset();

  ssl_client_session_cache_->Flush();

  // After clearing the session cache, the next handshake doesn't resume.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // Pick up the ticket again and confirm resumption works.
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  sock_.reset();

  // Updating the context-wide configuration should flush the session cache.
  SSLContextConfig config;
  config.disabled_cipher_suites = {1234};
  ssl_config_service_->UpdateSSLConfigAndNotify(config);
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
}

namespace {

// FakePeerAddressSocket wraps a |StreamSocket|, forwarding all calls except
// that it provides a given answer for |GetPeerAddress|.
class FakePeerAddressSocket : public WrappedStreamSocket {
 public:
  FakePeerAddressSocket(std::unique_ptr<StreamSocket> socket,
                        const IPEndPoint& address)
      : WrappedStreamSocket(std::move(socket)), address_(address) {}
  ~FakePeerAddressSocket() override = default;

  int GetPeerAddress(IPEndPoint* address) const override {
    *address = address_;
    return OK;
  }

 private:
  const IPEndPoint address_;
};

}  // namespace

TEST_F(SSLClientSocketTest, SessionResumption_RSA) {
  for (bool use_rsa : {false, true}) {
    SCOPED_TRACE(use_rsa);

    SSLServerConfig server_config;
    server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
    server_config.cipher_suite_for_testing =
        use_rsa ? kRSACipher : kModernTLS12Cipher;
    ASSERT_TRUE(
        StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));
    SSLConfig ssl_config;
    ssl_client_session_cache_->Flush();

    for (int i = 0; i < 3; i++) {
      SCOPED_TRACE(i);

      auto transport = std::make_unique<TCPClientSocket>(
          addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());
      TestCompletionCallback callback;
      ASSERT_THAT(callback.GetResult(transport->Connect(callback.callback())),
                  IsOk());
      // The third handshake sees a different destination IP address.
      IPEndPoint fake_peer_address(IPAddress(1, 1, 1, i == 2 ? 2 : 1), 443);
      auto socket = std::make_unique<FakePeerAddressSocket>(
          std::move(transport), fake_peer_address);
      std::unique_ptr<SSLClientSocket> sock = CreateSSLClientSocket(
          std::move(socket), HostPortPair("example.com", 443), ssl_config);
      ASSERT_THAT(callback.GetResult(sock->Connect(callback.callback())),
                  IsOk());
      SSLInfo ssl_info;
      ASSERT_TRUE(sock->GetSSLInfo(&ssl_info));
      sock.reset();

      switch (i) {
        case 0:
          // Initial handshake should be a full handshake.
          EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
          break;
        case 1:
          // Second handshake should resume.
          EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
          break;
        case 2:
          // Third handshake gets a different IP address and, if the
          // session used RSA key exchange, it should not resume.
          EXPECT_EQ(
              use_rsa ? SSLInfo::HANDSHAKE_FULL : SSLInfo::HANDSHAKE_RESUME,
              ssl_info.handshake_type);
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }
}

// Tests that ALPN works with session resumption.
TEST_F(SSLClientSocketTest, SessionResumptionAlpn) {
  SSLServerConfig server_config;
  server_config.alpn_protos = {NextProto::kProtoHTTP2, NextProto::kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // First, perform a full handshake.
  SSLConfig ssl_config;
  ssl_config.alpn_protos.push_back(kProtoHTTP2);
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  EXPECT_EQ(kProtoHTTP2, sock_->GetNegotiatedProtocol());

  // TLS 1.2 with False Start and TLS 1.3 cause the ticket to arrive later, so
  // use the socket to ensure the session ticket has been picked up.
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());

  // The next connection should resume; ALPN should be renegotiated.
  ssl_config.alpn_protos.clear();
  ssl_config.alpn_protos.push_back(kProtoHTTP11);
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_EQ(kProtoHTTP11, sock_->GetNegotiatedProtocol());
}

// Tests that the session cache is not sharded by NetworkAnonymizationKey if the
// feature is disabled.
TEST_P(SSLClientSocketVersionTest,
       SessionResumptionNetworkIsolationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  // First, perform a full handshake.
  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // TLS 1.2 with False Start and TLS 1.3 cause the ticket to arrive later, so
  // use the socket to ensure the session ticket has been picked up. Do this for
  // every connection to avoid problems with TLS 1.3 single-use tickets.
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());

  // The next connection should resume.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  // Using a different NetworkAnonymizationKey shares session cache key because
  // sharding is disabled.
  const SchemefulSite kSiteA(GURL("https://a.test"));
  ssl_config.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteA);
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  const SchemefulSite kSiteB(GURL("https://a.test"));
  ssl_config.network_anonymization_key =
      NetworkAnonymizationKey::CreateSameSite(kSiteB);
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();
}

// Tests that the session cache is sharded by NetworkAnonymizationKey if the
// feature is enabled.
TEST_P(SSLClientSocketVersionTest,
       SessionResumptionNetworkIsolationKeyEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const SchemefulSite kSiteA(GURL("https://a.test"));
  const SchemefulSite kSiteB(GURL("https://b.test"));
  const auto kNetworkAnonymizationKeyA =
      NetworkAnonymizationKey::CreateSameSite(kSiteA);
  const auto kNetworkAnonymizationKeyB =
      NetworkAnonymizationKey::CreateSameSite(kSiteB);

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  // First, perform a full handshake.
  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);

  // TLS 1.2 with False Start and TLS 1.3 cause the ticket to arrive later, so
  // use the socket to ensure the session ticket has been picked up. Do this for
  // every connection to avoid problems with TLS 1.3 single-use tickets.
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());

  // The next connection should resume.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  // Using a different NetworkAnonymizationKey uses a different session cache
  // key.
  ssl_config.network_anonymization_key = kNetworkAnonymizationKeyA;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  // We, however, can resume under that newly-established session.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  // Repeat with another non-null key.
  ssl_config.network_anonymization_key = kNetworkAnonymizationKeyB;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();

  // b.test does not evict a.test's session.
  ssl_config.network_anonymization_key = kNetworkAnonymizationKeyA;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  sock_.reset();
}

// Tests that connections with certificate errors do not add entries to the
// session cache.
TEST_P(SSLClientSocketVersionTest, CertificateErrorNoResume) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));

  cert_verifier_->set_default_result(ERR_CERT_COMMON_NAME_INVALID);

  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsError(ERR_CERT_COMMON_NAME_INVALID));

  cert_verifier_->set_default_result(OK);

  // The next connection should perform a full handshake.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  ASSERT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

TEST_F(SSLClientSocketTest, RequireECDHE) {
  // Run test server without ECDHE.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kRSACipher;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig config;
  config.require_ecdhe = true;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

TEST_F(SSLClientSocketTest, 3DES) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = k3DESCipher;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // 3DES is always disabled.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

TEST_F(SSLClientSocketTest, SHA1) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  // Disable RSA key exchange, to ensure the server does not pick a non-signing
  // cipher.
  server_config.require_ecdhe = true;
  server_config.signature_algorithm_for_testing = SSL_SIGN_RSA_PKCS1_SHA1;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // SHA-1 server signatures are always disabled.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

TEST_F(SSLClientSocketFalseStartTest, FalseStartEnabled) {
  // False Start requires ALPN, ECDHE, and an AEAD.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_config, client_config, true));
}

// Test that False Start is disabled without ALPN.
TEST_F(SSLClientSocketFalseStartTest, NoAlpn) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  SSLConfig client_config;
  client_config.alpn_protos.clear();
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_config, client_config, false));
}

// Test that False Start is disabled with plain RSA ciphers.
TEST_F(SSLClientSocketFalseStartTest, RSA) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kRSACipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_config, client_config, false));
}

// Test that False Start is disabled without an AEAD.
TEST_F(SSLClientSocketFalseStartTest, NoAEAD) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kCBCCipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_config, client_config, false));
}

// Test that sessions are resumable after receiving the server Finished message.
TEST_F(SSLClientSocketFalseStartTest, SessionResumption) {
  // Start a server.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);

  // Let a full handshake complete with False Start.
  ASSERT_NO_FATAL_FAILURE(TestFalseStart(server_config, client_config, true));

  // Make a second connection.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  // It should resume the session.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that the client completes the handshake in the background and installs
// new sessions, even if the socket isn't used. This also avoids a theoretical
// deadlock if NewSessionTicket is sufficiently large that neither it nor the
// client's HTTP/1.1 POST fit in transport windows.
TEST_F(SSLClientSocketFalseStartTest, CompleteHandshakeWithoutRequest) {
  // Start a server.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);

  // Start a handshake up to the server Finished message.
  TestCompletionCallback callback;
  FakeBlockingStreamSocket* raw_transport = nullptr;
  std::unique_ptr<SSLClientSocket> sock;
  ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
      client_config, &callback, &raw_transport, &sock));

  // Wait for the server Finished to arrive, release it, and allow
  // SSLClientSocket to process it. This should install a session. It make take
  // a few iterations to complete if the server writes in small chunks
  while (ssl_client_session_cache_->size() == 0) {
    raw_transport->WaitForReadResult();
    raw_transport->UnblockReadResult();
    base::RunLoop().RunUntilIdle();
    raw_transport->BlockReadResult();
  }

  // Drop the old socket. This is needed because the Python test server can't
  // service two sockets in parallel.
  sock.reset();

  // Make a second connection.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  // It should resume the session.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that False Started sessions are not resumable before receiving the
// server Finished message.
TEST_F(SSLClientSocketFalseStartTest, NoSessionResumptionBeforeFinished) {
  // Start a server.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);

  // Start a handshake up to the server Finished message.
  TestCompletionCallback callback;
  FakeBlockingStreamSocket* raw_transport1 = nullptr;
  std::unique_ptr<SSLClientSocket> sock1;
  ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
      client_config, &callback, &raw_transport1, &sock1));
  // Although raw_transport1 has the server Finished blocked, the handshake
  // still completes.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Continue to block the client (|sock1|) from processing the Finished
  // message, but allow it to arrive on the socket. This ensures that, from the
  // server's point of view, it has completed the handshake and added the
  // session to its session cache.
  //
  // The actual read on |sock1| will not complete until the Finished message is
  // processed; however, pump the underlying transport so that it is read from
  // the socket. NOTE: This may flakily pass if the server's final flight
  // doesn't come in one Read.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int rv = sock1->Read(buf.get(), 4096, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport1->WaitForReadResult();

  // Drop the old socket. This is needed because the Python test server can't
  // service two sockets in parallel.
  sock1.reset();

  // Start a second connection.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  // No session resumption because the first connection never received a server
  // Finished message.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Test that False Started sessions are not resumable if the server Finished
// message was bad.
TEST_F(SSLClientSocketFalseStartTest, NoSessionResumptionBadFinished) {
  // Start a server.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = kModernTLS12Cipher;
  server_config.alpn_protos = {NextProto::kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);

  // Start a handshake up to the server Finished message.
  TestCompletionCallback callback;
  FakeBlockingStreamSocket* raw_transport1 = nullptr;
  std::unique_ptr<SSLClientSocket> sock1;
  ASSERT_NO_FATAL_FAILURE(CreateAndConnectUntilServerFinishedReceived(
      client_config, &callback, &raw_transport1, &sock1));
  // Although raw_transport1 has the server Finished blocked, the handshake
  // still completes.
  EXPECT_THAT(callback.WaitForResult(), IsOk());

  // Continue to block the client (|sock1|) from processing the Finished
  // message, but allow it to arrive on the socket. This ensures that, from the
  // server's point of view, it has completed the handshake and added the
  // session to its session cache.
  //
  // The actual read on |sock1| will not complete until the Finished message is
  // processed; however, pump the underlying transport so that it is read from
  // the socket.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int rv = sock1->Read(buf.get(), 4096, callback.callback());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport1->WaitForReadResult();

  // The server's second leg, or part of it, is now received but not yet sent to
  // |sock1|. Before doing so, break the server's second leg.
  int bytes_read = raw_transport1->pending_read_result();
  ASSERT_LT(0, bytes_read);
  raw_transport1->pending_read_buf()->data()[bytes_read - 1]++;

  // Unblock the Finished message. |sock1->Read| should now fail.
  raw_transport1->UnblockReadResult();
  EXPECT_THAT(callback.GetResult(rv), IsError(ERR_SSL_PROTOCOL_ERROR));

  // Drop the old socket. This is needed because the Python test server can't
  // service two sockets in parallel.
  sock1.reset();

  // Start a second connection.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  // No session resumption because the first connection never received a server
  // Finished message.
  SSLInfo ssl_info;
  EXPECT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Server preference should win in ALPN.
TEST_F(SSLClientSocketTest, Alpn) {
  SSLServerConfig server_config;
  server_config.alpn_protos = {NextProto::kProtoHTTP2, NextProto::kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;
  client_config.alpn_protos.push_back(kProtoHTTP11);
  client_config.alpn_protos.push_back(kProtoHTTP2);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  EXPECT_EQ(kProtoHTTP2, sock_->GetNegotiatedProtocol());
}

// If the server supports ALPN but the client does not, then ALPN is not used.
TEST_F(SSLClientSocketTest, AlpnClientDisabled) {
  SSLServerConfig server_config;
  server_config.alpn_protos = {NextProto::kProtoHTTP2};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  EXPECT_EQ(kProtoUnknown, sock_->GetNegotiatedProtocol());
}

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
// Connect to a server requesting client authentication, do not send
// any client certificates. It should refuse the connection.
TEST_P(SSLClientSocketVersionTest, NoCert) {
  SSLServerConfig server_config = GetServerConfig();
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));

  EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));
  EXPECT_FALSE(sock_->IsConnected());
}

// Connect to a server requesting client authentication, and send it
// an empty certificate.
TEST_P(SSLClientSocketVersionTest, SendEmptyCert) {
  SSLServerConfig server_config = GetServerConfig();
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));

  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.client_cert_sent);
}

// Connect to a server requesting client authentication and send a certificate.
TEST_P(SSLClientSocketVersionTest, SendGoodCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(certs_dir, "client_1.pem");
  ASSERT_TRUE(client_cert);

  // Configure the server to only accept |client_cert|.
  MockClientCertVerifier verifier;
  verifier.set_default_result(ERR_CERT_INVALID);
  verifier.AddResultForCert(client_cert.get(), OK);

  SSLServerConfig server_config = GetServerConfig();
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  server_config.client_cert_verifier = &verifier;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  context_->SetClientCertificate(
      host_port_pair(), client_cert,
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));

  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.client_cert_sent);

  sock_->Disconnect();
  EXPECT_FALSE(sock_->IsConnected());

  // Shut down the test server before |verifier| goes out of scope.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// When client certificate preferences change, the session cache should be
// cleared so the client certificate preferences are applied.
TEST_F(SSLClientSocketTest, ClearSessionCacheOnClientCertChange) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Connecting without a client certificate will fail with
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

  // Configure a client certificate.
  base::FilePath certs_dir = GetTestCertsDirectory();
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));

  // Now the connection succeeds.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.client_cert_sent);
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_FULL);

  // Make a second connection. This should resume the session from the previous
  // connection.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.client_cert_sent);
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_RESUME);

  // Clear the client certificate preference.
  context_->ClearClientCertificate(host_port_pair());

  // Connections return to failing, rather than resume the previous session.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED));

  // Establish a new session with the correct client certificate.
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.client_cert_sent);
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_FULL);

  // Switch to continuing without a client certificate.
  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);

  // This also clears the session cache and the new preference is applied.
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
}

TEST_F(SSLClientSocketTest, ClearSessionCacheOnClientCertDatabaseChange) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  HostPortPair host_port_pair2("example.com", 42);
  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})));
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair2})));
  EXPECT_CALL(observer,
              OnSSLConfigForServersChanged(base::flat_set<HostPortPair>(
                  {host_port_pair(), host_port_pair2})));

  context_->AddObserver(&observer);

  base::FilePath certs_dir = GetTestCertsDirectory();
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));

  context_->SetClientCertificate(
      host_port_pair2, ImportCertFromFile(certs_dir, "client_2.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_2.key")));

  EXPECT_EQ(2U, context_->GetClientCertificateCachedServersForTesting().size());

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  EXPECT_EQ(1U, context_->ssl_client_session_cache()->size());

  CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0U, context_->GetClientCertificateCachedServersForTesting().size());
  EXPECT_EQ(0U, context_->ssl_client_session_cache()->size());

  context_->RemoveObserver(&observer);
}

TEST_F(SSLClientSocketTest, DontClearEmptyClientCertCache) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  context_->AddObserver(&observer);

  // No cached client certs and no open session.
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  context_->ClearClientCertificateIfNeeded(host_port_pair(), certificate1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_FALSE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest, DontClearMatchingClientCertificates) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})));
  context_->AddObserver(&observer);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> private_key1 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));

  context_->SetClientCertificate(host_port_pair(), certificate1, private_key1);
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  context_->ClearClientCertificateIfNeeded(host_port_pair(), certificate1);
  base::RunLoop().RunUntilIdle();

  // Cached certificate and session should not have been cleared since the
  // certificates were identical.
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().contains(
      host_port_pair()));
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_FALSE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest, ClearMismatchingClientCertificates) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})))
      .Times(2);
  context_->AddObserver(&observer);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> private_key1 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));

  context_->SetClientCertificate(host_port_pair(), certificate1, private_key1);
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  scoped_refptr<net::X509Certificate> certificate2 =
      ImportCertFromFile(certs_dir, "client_2.pem");
  context_->ClearClientCertificateIfNeeded(host_port_pair(), certificate2);
  base::RunLoop().RunUntilIdle();

  // Cached certificate and session should have been cleared since the
  // certificates were different.
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_TRUE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest,
       ClearMismatchingClientCertificatesWithNullParameter) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})))
      .Times(2);
  context_->AddObserver(&observer);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> private_key1 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));

  context_->SetClientCertificate(host_port_pair(), certificate1, private_key1);
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  context_->ClearClientCertificateIfNeeded(host_port_pair(), nullptr);
  base::RunLoop().RunUntilIdle();

  // Cached certificate and session should have been cleared since the
  // certificates were different.
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_TRUE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest,
       ClearMismatchingClientCertificatesWithNullCachedCert) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})))
      .Times(2);
  context_->AddObserver(&observer);

  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate2 =
      ImportCertFromFile(certs_dir, "client_2.pem");
  context_->ClearClientCertificateIfNeeded(host_port_pair(), certificate2);
  base::RunLoop().RunUntilIdle();

  // Cached certificate and session should have been cleared since the
  // certificates were different.
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_TRUE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest, DontClearClientCertificatesWithNullCerts) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})));
  context_->AddObserver(&observer);

  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  context_->ClearClientCertificateIfNeeded(host_port_pair(), nullptr);
  base::RunLoop().RunUntilIdle();

  // Cached certificate and session should not have been cleared since the
  // certificates were identical.
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);
  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().contains(
      host_port_pair()));
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  context_->RemoveObserver(&observer);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(GetStringValueFromParams(entries[0], "host"),
            host_port_pair().ToString());
  EXPECT_FALSE(GetBooleanValueFromParams(entries[0], "is_cleared"));
}

TEST_F(SSLClientSocketTest, ClearMatchingCertDontClearEmptyClientCertCache) {
  SSLServerConfig server_config;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // No cached client certs and no open session.
  ASSERT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  ASSERT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  context_->ClearMatchingClientCertificate(certificate1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context_->GetClientCertificateCachedServersForTesting().empty());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_MATCHING_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());

  const auto& log_entry = entries[0];
  ASSERT_FALSE(log_entry.params.empty());

  const base::Value::List* hosts_values =
      log_entry.params.FindListByDottedPath("hosts");
  ASSERT_TRUE(hosts_values);
  ASSERT_TRUE(hosts_values->empty());

  const base::Value::List* certificates_values =
      log_entry.params.FindListByDottedPath("certificates");
  ASSERT_TRUE(certificates_values);
  EXPECT_FALSE(certificates_values->empty());
}

TEST_F(SSLClientSocketTest, ClearMatchingCertSingleNotMatching) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Add a client cert decision to the cache.
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> private_key1 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));
  context_->SetClientCertificate(host_port_pair(), certificate1, private_key1);
  ASSERT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);

  // Create a connection to `host_port_pair()`.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  scoped_refptr<net::X509Certificate> certificate2 =
      ImportCertFromFile(certs_dir, "client_2.pem");
  context_->ClearMatchingClientCertificate(certificate2);
  base::RunLoop().RunUntilIdle();

  // Verify that calling with an unused certificate should not invalidate the
  // cache, but will still log an event with no hosts.
  EXPECT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 1U);
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_MATCHING_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());

  const auto& log_entry = entries[0];
  ASSERT_FALSE(log_entry.params.empty());

  const base::Value::List* hosts_values =
      log_entry.params.FindListByDottedPath("hosts");
  ASSERT_TRUE(hosts_values);
  ASSERT_TRUE(hosts_values->empty());

  const base::Value::List* certificates_values =
      log_entry.params.FindListByDottedPath("certificates");
  ASSERT_TRUE(certificates_values);
  EXPECT_FALSE(certificates_values->empty());
}

TEST_F(SSLClientSocketTest, ClearMatchingCertSingleMatching) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Add a couple of client cert decision to the cache.
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> certificate1 =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> private_key1 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));
  context_->SetClientCertificate(host_port_pair(), certificate1, private_key1);

  HostPortPair host_port_pair2("example.com", 42);
  scoped_refptr<net::X509Certificate> certificate2 =
      ImportCertFromFile(certs_dir, "client_2.pem");
  scoped_refptr<net::SSLPrivateKey> private_key2 =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_2.key"));
  context_->SetClientCertificate(host_port_pair2, certificate2, private_key2);
  ASSERT_EQ(context_->GetClientCertificateCachedServersForTesting().size(), 2U);

  // Create a connection to `host_port_pair()`.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 1U);

  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})));
  context_->AddObserver(&observer);

  context_->ClearMatchingClientCertificate(certificate1);
  base::RunLoop().RunUntilIdle();

  context_->RemoveObserver(&observer);
  auto cached_servers_with_decision =
      context_->GetClientCertificateCachedServersForTesting();
  EXPECT_EQ(cached_servers_with_decision.size(), 1U);
  EXPECT_TRUE(cached_servers_with_decision.contains(host_port_pair2));

  EXPECT_EQ(context_->ssl_client_session_cache()->size(), 0U);

  auto entries = log_observer_.GetEntriesWithType(
      NetLogEventType::CLEAR_MATCHING_CACHED_CLIENT_CERT);
  ASSERT_EQ(1u, entries.size());

  const auto& log_entry = entries[0];
  ASSERT_FALSE(log_entry.params.empty());

  const base::Value::List* hosts_values =
      log_entry.params.FindListByDottedPath("hosts");
  ASSERT_TRUE(hosts_values);
  ASSERT_EQ(hosts_values->size(), 1U);
  EXPECT_EQ(hosts_values->front().GetString(), host_port_pair().ToString());

  const base::Value::List* certificates_values =
      log_entry.params.FindListByDottedPath("certificates");
  ASSERT_TRUE(certificates_values);
  EXPECT_FALSE(certificates_values->empty());
}

TEST_F(SSLClientSocketTest, DontClearSessionCacheOnServerCertDatabaseChange) {
  SSLServerConfig server_config;
  // TLS 1.3 reports client certificate errors after the handshake, so test at
  // TLS 1.2 for simplicity.
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  HostPortPair host_port_pair2("example.com", 42);
  testing::StrictMock<MockSSLClientContextObserver> observer;
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair()})));
  EXPECT_CALL(observer, OnSSLConfigForServersChanged(
                            base::flat_set<HostPortPair>({host_port_pair2})));
  EXPECT_CALL(observer,
              OnSSLConfigChanged(
                  SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged));

  context_->AddObserver(&observer);

  base::FilePath certs_dir = GetTestCertsDirectory();
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));

  context_->SetClientCertificate(
      host_port_pair2, ImportCertFromFile(certs_dir, "client_2.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_2.key")));

  EXPECT_EQ(2U, context_->GetClientCertificateCachedServersForTesting().size());

  // Connect to `host_port_pair()` using the client cert.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  EXPECT_EQ(1U, context_->ssl_client_session_cache()->size());

  CertDatabase::GetInstance()->NotifyObserversTrustStoreChanged();
  base::RunLoop().RunUntilIdle();

  // The `OnSSLConfigChanged` observer call should be verified by the
  // mock observer, but the client auth and client session cache should be
  // untouched.

  EXPECT_EQ(2U, context_->GetClientCertificateCachedServersForTesting().size());
  EXPECT_EQ(1U, context_->ssl_client_session_cache()->size());

  context_->RemoveObserver(&observer);
}

// Test client certificate signature algorithm selection.
TEST_F(SSLClientSocketTest, ClientCertSignatureAlgorithm) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> client_cert =
      ImportCertFromFile(certs_dir, "client_1.pem");
  scoped_refptr<net::SSLPrivateKey> client_key =
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key"));

  const struct {
    const char* name;
    bool legacy_pkcs1_enabled = true;
    uint16_t version;
    std::vector<uint16_t> server_prefs;
    std::vector<uint16_t> client_prefs;
    Error error = OK;
    uint16_t expected_signature_algorithm = 0;
  } kTests[] = {
      {
          .name = "TLS 1.2 client preference",
          .version = SSL_PROTOCOL_VERSION_TLS1_2,
          .server_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA384,
                           SSL_SIGN_RSA_PSS_RSAE_SHA256},
          .client_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA256,
                           SSL_SIGN_RSA_PSS_RSAE_SHA384},
          // The client's preference should be used.
          .expected_signature_algorithm = SSL_SIGN_RSA_PSS_RSAE_SHA256,
      },
      {
          .name = "TLS 1.3 client preference",
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA384,
                           SSL_SIGN_RSA_PSS_RSAE_SHA256},
          .client_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA256,
                           SSL_SIGN_RSA_PSS_RSAE_SHA384},
          // The client's preference should be used.
          .expected_signature_algorithm = SSL_SIGN_RSA_PSS_RSAE_SHA256,
      },

      {
          .name = "TLS 1.2 no common algorithms",
          .version = SSL_PROTOCOL_VERSION_TLS1_2,
          .server_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA384},
          .client_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA256},
          .error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
      },
      {
          .name = "TLS 1.3 no common algorithms",
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA384},
          .client_prefs = {SSL_SIGN_RSA_PSS_RSAE_SHA256},
          .error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
      },

      {
          .name = "TLS 1.2 PKCS#1",
          .version = SSL_PROTOCOL_VERSION_TLS1_2,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          .expected_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA256,
      },
      {
          .name = "TLS 1.2 no PKCS#1",
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          // The rsa_pkcs1_sha256 codepoint may not be used in TLS 1.3, so the
          // TLS library should exclude it.
          .error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
      },

      // Test rsa_pkcs1_sha256_legacy. The value is omitted from `client_prefs`
      // because SSLPrivateKey implementations are not expected to specify
      // `SSL_SIGN_RSA_PKCS1_SHA256_LEGACY`. Instead, SSLClientSocket
      // automatically applies support when `SSL_SIGN_RSA_PKCS1_SHA256` is
      // available.
      {
          .name = "TLS 1.2 no legacy PKCS#1",
          .version = SSL_PROTOCOL_VERSION_TLS1_2,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256_LEGACY},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          // The rsa_pkcs1_sha256_legacy codepoint is specifically for
          // restoring PKCS#1 to TLS 1.3, so it should not be accepted.
          .error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
      },
      {
          .name = "TLS 1.3 legacy PKCS#1",
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256_LEGACY},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          // The rsa_pkcs1_sha256_legacy codepoint may be used in TLS 1.3.
          .expected_signature_algorithm = SSL_SIGN_RSA_PKCS1_SHA256_LEGACY,
      },
      {
          .name = "TLS 1.3 legacy PKCS#1 disabled",
          .legacy_pkcs1_enabled = false,
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256_LEGACY},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256},
          // The rsa_pkcs1_sha256_legacy codepoint may be used in TLS 1.3, but
          // was disabled.
          .error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS,
      },
      {
          .name = "TLS 1.3 legacy PKCS#1 not preferred",
          .version = SSL_PROTOCOL_VERSION_TLS1_3,
          .server_prefs = {SSL_SIGN_RSA_PKCS1_SHA256_LEGACY,
                           SSL_SIGN_RSA_PSS_RSAE_SHA256},
          .client_prefs = {SSL_SIGN_RSA_PKCS1_SHA256,
                           SSL_SIGN_RSA_PSS_RSAE_SHA256},
          // The legacy codepoint is only used when no other options are
          // available. The key supports PSS, so we will use PSS instead.
          .expected_signature_algorithm = SSL_SIGN_RSA_PSS_RSAE_SHA256,
      },
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        net::features::kLegacyPKCS1ForTLS13, test.legacy_pkcs1_enabled);

    SSLServerConfig server_config;
    server_config.version_min = test.version;
    server_config.version_max = test.version;
    server_config.client_cert_type = SSLServerConfig::REQUIRE_CLIENT_CERT;
    server_config.client_cert_signature_algorithms = test.server_prefs;
    ASSERT_TRUE(
        StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

    // Connect with the client certificate.
    context_->SetClientCertificate(
        host_port_pair(), client_cert,
        WrapSSLPrivateKeyWithPreferences(client_key, test.client_prefs));
    int rv;
    ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
    if (test.error != OK) {
      EXPECT_THAT(rv, IsError(test.error));
      continue;
    }

    EXPECT_THAT(rv, IsOk());
    EXPECT_TRUE(sock_->IsConnected());

    // Capture the SSLInfo from the server to get the client's chosen signature
    // algorithm.
    EXPECT_THAT(MakeHTTPRequest(sock_.get(), "/ssl-info"), IsOk());
    std::optional<SSLInfo> server_ssl_info = LastSSLInfoFromServer();
    ASSERT_TRUE(server_ssl_info);
    EXPECT_EQ(server_ssl_info->peer_signature_algorithm,
              test.expected_signature_algorithm);
  }
}
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

HashValueVector MakeHashValueVector(uint8_t value) {
  HashValueVector out;
  HashValue hash(HASH_VALUE_SHA256);
  memset(hash.data(), value, hash.size());
  out.push_back(hash);
  return out;
}

// Test that |ssl_info.pkp_bypassed| is set when a local trust anchor causes
// pinning to be bypassed.
TEST_P(SSLClientSocketVersionTest, PKPBypassedSet) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // The certificate needs to be trusted, but chain to a local root with
  // different public key hashes than specified in the pin.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = false;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kBadHashValueVectorInput);
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  transport_security_state_->EnableStaticPinsForTesting();
  transport_security_state_->SetPinningListAlwaysTimelyForTesting(true);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  SSLConfig ssl_config;
  int rv;
  HostPortPair new_host_port_pair("example.test", host_port_pair().port());
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(ssl_config,
                                                      new_host_port_pair, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(sock_->IsConnected());

  EXPECT_TRUE(ssl_info.pkp_bypassed);
  EXPECT_FALSE(ssl_info.cert_status & CERT_STATUS_PINNED_KEY_MISSING);
}

TEST_P(SSLClientSocketVersionTest, PKPEnforced) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted, but chains to a public root that doesn't match the
  // pin hashes.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kBadHashValueVectorInput);
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  transport_security_state_->EnableStaticPinsForTesting();
  transport_security_state_->SetPinningListAlwaysTimelyForTesting(true);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  SSLConfig ssl_config;
  int rv;
  HostPortPair new_host_port_pair("example.test", host_port_pair().port());
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(ssl_config,
                                                      new_host_port_pair, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  EXPECT_THAT(rv, IsError(ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN));
  EXPECT_TRUE(ssl_info.cert_status & CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_FALSE(sock_->IsConnected());

  EXPECT_FALSE(ssl_info.pkp_bypassed);
}

namespace {
// TLS_RSA_WITH_AES_128_GCM_SHA256's key exchange involves encrypting to the
// server long-term key.
const uint16_t kEncryptingCipher = kRSACipher;
// TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256's key exchange involves a signature by
// the server long-term key.
const uint16_t kSigningCipher = kModernTLS12Cipher;
}  // namespace

struct KeyUsageTest {
  EmbeddedTestServer::ServerCertificate server_cert;
  uint16_t cipher_suite;
  bool match;
};

class SSLClientSocketKeyUsageTest
    : public SSLClientSocketTest,
      public ::testing::WithParamInterface<
          std::tuple<KeyUsageTest, bool /*known_root*/>> {};

const KeyUsageTest kKeyUsageTests[] = {
    // keyUsage matches cipher suite.
    {EmbeddedTestServer::CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE, kSigningCipher,
     true},
    {EmbeddedTestServer::CERT_KEY_USAGE_RSA_ENCIPHERMENT, kEncryptingCipher,
     true},
    // keyUsage does not match cipher suite.
    {EmbeddedTestServer::CERT_KEY_USAGE_RSA_ENCIPHERMENT, kSigningCipher,
     false},
    {EmbeddedTestServer::CERT_KEY_USAGE_RSA_DIGITAL_SIGNATURE,
     kEncryptingCipher, false},
};

TEST_P(SSLClientSocketKeyUsageTest, RSAKeyUsage) {
  const auto& [test, known_root] = GetParam();
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.cipher_suite_for_testing = test.cipher_suite;
  ASSERT_TRUE(StartEmbeddedTestServer(test.server_cert, server_config));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = known_root;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kGoodHashValueVectorInput);
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  if (test.match) {
    EXPECT_THAT(rv, IsOk());
    EXPECT_TRUE(sock_->IsConnected());
  } else {
    EXPECT_THAT(rv, IsError(ERR_SSL_KEY_USAGE_INCOMPATIBLE));
    EXPECT_FALSE(sock_->IsConnected());
  }
}

INSTANTIATE_TEST_SUITE_P(RSAKeyUsageInstantiation,
                         SSLClientSocketKeyUsageTest,
                         Combine(ValuesIn(kKeyUsageTests), Bool()));

// Test that when CT is required (in this case, by the delegate), the
// absence of CT information is a socket error.
TEST_P(SSLClientSocketVersionTest, CTIsRequired) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted and chains to a public root.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kGoodHashValueVectorInput);
  verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  // Set up CT
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_->SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate,
              IsCTRequiredForHost(host_port_pair().host(), _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));

  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  EXPECT_THAT(rv, IsError(ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  EXPECT_TRUE(ssl_info.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(sock_->IsConnected());
}

// Test that when CT is required, setting ignore_certificate_errors
// ignores errors in CT.
TEST_P(SSLClientSocketVersionTest, IgnoreCertificateErrorsBypassesRequiredCT) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted and chains to a public root.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kGoodHashValueVectorInput);
  verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  // Set up CT
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_->SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate,
              IsCTRequiredForHost(host_port_pair().host(), _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));

  SSLConfig ssl_config;
  ssl_config.ignore_certificate_errors = true;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(ssl_info.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_TRUE(sock_->IsConnected());
}

// When both PKP and CT are required for a host, and both fail, the more
// serious error is that the pin validation failed.
TEST_P(SSLClientSocketVersionTest, PKPMoreImportantThanCT) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted, but chains to a public root that doesn't match the
  // pin hashes.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kBadHashValueVectorInput);
  verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  transport_security_state_->EnableStaticPinsForTesting();
  transport_security_state_->SetPinningListAlwaysTimelyForTesting(true);
  ScopedTransportSecurityStateSource scoped_security_state_source;

  const char kCTHost[] = "hsts-hpkp-preloaded.test";

  // Set up CT.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_->SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::NOT_REQUIRED));
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(kCTHost, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));

  SSLConfig ssl_config;
  int rv;
  HostPortPair ct_host_port_pair(kCTHost, host_port_pair().port());
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(ssl_config,
                                                      ct_host_port_pair, &rv));
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));

  EXPECT_THAT(rv, IsError(ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN));
  EXPECT_TRUE(ssl_info.cert_status & CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_TRUE(ssl_info.cert_status &
              CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED);
  EXPECT_FALSE(sock_->IsConnected());
}

// Tests that the SCTAuditingDelegate is called to enqueue SCT reports.
TEST_P(SSLClientSocketVersionTest, SCTAuditingReportCollected) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, GetServerConfig()));
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();

  // Certificate is trusted and chains to a public root.
  CertVerifyResult verify_result;
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = server_cert;
  verify_result.public_key_hashes =
      MakeHashValueVector(kGoodHashValueVectorInput);
  verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
  cert_verifier_->AddResultForCert(server_cert.get(), verify_result, OK);

  // Set up CT and auditing delegate.
  MockRequireCTDelegate require_ct_delegate;
  transport_security_state_->SetRequireCTDelegate(&require_ct_delegate);
  EXPECT_CALL(require_ct_delegate, IsCTRequiredForHost(_, _, _))
      .WillRepeatedly(Return(TransportSecurityState::RequireCTDelegate::
                                 CTRequirementLevel::REQUIRED));

  MockSCTAuditingDelegate sct_auditing_delegate;
  context_ = std::make_unique<SSLClientContext>(
      ssl_config_service_.get(), cert_verifier_.get(),
      transport_security_state_.get(), ssl_client_session_cache_.get(),
      &sct_auditing_delegate);

  EXPECT_CALL(sct_auditing_delegate, IsSCTAuditingEnabled())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(sct_auditing_delegate,
              MaybeEnqueueReport(host_port_pair(), server_cert.get(), _))
      .Times(1);

  SSLConfig ssl_config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(ssl_config, &rv));
  EXPECT_THAT(rv, 0);
  EXPECT_TRUE(sock_->IsConnected());
}

// Test that handshake_failure alerts at the ServerHello are mapped to
// ERR_SSL_VERSION_OR_CIPHER_MISMATCH.
TEST_F(SSLClientSocketTest, HandshakeFailureServerHello) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(40 /* AlertDescription.handshake_failure */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

// Test that handshake_failure alerts after the ServerHello but without a
// CertificateRequest are mapped to ERR_SSL_PROTOCOL_ERROR.
TEST_F(SSLClientSocketTest, HandshakeFailureNoClientCerts) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Release the ServerHello and wait for the client to write its second flight.
  raw_transport->BlockWrite();
  raw_transport->UnblockReadResult();
  raw_transport->WaitForWrite();

  // Wait for the server's final flight.
  raw_transport->BlockReadResult();
  raw_transport->UnblockWrite();
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(40 /* AlertDescription.handshake_failure */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

// Test that handshake_failure alerts after the ServerHello map to
// ERR_BAD_SSL_CLIENT_AUTH_CERT if a client certificate was requested but not
// supplied. TLS does not have an alert for this case, so handshake_failure is
// common. See https://crbug.com/646567.
TEST_F(SSLClientSocketTest, LateHandshakeFailureMissingClientCerts) {
  // Request a client certificate.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  // Send no client certificate.
  context_->SetClientCertificate(host_port_pair(), nullptr, nullptr);
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Release the ServerHello and wait for the client to write its second flight.
  raw_transport->BlockWrite();
  raw_transport->UnblockReadResult();
  raw_transport->WaitForWrite();

  // Wait for the server's final flight.
  raw_transport->BlockReadResult();
  raw_transport->UnblockWrite();
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(40 /* AlertDescription.handshake_failure */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
}

// Test that handshake_failure alerts after the ServerHello map to
// ERR_SSL_PROTOCOL_ERROR if received after sending a client certificate. It is
// assumed servers will send a more appropriate alert in this case.
TEST_F(SSLClientSocketTest, LateHandshakeFailureSendClientCerts) {
  // Request a client certificate.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  // Send a client certificate.
  base::FilePath certs_dir = GetTestCertsDirectory();
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Release the ServerHello and wait for the client to write its second flight.
  raw_transport->BlockWrite();
  raw_transport->UnblockReadResult();
  raw_transport->WaitForWrite();

  // Wait for the server's final flight.
  raw_transport->BlockReadResult();
  raw_transport->UnblockWrite();
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(40 /* AlertDescription.handshake_failure */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

// Test that access_denied alerts are mapped to ERR_SSL_PROTOCOL_ERROR if
// received on a connection not requesting client certificates. This is an
// incorrect use of the alert but is common. See https://crbug.com/630883.
TEST_F(SSLClientSocketTest, AccessDeniedNoClientCerts) {
  // Request a client certificate.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Release the ServerHello and wait for the client to write its second flight.
  raw_transport->BlockWrite();
  raw_transport->UnblockReadResult();
  raw_transport->WaitForWrite();

  // Wait for the server's final flight.
  raw_transport->BlockReadResult();
  raw_transport->UnblockWrite();
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(49 /* AlertDescription.access_denied */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_SSL_PROTOCOL_ERROR));
}

// Test that access_denied alerts are mapped to ERR_BAD_SSL_CLIENT_AUTH_CERT if
// received on a connection requesting client certificates.
TEST_F(SSLClientSocketTest, AccessDeniedClientCerts) {
  // Request a client certificate.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.client_cert_type = SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  TestCompletionCallback callback;
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_THAT(rv, IsOk());

  // Send a client certificate.
  base::FilePath certs_dir = GetTestCertsDirectory();
  context_->SetClientCertificate(
      host_port_pair(), ImportCertFromFile(certs_dir, "client_1.pem"),
      key_util::LoadPrivateKeyOpenSSL(certs_dir.AppendASCII("client_1.key")));
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(transport), host_port_pair(), SSLConfig()));

  // Connect. Stop before the client processes ServerHello.
  raw_transport->BlockReadResult();
  rv = sock->Connect(callback.callback());
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));
  raw_transport->WaitForReadResult();

  // Release the ServerHello and wait for the client to write its second flight.
  raw_transport->BlockWrite();
  raw_transport->UnblockReadResult();
  raw_transport->WaitForWrite();

  // Wait for the server's final flight.
  raw_transport->BlockReadResult();
  raw_transport->UnblockWrite();
  raw_transport->WaitForReadResult();

  // Replace it with an alert.
  raw_transport->ReplaceReadResult(
      FormatTLS12Alert(49 /* AlertDescription.access_denied */));
  raw_transport->UnblockReadResult();

  rv = callback.GetResult(rv);
  EXPECT_THAT(rv, IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
}

// Test the client can send application data before the ServerHello comes in.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTEarlyDataBeforeServerHello) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() and Write() complete even though the
  // ServerHello is blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  // Release the ServerHello. Now reads complete.
  socket->UnblockReadResult();
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('1', buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that the client sends 1-RTT data if the ServerHello happens to come in
// before Write() is called. See https://crbug.com/950706.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTEarlyDataAfterServerHello) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() completes even though the ServerHello is
  // blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  // Wait for the ServerHello to come in and for SSLClientSocket to process it.
  socket->WaitForReadResult();
  socket->UnblockReadResult();
  base::RunLoop().RunUntilIdle();

  // Now write to the socket.
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  // Although the socket was created in early data state and the client never
  // explicitly called ReaD() or ConfirmHandshake(), SSLClientSocketImpl
  // internally consumed the ServerHello and switch keys. The server then
  // responds with '0'.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('0', buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Check that 0RTT is confirmed after a Write and Read.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTConfirmedAfterRead) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() and Write() complete even though the
  // ServerHello is blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  socket->UnblockReadResult();
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('1', buf->data()[size - 1]);

  // After the handshake is confirmed, ConfirmHandshake should return
  // synchronously.
  TestCompletionCallback callback;
  ASSERT_THAT(ssl_socket()->ConfirmHandshake(callback.callback()), IsOk());

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that writes wait for the ServerHello once it has reached the early data
// limit.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTEarlyDataLimit) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() completes even though the ServerHello is
  // blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  // EmbeddedTestServer uses BoringSSL's hard-coded early data limit, which is
  // below 16k.
  constexpr size_t kRequestSize = 16 * 1024;
  std::string request = "GET /zerortt HTTP/1.0\r\n";
  while (request.size() < kRequestSize) {
    request += "The-Answer-To-Life-The-Universe-And-Everything: 42\r\n";
  }
  request += "\r\n";

  // Writing the large input should not succeed. It is blocked on the
  // ServerHello.
  TestCompletionCallback write_callback;
  auto write_buf = base::MakeRefCounted<StringIOBuffer>(request);
  int write_rv = ssl_socket()->Write(write_buf.get(), request.size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_THAT(write_rv, IsError(ERR_IO_PENDING));

  // The Write should have issued a read for the ServerHello, so
  // WaitForReadResult has something to wait for.
  socket->WaitForReadResult();
  EXPECT_TRUE(socket->pending_read_result());

  // Queue a read. It should be blocked on the ServerHello.
  TestCompletionCallback read_callback;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int read_rv =
      ssl_socket()->Read(read_buf.get(), 4096, read_callback.callback());
  ASSERT_THAT(read_rv, IsError(ERR_IO_PENDING));

  // Also queue a ConfirmHandshake. It should also be blocked on ServerHello.
  TestCompletionCallback confirm_callback;
  int confirm_rv = ssl_socket()->ConfirmHandshake(confirm_callback.callback());
  ASSERT_THAT(confirm_rv, IsError(ERR_IO_PENDING));

  // Double-check the write was not accidentally blocked on the network.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(write_callback.have_result());

  // At this point, the maximum possible number of events are all blocked on the
  // same thing. Release the ServerHello. All three should complete.
  socket->UnblockReadResult();
  EXPECT_EQ(static_cast<int>(request.size()),
            write_callback.GetResult(write_rv));
  EXPECT_THAT(confirm_callback.GetResult(confirm_rv), IsOk());
  int size = read_callback.GetResult(read_rv);
  ASSERT_GT(size, 0);
  EXPECT_EQ('1', read_buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// When a client socket reaches the 0-RTT early data limit, both Write() and
// ConfirmHandshake() become blocked on a transport read. Test that
// CancelReadIfReady() does not interrupt those.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTEarlyDataLimitCancelReadIfReady) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() completes even though the ServerHello is
  // blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  // EmbeddedTestServer uses BoringSSL's hard-coded early data limit, which is
  // below 16k.
  constexpr size_t kRequestSize = 16 * 1024;
  std::string request = "GET /zerortt HTTP/1.0\r\n";
  while (request.size() < kRequestSize) {
    request += "The-Answer-To-Life-The-Universe-And-Everything: 42\r\n";
  }
  request += "\r\n";

  // Writing the large input should not succeed. It is blocked on the
  // ServerHello.
  TestCompletionCallback write_callback;
  auto write_buf = base::MakeRefCounted<StringIOBuffer>(request);
  int write_rv = ssl_socket()->Write(write_buf.get(), request.size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  ASSERT_THAT(write_rv, IsError(ERR_IO_PENDING));

  // The Write should have issued a read for the ServerHello, so
  // WaitForReadResult has something to wait for.
  socket->WaitForReadResult();
  EXPECT_TRUE(socket->pending_read_result());

  // Attempt a ReadIfReady(). It should be blocked on the ServerHello.
  TestCompletionCallback read_callback;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int read_rv =
      ssl_socket()->ReadIfReady(read_buf.get(), 4096, read_callback.callback());
  ASSERT_THAT(read_rv, IsError(ERR_IO_PENDING));

  // Also queue a ConfirmHandshake. It should also be blocked on ServerHello.
  TestCompletionCallback confirm_callback;
  int confirm_rv = ssl_socket()->ConfirmHandshake(confirm_callback.callback());
  ASSERT_THAT(confirm_rv, IsError(ERR_IO_PENDING));

  // Cancel the ReadIfReady() and release the ServerHello. The remaining
  // operations should complete.
  ASSERT_THAT(ssl_socket()->CancelReadIfReady(), IsOk());
  socket->UnblockReadResult();
  EXPECT_EQ(static_cast<int>(request.size()),
            write_callback.GetResult(write_rv));
  EXPECT_THAT(confirm_callback.GetResult(confirm_rv), IsOk());

  // ReadIfReady() should not complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);

  // After a canceled read, future reads are still possible.
  TestCompletionCallback read_callback2;
  read_rv = read_callback2.GetResult(
      ssl_socket()->Read(read_buf.get(), 4096, read_callback2.callback()));
  ASSERT_GT(read_rv, 0);
}

TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTReject) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  SSLServerConfig server_config;
  server_config.early_data_enabled = false;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;
  SetServerConfig(server_config);

  // 0-RTT Connection
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));
  socket->UnblockReadResult();

  // Expect early data to be rejected.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int rv = ReadAndWait(buf.get(), 4096);
  EXPECT_EQ(ERR_EARLY_DATA_REJECTED, rv);
  rv = WriteAndWait(kRequest);
  EXPECT_EQ(ERR_EARLY_DATA_REJECTED, rv);

  // Run the event loop so the rejection has reached the TLS session cache.
  base::RunLoop().RunUntilIdle();

  // Now that the session cache has been updated, retrying the connection
  // should succeed.
  socket = MakeClient(true);
  ASSERT_THAT(Connect(), IsOk());
  ASSERT_THAT(MakeHTTPRequest(ssl_socket()), IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTWrongVersion) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  SetServerConfig(server_config);

  // 0-RTT Connection
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));
  socket->UnblockReadResult();

  // Expect early data to be rejected because the TLS version was incorrect.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int rv = ReadAndWait(buf.get(), 4096);
  EXPECT_EQ(ERR_WRONG_VERSION_ON_EARLY_DATA, rv);
  rv = WriteAndWait(kRequest);
  EXPECT_EQ(ERR_WRONG_VERSION_ON_EARLY_DATA, rv);

  // Run the event loop so the rejection has reached the TLS session cache.
  base::RunLoop().RunUntilIdle();

  // Now that the session cache has been updated, retrying the connection
  // should succeed.
  socket = MakeClient(true);
  ASSERT_THAT(Connect(), IsOk());
  ASSERT_THAT(MakeHTTPRequest(ssl_socket()), IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
}

// Test that the ConfirmHandshake successfully completes the handshake and that
// it blocks until the server's leg has been received.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTConfirmHandshake) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // 0-RTT Connection
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  // The ServerHello is blocked, so ConfirmHandshake should not complete.
  TestCompletionCallback callback;
  ASSERT_EQ(ERR_IO_PENDING,
            ssl_socket()->ConfirmHandshake(callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  // Release the ServerHello. ConfirmHandshake now completes.
  socket->UnblockReadResult();
  ASSERT_THAT(callback.GetResult(ERR_IO_PENDING), IsOk());

  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('0', buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

// Test that an early read does not break during zero RTT.
TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTReadBeforeWrite) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // Make a 0-RTT Connection. Connect() completes even though the ServerHello is
  // blocked.
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  // Read() does not make progress.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            ssl_socket()->Read(buf.get(), 4096, read_callback.callback()));

  // Write() completes, even though reads are blocked.
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  // Release the ServerHello, etc. The Read() now completes.
  socket->UnblockReadResult();
  int size = read_callback.GetResult(ERR_IO_PENDING);
  EXPECT_GT(size, 0);
  EXPECT_EQ('1', buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTDoubleConfirmHandshake) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // 0-RTT Connection
  MakeClient(true);
  ASSERT_THAT(Connect(), IsOk());
  TestCompletionCallback callback;
  ASSERT_THAT(
      callback.GetResult(ssl_socket()->ConfirmHandshake(callback.callback())),
      IsOk());
  // After the handshake is confirmed, ConfirmHandshake should return
  // synchronously.
  ASSERT_THAT(ssl_socket()->ConfirmHandshake(callback.callback()), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('0', buf->data()[size - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

TEST_F(SSLClientSocketZeroRTTTest, ZeroRTTParallelReadConfirm) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // 0-RTT Connection
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());

  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  // The ServerHello is blocked, so ConfirmHandshake should not complete.
  TestCompletionCallback callback;
  ASSERT_EQ(ERR_IO_PENDING,
            ssl_socket()->ConfirmHandshake(callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(callback.have_result());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  TestCompletionCallback read_callback;
  ASSERT_EQ(ERR_IO_PENDING,
            ssl_socket()->Read(buf.get(), 4096, read_callback.callback()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  // Release the ServerHello. ConfirmHandshake now completes.
  socket->UnblockReadResult();
  ASSERT_THAT(callback.WaitForResult(), IsOk());

  int result = read_callback.WaitForResult();
  EXPECT_GT(result, 0);
  EXPECT_EQ('1', buf->data()[result - 1]);

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
}

TEST_P(SSLClientSocketReadTest, IdleAfterRead) {
  // Set up a TCP server.
  TCPServerSocket server_listener(nullptr, NetLogSource());
  ASSERT_THAT(server_listener.Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0),
                                     1, /*ipv6_only=*/std::nullopt),
              IsOk());
  IPEndPoint server_address;
  ASSERT_THAT(server_listener.GetLocalAddress(&server_address), IsOk());

  // Connect a TCP client and server socket.
  TestCompletionCallback server_callback;
  std::unique_ptr<StreamSocket> server_transport;
  int server_rv =
      server_listener.Accept(&server_transport, server_callback.callback());

  TestCompletionCallback client_callback;
  auto client_transport = std::make_unique<TCPClientSocket>(
      AddressList(server_address), nullptr, nullptr, nullptr, NetLogSource());
  int client_rv = client_transport->Connect(client_callback.callback());

  EXPECT_THAT(server_callback.GetResult(server_rv), IsOk());
  EXPECT_THAT(client_callback.GetResult(client_rv), IsOk());

  // Set up an SSL server.
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> cert =
      ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(cert);
  bssl::UniquePtr<EVP_PKEY> pkey =
      key_util::LoadEVP_PKEYFromPEM(certs_dir.AppendASCII("ok_cert.pem"));
  ASSERT_TRUE(pkey);
  std::unique_ptr<crypto::RSAPrivateKey> key =
      crypto::RSAPrivateKey::CreateFromKey(pkey.get());
  ASSERT_TRUE(key);
  std::unique_ptr<SSLServerContext> server_context =
      CreateSSLServerContext(cert.get(), *key.get(), GetServerConfig());

  // Complete the SSL handshake on both sides.
  std::unique_ptr<SSLClientSocket> client(CreateSSLClientSocket(
      std::move(client_transport), HostPortPair::FromIPEndPoint(server_address),
      SSLConfig()));
  std::unique_ptr<SSLServerSocket> server(
      server_context->CreateSSLServerSocket(std::move(server_transport)));

  server_rv = server->Handshake(server_callback.callback());
  client_rv = client->Connect(client_callback.callback());

  EXPECT_THAT(server_callback.GetResult(server_rv), IsOk());
  EXPECT_THAT(client_callback.GetResult(client_rv), IsOk());

  // Write a single record on the server.
  auto write_buf = base::MakeRefCounted<StringIOBuffer>("a");
  server_rv = server->Write(write_buf.get(), 1, server_callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS);

  // Read that record on the server, but with a much larger buffer than
  // necessary.
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(1024);
  client_rv =
      Read(client.get(), read_buf.get(), 1024, client_callback.callback());

  EXPECT_EQ(1, server_callback.GetResult(server_rv));
  EXPECT_EQ(1, WaitForReadCompletion(client.get(), read_buf.get(), 1024,
                                     &client_callback, client_rv));

  // At this point the client socket should be idle.
  EXPECT_TRUE(client->IsConnectedAndIdle());
}

// Test that certificate errors are properly reported when the underlying
// transport is itself a TLS connection, such as when tunneling over an HTTPS
// proxy. See https://crbug.com/959305.
TEST_F(SSLClientSocketTest, SSLOverSSLBadCertificate) {
  // Load a pair of certificates.
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<net::X509Certificate> ok_cert =
      ImportCertFromFile(certs_dir, "ok_cert.pem");
  ASSERT_TRUE(ok_cert);
  bssl::UniquePtr<EVP_PKEY> ok_pkey =
      key_util::LoadEVP_PKEYFromPEM(certs_dir.AppendASCII("ok_cert.pem"));
  ASSERT_TRUE(ok_pkey);

  scoped_refptr<net::X509Certificate> expired_cert =
      ImportCertFromFile(certs_dir, "expired_cert.pem");
  ASSERT_TRUE(expired_cert);
  bssl::UniquePtr<EVP_PKEY> expired_pkey =
      key_util::LoadEVP_PKEYFromPEM(certs_dir.AppendASCII("expired_cert.pem"));
  ASSERT_TRUE(expired_pkey);

  CertVerifyResult expired_result;
  expired_result.verified_cert = expired_cert;
  expired_result.cert_status = CERT_STATUS_DATE_INVALID;
  cert_verifier_->AddResultForCert(expired_cert, expired_result,
                                   ERR_CERT_DATE_INVALID);

  // Set up a TCP server.
  TCPServerSocket server_listener(nullptr, NetLogSource());
  ASSERT_THAT(server_listener.Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0),
                                     1, /*ipv6_only=*/std::nullopt),
              IsOk());
  IPEndPoint server_address;
  ASSERT_THAT(server_listener.GetLocalAddress(&server_address), IsOk());

  // Connect a TCP client and server socket.
  TestCompletionCallback server_callback;
  std::unique_ptr<StreamSocket> server_transport;
  int server_rv =
      server_listener.Accept(&server_transport, server_callback.callback());

  TestCompletionCallback client_callback;
  auto client_transport = std::make_unique<TCPClientSocket>(
      AddressList(server_address), nullptr, nullptr, nullptr, NetLogSource());
  int client_rv = client_transport->Connect(client_callback.callback());

  ASSERT_THAT(server_callback.GetResult(server_rv), IsOk());
  ASSERT_THAT(client_callback.GetResult(client_rv), IsOk());

  // Set up a pair of SSL servers.
  std::unique_ptr<crypto::RSAPrivateKey> ok_key =
      crypto::RSAPrivateKey::CreateFromKey(ok_pkey.get());
  ASSERT_TRUE(ok_key);
  std::unique_ptr<SSLServerContext> ok_server_context =
      CreateSSLServerContext(ok_cert.get(), *ok_key.get(), SSLServerConfig());

  std::unique_ptr<crypto::RSAPrivateKey> expired_key =
      crypto::RSAPrivateKey::CreateFromKey(expired_pkey.get());
  ASSERT_TRUE(expired_key);
  std::unique_ptr<SSLServerContext> expired_server_context =
      CreateSSLServerContext(expired_cert.get(), *expired_key.get(),
                             SSLServerConfig());

  // Complete the proxy SSL handshake with ok_cert.pem. This should succeed.
  std::unique_ptr<SSLClientSocket> client =
      CreateSSLClientSocket(std::move(client_transport),
                            HostPortPair("proxy.test", 443), SSLConfig());
  std::unique_ptr<SSLServerSocket> server =
      ok_server_context->CreateSSLServerSocket(std::move(server_transport));

  client_rv = client->Connect(client_callback.callback());
  server_rv = server->Handshake(server_callback.callback());
  ASSERT_THAT(client_callback.GetResult(client_rv), IsOk());
  ASSERT_THAT(server_callback.GetResult(server_rv), IsOk());

  // Run the tunneled SSL handshake on with expired_cert.pem. This should fail.
  client = CreateSSLClientSocket(std::move(client),
                                 HostPortPair("server.test", 443), SSLConfig());
  server = expired_server_context->CreateSSLServerSocket(std::move(server));

  client_rv = client->Connect(client_callback.callback());
  server_rv = server->Handshake(server_callback.callback());

  // The client should observe the bad certificate error.
  EXPECT_THAT(client_callback.GetResult(client_rv),
              IsError(ERR_CERT_DATE_INVALID));
  SSLInfo ssl_info;
  ASSERT_TRUE(client->GetSSLInfo(&ssl_info));
  EXPECT_EQ(ssl_info.cert_status, expired_result.cert_status);

  // TODO(crbug.com/41430308): The server sees
  // ERR_BAD_SSL_CLIENT_AUTH_CERT because its peer (the client) alerts it with
  // bad_certificate. The alert-mapping code assumes it is running on a client,
  // so it translates bad_certificate to ERR_BAD_SSL_CLIENT_AUTH_CERT, which
  // shouldn't be the error for a bad server certificate.
  EXPECT_THAT(server_callback.GetResult(server_rv),
              IsError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
}

TEST_F(SSLClientSocketTest, Tag) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  auto transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, NetLog::Get(), NetLogSource());

  auto tagging_sock =
      std::make_unique<MockTaggingStreamSocket>(std::move(transport));
  auto* tagging_sock_ptr = tagging_sock.get();

  // |sock| takes ownership of |tagging_sock|, but keep a
  // non-owning pointer to it.
  std::unique_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      std::move(tagging_sock), host_port_pair(), SSLConfig()));

  EXPECT_EQ(tagging_sock_ptr->tag(), SocketTag());
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag(0x12345678, 0x87654321);
  sock->ApplySocketTag(tag);
  EXPECT_EQ(tagging_sock_ptr->tag(), tag);
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(SSLClientSocketTest, ECH) {
  SSLServerConfig server_config;
  SSLConfig client_config;
  server_config.ech_keys = MakeTestEchKeys(
      "public.example", /*max_name_len=*/64, &client_config.ech_config_list);
  ASSERT_TRUE(server_config.ech_keys);

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Connecting with the client should use ECH.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());
  SSLInfo ssl_info;
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, ssl_info.handshake_type);
  EXPECT_TRUE(ssl_info.encrypted_client_hello);

  // TLS 1.3 causes the ticket to arrive later. Use the socket to ensure we have
  // a ticket. This also populates the SSLInfo from the server.
  EXPECT_THAT(MakeHTTPRequest(sock_.get(), "/ssl-info"), IsOk());
  std::optional<SSLInfo> server_ssl_info = LastSSLInfoFromServer();
  ASSERT_TRUE(server_ssl_info);
  EXPECT_TRUE(server_ssl_info->encrypted_client_hello);

  // Reconnect. ECH should not interfere with resumption.
  sock_.reset();
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);
  EXPECT_TRUE(ssl_info.encrypted_client_hello);

  // Check SSLInfo from the server.
  EXPECT_THAT(MakeHTTPRequest(sock_.get(), "/ssl-info"), IsOk());
  server_ssl_info = LastSSLInfoFromServer();
  ASSERT_TRUE(server_ssl_info);
  EXPECT_TRUE(server_ssl_info->encrypted_client_hello);

  // Connecting without ECH should not report ECH was used.
  client_config.ech_config_list.clear();
  sock_.reset();
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.encrypted_client_hello);

  // Check SSLInfo from the server.
  EXPECT_THAT(MakeHTTPRequest(sock_.get(), "/ssl-info"), IsOk());
  server_ssl_info = LastSSLInfoFromServer();
  ASSERT_TRUE(server_ssl_info);
  EXPECT_FALSE(server_ssl_info->encrypted_client_hello);
}

// Test that, on key mismatch, the public name can be used to authenticate
// replacement keys.
TEST_F(SSLClientSocketTest, ECHWrongKeys) {
  static const char kPublicName[] = "public.example";
  std::vector<uint8_t> ech_config_list1, ech_config_list2;
  bssl::UniquePtr<SSL_ECH_KEYS> keys1 =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list1);
  ASSERT_TRUE(keys1);
  bssl::UniquePtr<SSL_ECH_KEYS> keys2 =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list2);
  ASSERT_TRUE(keys2);

  // Configure the client and server with different keys.
  SSLServerConfig server_config;
  server_config.ech_keys = std::move(keys1);
  SSLConfig client_config;
  client_config.ech_config_list = std::move(ech_config_list2);

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Verify the fallback handshake verifies the certificate against the public
  // name.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();
  CertVerifyResult verify_result;
  verify_result.verified_cert = server_cert;
  cert_verifier_->AddResultForCertAndHost(server_cert, kPublicName,
                                          verify_result, OK);

  // Connecting with the client should report ECH was not negotiated.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_ECH_NOT_NEGOTIATED));

  // The server's keys are available as retry keys.
  EXPECT_EQ(ech_config_list1, sock_->GetECHRetryConfigs());
}

// Test that, if the server does not support ECH, it can securely report this
// via the public name. This allows recovery if the server needed to
// rollback ECH support.
TEST_F(SSLClientSocketTest, ECHSecurelyDisabled) {
  static const char kPublicName[] = "public.example";
  std::vector<uint8_t> ech_config_list;
  bssl::UniquePtr<SSL_ECH_KEYS> keys =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list);
  ASSERT_TRUE(keys);

  // The server does not have keys configured.
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  // However it can authenticate for kPublicName.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();
  CertVerifyResult verify_result;
  verify_result.verified_cert = server_cert;
  cert_verifier_->AddResultForCertAndHost(server_cert, kPublicName,
                                          verify_result, OK);

  // Connecting with the client should report ECH was not negotiated.
  SSLConfig client_config;
  client_config.ech_config_list = std::move(ech_config_list);
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_ECH_NOT_NEGOTIATED));

  // The retry config is empty, meaning the server has securely reported that
  // ECH is disabled
  EXPECT_TRUE(sock_->GetECHRetryConfigs().empty());
}

// The same as the above, but testing that it also works in TLS 1.2, which
// otherwise does not support ECH.
TEST_F(SSLClientSocketTest, ECHSecurelyDisabledTLS12) {
  static const char kPublicName[] = "public.example";
  std::vector<uint8_t> ech_config_list;
  bssl::UniquePtr<SSL_ECH_KEYS> keys =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list);
  ASSERT_TRUE(keys);

  // The server does not have keys configured.
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // However it can authenticate for kPublicName.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);
  scoped_refptr<X509Certificate> server_cert =
      embedded_test_server()->GetCertificate();
  CertVerifyResult verify_result;
  verify_result.verified_cert = server_cert;
  cert_verifier_->AddResultForCertAndHost(server_cert, kPublicName,
                                          verify_result, OK);

  // Connecting with the client should report ECH was not negotiated.
  SSLConfig client_config;
  client_config.ech_config_list = std::move(ech_config_list);
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_ECH_NOT_NEGOTIATED));

  // The retry config is empty, meaning the server has securely reported that
  // ECH is disabled
  EXPECT_TRUE(sock_->GetECHRetryConfigs().empty());
}

// Test that the ECH fallback handshake rejects bad certificates.
TEST_F(SSLClientSocketTest, ECHFallbackBadCert) {
  static const char kPublicName[] = "public.example";
  std::vector<uint8_t> ech_config_list1, ech_config_list2;
  bssl::UniquePtr<SSL_ECH_KEYS> keys1 =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list1);
  ASSERT_TRUE(keys1);
  bssl::UniquePtr<SSL_ECH_KEYS> keys2 =
      MakeTestEchKeys(kPublicName, /*max_name_len=*/64, &ech_config_list2);
  ASSERT_TRUE(keys2);

  // Configure the client and server with different keys.
  SSLServerConfig server_config;
  server_config.ech_keys = std::move(keys1);
  SSLConfig client_config;
  client_config.ech_config_list = std::move(ech_config_list2);

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Configure the client to reject the certificate for the public name (or any
  // other name).
  cert_verifier_->set_default_result(ERR_CERT_INVALID);

  // Connecting with the client will fail with a fatal error.
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_ECH_FALLBACK_CERTIFICATE_INVALID));
}

TEST_F(SSLClientSocketTest, InvalidECHConfigList) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  // If the ECHConfigList cannot be parsed at all, report an error to the
  // caller.
  SSLConfig client_config;
  client_config.ech_config_list = {0x00};
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsError(ERR_INVALID_ECH_CONFIG_LIST));
}

// Test that, if no ECHConfigList is available, the client sends ECH GREASE.
TEST_F(SSLClientSocketTest, ECHGreaseEnabled) {
  // Configure the server to expect an ECH extension.
  bool ran_callback = false;
  SSLServerConfig server_config;
  server_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        const uint8_t* data;
        size_t len;
        EXPECT_TRUE(SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_encrypted_client_hello, &data, &len));
        ran_callback = true;
        return true;
      });
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
}

// Test that, if ECH is disabled, the client does not send ECH GREASE.
TEST_F(SSLClientSocketTest, ECHGreaseDisabled) {
  SSLContextConfig context_config;
  context_config.ech_enabled = false;
  ssl_config_service_->UpdateSSLConfigAndNotify(context_config);

  // Configure the server not to expect an ECH extension.
  bool ran_callback = false;
  SSLServerConfig server_config;
  server_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        const uint8_t* data;
        size_t len;
        EXPECT_FALSE(SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_encrypted_client_hello, &data, &len));
        ran_callback = true;
        return true;
      });
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
  EXPECT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
}

struct SSLHandshakeDetailsParams {
  bool alpn;
  bool early_data;
  uint16_t version;
  SSLHandshakeDetails expected_initial;
  SSLHandshakeDetails expected_resume;
};

const SSLHandshakeDetailsParams kSSLHandshakeDetailsParams[] = {
    // TLS 1.2 does False Start if ALPN is enabled.
    {false /* no ALPN */, false /* no early data */,
     SSL_PROTOCOL_VERSION_TLS1_2, SSLHandshakeDetails::kTLS12Full,
     SSLHandshakeDetails::kTLS12Resume},
    {true /* ALPN */, false /* no early data */, SSL_PROTOCOL_VERSION_TLS1_2,
     SSLHandshakeDetails::kTLS12FalseStart, SSLHandshakeDetails::kTLS12Resume},

    // TLS 1.3 supports full handshakes, resumption, and 0-RTT.
    {false /* no ALPN */, false /* no early data */,
     SSL_PROTOCOL_VERSION_TLS1_3, SSLHandshakeDetails::kTLS13Full,
     SSLHandshakeDetails::kTLS13Resume},
    {false /* no ALPN */, true /* early data */, SSL_PROTOCOL_VERSION_TLS1_3,
     SSLHandshakeDetails::kTLS13Full, SSLHandshakeDetails::kTLS13Early},
};

class SSLHandshakeDetailsTest
    : public SSLClientSocketTest,
      public ::testing::WithParamInterface<SSLHandshakeDetailsParams> {};

INSTANTIATE_TEST_SUITE_P(All,
                         SSLHandshakeDetailsTest,
                         ValuesIn(kSSLHandshakeDetailsParams));

TEST_P(SSLHandshakeDetailsTest, Metrics) {
  // Enable all test features in the server.
  SSLServerConfig server_config;
  server_config.early_data_enabled = true;
  server_config.alpn_protos = {kProtoHTTP11};
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLContextConfig client_context_config;
  client_context_config.version_min = GetParam().version;
  client_context_config.version_max = GetParam().version;
  ssl_config_service_->UpdateSSLConfigAndNotify(client_context_config);

  SSLConfig client_config;
  client_config.version_min_override = GetParam().version;
  client_config.version_max_override = GetParam().version;
  client_config.early_data_enabled = GetParam().early_data;
  if (GetParam().alpn) {
    client_config.alpn_protos = {kProtoHTTP11};
  }

  SSLVersion version;
  switch (GetParam().version) {
    case SSL_PROTOCOL_VERSION_TLS1_2:
      version = SSL_CONNECTION_VERSION_TLS1_2;
      break;
    case SSL_PROTOCOL_VERSION_TLS1_3:
      version = SSL_CONNECTION_VERSION_TLS1_3;
      break;
    default:
      FAIL() << GetParam().version;
  }

  // Make the initial connection.
  {
    base::HistogramTester histograms;
    int rv;
    ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
    EXPECT_THAT(rv, IsOk());

    // Sanity-check the socket matches the test parameters.
    SSLInfo info;
    ASSERT_TRUE(sock_->GetSSLInfo(&info));
    EXPECT_EQ(version, SSLConnectionStatusToVersion(info.connection_status));
    EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, info.handshake_type);

    histograms.ExpectUniqueSample("Net.SSLHandshakeDetails",
                                  GetParam().expected_initial, 1);

    // TLS 1.2 with False Start and TLS 1.3 cause the ticket to arrive later, so
    // use the socket to ensure the session ticket has been picked up.
    EXPECT_THAT(MakeHTTPRequest(sock_.get()), IsOk());
  }

  // Make a resumption connection.
  {
    base::HistogramTester histograms;
    int rv;
    ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
    EXPECT_THAT(rv, IsOk());

    // Sanity-check the socket matches the test parameters.
    SSLInfo info;
    ASSERT_TRUE(sock_->GetSSLInfo(&info));
    EXPECT_EQ(version, SSLConnectionStatusToVersion(info.connection_status));
    EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, info.handshake_type);

    histograms.ExpectUniqueSample("Net.SSLHandshakeDetails",
                                  GetParam().expected_resume, 1);
  }
}

TEST_F(SSLClientSocketZeroRTTTest, EarlyDataReasonNewSession) {
  const char kReasonHistogram[] = "Net.SSLHandshakeEarlyDataReason";

  ASSERT_TRUE(StartServer());
  base::HistogramTester histograms;
  ASSERT_TRUE(RunInitialConnection());
  histograms.ExpectUniqueSample(kReasonHistogram,
                                ssl_early_data_no_session_offered, 1);
}

// Test 0-RTT logging when the server declines to resume a connection.
TEST_F(SSLClientSocketZeroRTTTest, EarlyDataReasonNoResume) {
  const char kReasonHistogram[] = "Net.SSLHandshakeEarlyDataReason";

  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  SSLServerConfig server_config;
  server_config.early_data_enabled = false;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;

  SetServerConfig(server_config);

  base::HistogramTester histograms;

  // 0-RTT Connection
  FakeBlockingStreamSocket* socket = MakeClient(true);
  socket->BlockReadResult();
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));
  socket->UnblockReadResult();

  // Expect early data to be rejected.
  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int rv = ReadAndWait(buf.get(), 4096);
  EXPECT_EQ(ERR_EARLY_DATA_REJECTED, rv);

  // The histogram may be record asynchronously.
  base::RunLoop().RunUntilIdle();
  histograms.ExpectUniqueSample(kReasonHistogram,
                                ssl_early_data_session_not_resumed, 1);
}

// Test 0-RTT logging in the standard ConfirmHandshake-after-acceptance case.
TEST_F(SSLClientSocketZeroRTTTest, EarlyDataReasonZeroRTT) {
  const char kReasonHistogram[] = "Net.SSLHandshakeEarlyDataReason";

  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // 0-RTT Connection
  base::HistogramTester histograms;
  MakeClient(true);
  ASSERT_THAT(Connect(), IsOk());
  TestCompletionCallback callback;
  ASSERT_THAT(
      callback.GetResult(ssl_socket()->ConfirmHandshake(callback.callback())),
      IsOk());

  base::RunLoop().RunUntilIdle();

  histograms.ExpectUniqueSample(kReasonHistogram, ssl_early_data_accepted, 1);
}

// Check that we're correctly logging 0-rtt success when the handshake
// concludes during a Read.
TEST_F(SSLClientSocketZeroRTTTest, EarlyDataReasonReadServerHello) {
  const char kReasonHistogram[] = "Net.SSLHandshakeEarlyDataReason";
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(RunInitialConnection());

  // 0-RTT Connection
  base::HistogramTester histograms;
  MakeClient(true);
  ASSERT_THAT(Connect(), IsOk());
  constexpr std::string_view kRequest = "GET /zerortt HTTP/1.0\r\n\r\n";
  EXPECT_EQ(static_cast<int>(kRequest.size()), WriteAndWait(kRequest));

  auto buf = base::MakeRefCounted<IOBufferWithSize>(4096);
  int size = ReadAndWait(buf.get(), 4096);
  EXPECT_GT(size, 0);
  EXPECT_EQ('1', buf->data()[size - 1]);

  // 0-RTT metrics are logged on a PostTask, so if Read returns synchronously,
  // it is possible the metrics haven't been picked up yet.
  base::RunLoop().RunUntilIdle();

  SSLInfo ssl_info;
  ASSERT_TRUE(GetSSLInfo(&ssl_info));
  EXPECT_EQ(SSLInfo::HANDSHAKE_RESUME, ssl_info.handshake_type);

  histograms.ExpectUniqueSample(kReasonHistogram, ssl_early_data_accepted, 1);
}

TEST_F(SSLClientSocketTest, VersionMaxOverride) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_3;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Connecting normally uses the global configuration.
  SSLConfig config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
  EXPECT_THAT(rv, IsOk());
  SSLInfo info;
  ASSERT_TRUE(sock_->GetSSLInfo(&info));
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
            SSLConnectionStatusToVersion(info.connection_status));

  // Individual sockets may override the maximum version.
  config.version_max_override = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
  EXPECT_THAT(rv, IsOk());
  ASSERT_TRUE(sock_->GetSSLInfo(&info));
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_2,
            SSLConnectionStatusToVersion(info.connection_status));
}

TEST_F(SSLClientSocketTest, VersionMinOverride) {
  SSLServerConfig server_config;
  server_config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // Connecting normally uses the global configuration.
  SSLConfig config;
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
  EXPECT_THAT(rv, IsOk());
  SSLInfo info;
  ASSERT_TRUE(sock_->GetSSLInfo(&info));
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_2,
            SSLConnectionStatusToVersion(info.connection_status));

  // Individual sockets may also override the minimum version.
  config.version_min_override = SSL_PROTOCOL_VERSION_TLS1_3;
  config.version_max_override = SSL_PROTOCOL_VERSION_TLS1_3;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(config, &rv));
  EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

// Basic test of CancelReadIfReady works.
TEST_F(SSLClientSocketTest, CancelReadIfReady) {
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, SSLServerConfig()));

  // Connect with a FakeBlockingStreamSocket.
  auto real_transport = std::make_unique<TCPClientSocket>(
      addr(), nullptr, nullptr, nullptr, NetLogSource());
  auto transport =
      std::make_unique<FakeBlockingStreamSocket>(std::move(real_transport));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  TestCompletionCallback callback;
  ASSERT_THAT(callback.GetResult(transport->Connect(callback.callback())),
              IsOk());

  // Complete the handshake. Disable the post-handshake peek so that, after the
  // handshake, there are no pending reads on the transport.
  SSLConfig config;
  config.disable_post_handshake_peek_for_testing = true;
  auto sock =
      CreateSSLClientSocket(std::move(transport), host_port_pair(), config);
  ASSERT_THAT(callback.GetResult(sock->Connect(callback.callback())), IsOk());

  // Block the socket and wait for some data to arrive from the server.
  raw_transport->BlockReadResult();
  auto write_buf =
      base::MakeRefCounted<StringIOBuffer>("GET / HTTP/1.0\r\n\r\n");
  ASSERT_EQ(callback.GetResult(sock->Write(write_buf.get(), write_buf->size(),
                                           callback.callback(),
                                           TRAFFIC_ANNOTATION_FOR_TESTS)),
            write_buf->size());

  // ReadIfReady() should not read anything because the socket is blocked.
  bool callback_called = false;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(100);
  int rv = sock->ReadIfReady(
      read_buf.get(), 100,
      base::BindLambdaForTesting([&](int rv) { callback_called = true; }));
  ASSERT_THAT(rv, IsError(ERR_IO_PENDING));

  // Cancel ReadIfReady() and unblock the socket.
  ASSERT_THAT(sock->CancelReadIfReady(), IsOk());
  raw_transport->WaitForReadResult();
  raw_transport->UnblockReadResult();
  base::RunLoop().RunUntilIdle();

  // Although data is now available, the callback should not have been called.
  EXPECT_FALSE(callback_called);

  // Future reads on the socket should still work. The data should be
  // synchronously available.
  EXPECT_GT(
      callback.GetResult(sock->Read(read_buf.get(), 100, callback.callback())),
      0);
}

// Test that the server_name extension (SNI) is sent on DNS names, and not IP
// literals.
TEST_F(SSLClientSocketTest, ServerName) {
  std::optional<std::string> got_server_name;
  bool ran_callback = false;
  auto reset_callback_state = [&] {
    got_server_name = std::nullopt;
    ran_callback = false;
  };

  // Start a server which records the server name.
  SSLServerConfig server_config;
  server_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        const char* server_name =
            SSL_get_servername(client_hello->ssl, TLSEXT_NAMETYPE_host_name);
        if (server_name) {
          got_server_name = server_name;
        } else {
          got_server_name = std::nullopt;
        }
        ran_callback = true;
        return true;
      });
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  // The client should send the server_name extension for DNS names.
  uint16_t port = host_port_pair().port();
  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(
      SSLConfig(), HostPortPair("example.com", port), &rv));
  ASSERT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(got_server_name, "example.com");

  // The client should not send the server_name extension for IPv4 and IPv6
  // literals. See https://crbug.com/500981.
  reset_callback_state();
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(
      SSLConfig(), HostPortPair("1.2.3.4", port), &rv));
  ASSERT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(got_server_name, std::nullopt);

  reset_callback_state();
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(
      SSLConfig(), HostPortPair("::1", port), &rv));
  ASSERT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(got_server_name, std::nullopt);

  reset_callback_state();
  ASSERT_TRUE(CreateAndConnectSSLClientSocketWithHost(
      SSLConfig(), HostPortPair("2001:db8::42", port), &rv));
  ASSERT_THAT(rv, IsOk());
  EXPECT_TRUE(ran_callback);
  EXPECT_EQ(got_server_name, std::nullopt);
}

TEST_F(SSLClientSocketTest, PostQuantumKeyExchange) {
  for (bool server_mlkem : {false, true}) {
    SCOPED_TRACE(server_mlkem);

    SSLServerConfig server_config;
    server_config.curves_for_testing.push_back(
        server_mlkem ? NID_X25519MLKEM768 : NID_X25519Kyber768Draft00);
    ASSERT_TRUE(
        StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

    for (bool client_mlkem : {false, true}) {
      SCOPED_TRACE(client_mlkem);

      base::test::ScopedFeatureList feature_list;
      feature_list.InitWithFeatureState(features::kUseMLKEM, client_mlkem);

      for (bool enabled : {false, true}) {
        SCOPED_TRACE(enabled);

        SSLContextConfig config;
        config.post_quantum_override = enabled;
        ssl_config_service_->UpdateSSLConfigAndNotify(config);
        int rv;
        ASSERT_TRUE(CreateAndConnectSSLClientSocket(SSLConfig(), &rv));
        if (enabled && server_mlkem == client_mlkem) {
          EXPECT_THAT(rv, IsOk());
        } else {
          EXPECT_THAT(rv, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
        }
      }
    }
  }
}

class SSLClientSocketAlpsTest
    : public SSLClientSocketTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  SSLClientSocketAlpsTest() {
    if (client_use_new_alps()) {
      feature_list_.InitAndEnableFeature(features::kUseNewAlpsCodepointHttp2);
    } else {
      feature_list_.InitAndDisableFeature(features::kUseNewAlpsCodepointHttp2);
    }
  }

  bool client_alps_enabled() const { return std::get<0>(GetParam()); }
  bool server_alps_enabled() const { return std::get<1>(GetParam()); }
  bool client_use_new_alps() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SSLClientSocketAlpsTest,
                         Combine(Bool(), Bool(), Bool()));

TEST_P(SSLClientSocketAlpsTest, Alps) {
  const std::string server_data = "server sends some test data";
  const std::string client_data = "client also sends some data";

  SSLServerConfig server_config;
  server_config.alpn_protos = {kProtoHTTP2};
  if (server_alps_enabled()) {
    server_config.application_settings[kProtoHTTP2] =
        std::vector<uint8_t>(server_data.begin(), server_data.end());
  }
  // Configure the server to support whichever ALPS codepoint the client sent.
  server_config.client_hello_callback_for_testing =
      base::BindRepeating([](const SSL_CLIENT_HELLO* client_hello) {
        const uint8_t* unused_extension_bytes;
        size_t unused_extension_len;
        int use_alps_new_codepoint = SSL_early_callback_ctx_extension_get(
            client_hello, TLSEXT_TYPE_application_settings,
            &unused_extension_bytes, &unused_extension_len);
        SSL_set_alps_use_new_codepoint(client_hello->ssl,
                                       use_alps_new_codepoint);
        return true;
      });

  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  SSLConfig client_config;
  client_config.alpn_protos = {kProtoHTTP2};
  if (client_alps_enabled()) {
    client_config.application_settings[kProtoHTTP2] =
        std::vector<uint8_t>(client_data.begin(), client_data.end());
  }

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());

  SSLInfo info;
  ASSERT_TRUE(sock_->GetSSLInfo(&info));
  EXPECT_EQ(SSL_CONNECTION_VERSION_TLS1_3,
            SSLConnectionStatusToVersion(info.connection_status));
  EXPECT_EQ(SSLInfo::HANDSHAKE_FULL, info.handshake_type);

  EXPECT_EQ(kProtoHTTP2, sock_->GetNegotiatedProtocol());

  // ALPS is negotiated only if ALPS is enabled both on client and server.
  const auto alps_data_received_by_client = sock_->GetPeerApplicationSettings();

  if (client_alps_enabled() && server_alps_enabled()) {
    ASSERT_TRUE(alps_data_received_by_client.has_value());
    EXPECT_EQ(server_data, alps_data_received_by_client.value());
  } else {
    EXPECT_FALSE(alps_data_received_by_client.has_value());
  }
}

// Test that unused protocols in `application_settings` are ignored.
TEST_P(SSLClientSocketAlpsTest, UnusedProtocols) {
  if (!client_alps_enabled() || !server_alps_enabled()) {
    return;
  }

  SSLConfig client_config;
  client_config.alpn_protos = {kProtoHTTP2};
  client_config.application_settings[kProtoHTTP2] = {};
  client_config.application_settings[kProtoHTTP11] = {};

  // Configure the server to check the ClientHello is as we expected.
  SSLServerConfig server_config;
  server_config.client_hello_callback_for_testing =
      base::BindLambdaForTesting([&](const SSL_CLIENT_HELLO* client_hello) {
        const uint8_t* data;
        size_t len;
        if (!SSL_early_callback_ctx_extension_get(
                client_hello,
                client_use_new_alps() ? TLSEXT_TYPE_application_settings
                                      : TLSEXT_TYPE_application_settings_old,
                &data, &len)) {
          return false;
        }
        // The client should only have sent "h2" in the extension. Note there
        // are two length prefixes. A two-byte length prefix (0x0003) followed
        // by a one-byte length prefix (0x02). See
        // https://www.ietf.org/archive/id/draft-vvv-tls-alps-01.html#section-4
        EXPECT_EQ(std::vector<uint8_t>(data, data + len),
                  std::vector<uint8_t>({0x00, 0x03, 0x02, 'h', '2'}));
        return true;
      });
  ASSERT_TRUE(
      StartEmbeddedTestServer(EmbeddedTestServer::CERT_OK, server_config));

  int rv;
  ASSERT_TRUE(CreateAndConnectSSLClientSocket(client_config, &rv));
  EXPECT_THAT(rv, IsOk());
}

}  // namespace net
