// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This test suite uses SSLClientSocket to test the implementation of
// SSLServerSocket. In order to establish connections between the sockets
// we need two additional classes:
// 1. FakeSocket
//    Connects SSL socket to FakeDataChannel. This class is just a stub.
//
// 2. FakeDataChannel
//    Implements the actual exchange of data between two FakeSockets.
//
// Implementations of these two classes are included in this file.

#include "net/socket/ssl_server_socket.h"

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "crypto/rsa_private_key.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/mock_client_cert_verifier.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
const char kClientCertFileName[] = "client_1.pem";
const char kClientPrivateKeyFileName[] = "client_1.pk8";
const char kWrongClientCertFileName[] = "client_2.pem";
const char kWrongClientPrivateKeyFileName[] = "client_2.pk8";
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

const uint16_t kEcdheCiphers[] = {
    0xc007,  // ECDHE_ECDSA_WITH_RC4_128_SHA
    0xc009,  // ECDHE_ECDSA_WITH_AES_128_CBC_SHA
    0xc00a,  // ECDHE_ECDSA_WITH_AES_256_CBC_SHA
    0xc011,  // ECDHE_RSA_WITH_RC4_128_SHA
    0xc013,  // ECDHE_RSA_WITH_AES_128_CBC_SHA
    0xc014,  // ECDHE_RSA_WITH_AES_256_CBC_SHA
    0xc02b,  // ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
    0xc02c,  // ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
    0xc02f,  // ECDHE_RSA_WITH_AES_128_GCM_SHA256
    0xc030,  // ECDHE_RSA_WITH_AES_256_GCM_SHA384
    0xcca8,  // ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
    0xcca9,  // ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
};

class FakeDataChannel {
 public:
  FakeDataChannel() = default;

  FakeDataChannel(const FakeDataChannel&) = delete;
  FakeDataChannel& operator=(const FakeDataChannel&) = delete;

  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback) {
    DCHECK(read_callback_.is_null());
    DCHECK(!read_buf_.get());
    if (closed_)
      return 0;
    if (data_.empty()) {
      read_callback_ = std::move(callback);
      read_buf_ = buf;
      read_buf_len_ = buf_len;
      return ERR_IO_PENDING;
    }
    return PropagateData(buf, buf_len);
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) {
    DCHECK(write_callback_.is_null());
    if (closed_) {
      if (write_called_after_close_)
        return ERR_CONNECTION_RESET;
      write_called_after_close_ = true;
      write_callback_ = std::move(callback);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&FakeDataChannel::DoWriteCallback,
                                    weak_factory_.GetWeakPtr()));
      return ERR_IO_PENDING;
    }
    // This function returns synchronously, so make a copy of the buffer.
    data_.push(base::MakeRefCounted<DrainableIOBuffer>(
        base::MakeRefCounted<StringIOBuffer>(std::string(buf->data(), buf_len)),
        buf_len));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeDataChannel::DoReadCallback,
                                  weak_factory_.GetWeakPtr()));
    return buf_len;
  }

  // Closes the FakeDataChannel. After Close() is called, Read() returns 0,
  // indicating EOF, and Write() fails with ERR_CONNECTION_RESET. Note that
  // after the FakeDataChannel is closed, the first Write() call completes
  // asynchronously, which is necessary to reproduce bug 127822.
  void Close() {
    closed_ = true;
    if (!read_callback_.is_null()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&FakeDataChannel::DoReadCallback,
                                    weak_factory_.GetWeakPtr()));
    }
  }

 private:
  void DoReadCallback() {
    if (read_callback_.is_null())
      return;

    if (closed_) {
      std::move(read_callback_).Run(ERR_CONNECTION_CLOSED);
      return;
    }

    if (data_.empty())
      return;

    int copied = PropagateData(read_buf_, read_buf_len_);
    read_buf_ = nullptr;
    read_buf_len_ = 0;
    std::move(read_callback_).Run(copied);
  }

  void DoWriteCallback() {
    if (write_callback_.is_null())
      return;

    std::move(write_callback_).Run(ERR_CONNECTION_RESET);
  }

  int PropagateData(scoped_refptr<IOBuffer> read_buf, int read_buf_len) {
    scoped_refptr<DrainableIOBuffer> buf = data_.front();
    int copied = std::min(buf->BytesRemaining(), read_buf_len);
    memcpy(read_buf->data(), buf->data(), copied);
    buf->DidConsume(copied);

    if (!buf->BytesRemaining())
      data_.pop();
    return copied;
  }

  CompletionOnceCallback read_callback_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_ = 0;

  CompletionOnceCallback write_callback_;

  base::queue<scoped_refptr<DrainableIOBuffer>> data_;

  // True if Close() has been called.
  bool closed_ = false;

  // Controls the completion of Write() after the FakeDataChannel is closed.
  // After the FakeDataChannel is closed, the first Write() call completes
  // asynchronously.
  bool write_called_after_close_ = false;

  base::WeakPtrFactory<FakeDataChannel> weak_factory_{this};
};

class FakeSocket : public StreamSocket {
 public:
  FakeSocket(FakeDataChannel* incoming_channel,
             FakeDataChannel* outgoing_channel)
      : incoming_(incoming_channel), outgoing_(outgoing_channel) {}

  FakeSocket(const FakeSocket&) = delete;
  FakeSocket& operator=(const FakeSocket&) = delete;

  ~FakeSocket() override = default;

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    // Read random number of bytes.
    buf_len = rand() % buf_len + 1;
    return incoming_->Read(buf, buf_len, std::move(callback));
  }

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    // Write random number of bytes.
    buf_len = rand() % buf_len + 1;
    return outgoing_->Write(buf, buf_len, std::move(callback),
                            TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  int SetReceiveBufferSize(int32_t size) override { return OK; }

  int SetSendBufferSize(int32_t size) override { return OK; }

  int Connect(CompletionOnceCallback callback) override { return OK; }

  void Disconnect() override {
    incoming_->Close();
    outgoing_->Close();
  }

  bool IsConnected() const override { return true; }

  bool IsConnectedAndIdle() const override { return true; }

  int GetPeerAddress(IPEndPoint* address) const override {
    *address = IPEndPoint(IPAddress::IPv4AllZeros(), 0 /*port*/);
    return OK;
  }

  int GetLocalAddress(IPEndPoint* address) const override {
    *address = IPEndPoint(IPAddress::IPv4AllZeros(), 0 /*port*/);
    return OK;
  }

  const NetLogWithSource& NetLog() const override { return net_log_; }

  bool WasEverUsed() const override { return true; }

  NextProto GetNegotiatedProtocol() const override { return kProtoUnknown; }

  bool GetSSLInfo(SSLInfo* ssl_info) override { return false; }

  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }

  void ApplySocketTag(const SocketTag& tag) override {}

 private:
  NetLogWithSource net_log_;
  raw_ptr<FakeDataChannel> incoming_;
  raw_ptr<FakeDataChannel> outgoing_;
};

}  // namespace

// Verify the correctness of the test helper classes first.
TEST(FakeSocketTest, DataTransfer) {
  base::test::TaskEnvironment task_environment;

  // Establish channels between two sockets.
  FakeDataChannel channel_1;
  FakeDataChannel channel_2;
  FakeSocket client(&channel_1, &channel_2);
  FakeSocket server(&channel_2, &channel_1);

  const char kTestData[] = "testing123";
  const int kTestDataSize = strlen(kTestData);
  const int kReadBufSize = 1024;
  auto write_buf = base::MakeRefCounted<StringIOBuffer>(kTestData);
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kReadBufSize);

  // Write then read.
  int written =
      server.Write(write_buf.get(), kTestDataSize, CompletionOnceCallback(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  int read =
      client.Read(read_buf.get(), kReadBufSize, CompletionOnceCallback());
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));

  // Read then write.
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            server.Read(read_buf.get(), kReadBufSize, callback.callback()));

  written =
      client.Write(write_buf.get(), kTestDataSize, CompletionOnceCallback(),
                   TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_GT(written, 0);
  EXPECT_LE(written, kTestDataSize);

  read = callback.WaitForResult();
  EXPECT_GT(read, 0);
  EXPECT_LE(read, written);
  EXPECT_EQ(0, memcmp(kTestData, read_buf->data(), read));
}

class SSLServerSocketTest : public PlatformTest, public WithTaskEnvironment {
 public:
  SSLServerSocketTest()
      : ssl_config_service_(
            std::make_unique<TestSSLConfigService>(SSLContextConfig())),
        cert_verifier_(std::make_unique<MockCertVerifier>()),
        client_cert_verifier_(std::make_unique<MockClientCertVerifier>()),
        transport_security_state_(std::make_unique<TransportSecurityState>()),
        ssl_client_session_cache_(std::make_unique<SSLClientSessionCache>(
            SSLClientSessionCache::Config())) {}

  void SetUp() override {
    PlatformTest::SetUp();

    cert_verifier_->set_default_result(ERR_CERT_AUTHORITY_INVALID);
    client_cert_verifier_->set_default_result(ERR_CERT_AUTHORITY_INVALID);

    server_cert_ =
        ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
    ASSERT_TRUE(server_cert_);
    server_private_key_ = ReadTestKey("unittest.key.bin");
    ASSERT_TRUE(server_private_key_);

    std::unique_ptr<crypto::RSAPrivateKey> key =
        ReadTestKey("unittest.key.bin");
    ASSERT_TRUE(key);
    server_ssl_private_key_ = WrapOpenSSLPrivateKey(bssl::UpRef(key->key()));

    // Certificate provided by the host doesn't need authority.
    client_ssl_config_.allowed_bad_certs.emplace_back(
        server_cert_, CERT_STATUS_AUTHORITY_INVALID);

    client_context_ = std::make_unique<SSLClientContext>(
        ssl_config_service_.get(), cert_verifier_.get(),
        transport_security_state_.get(), ssl_client_session_cache_.get(),
        nullptr);
  }

 protected:
  void CreateContext() {
    client_socket_.reset();
    server_socket_.reset();
    channel_1_.reset();
    channel_2_.reset();
    server_context_ = CreateSSLServerContext(
        server_cert_.get(), *server_private_key_, server_ssl_config_);
  }

  void CreateContextSSLPrivateKey() {
    client_socket_.reset();
    server_socket_.reset();
    channel_1_.reset();
    channel_2_.reset();
    server_context_.reset();
    server_context_ = CreateSSLServerContext(
        server_cert_.get(), server_ssl_private_key_, server_ssl_config_);
  }

  static HostPortPair GetHostAndPort() { return HostPortPair("unittest", 0); }

  void CreateSockets() {
    client_socket_.reset();
    server_socket_.reset();
    channel_1_ = std::make_unique<FakeDataChannel>();
    channel_2_ = std::make_unique<FakeDataChannel>();
    std::unique_ptr<StreamSocket> client_connection =
        std::make_unique<FakeSocket>(channel_1_.get(), channel_2_.get());
    std::unique_ptr<StreamSocket> server_socket =
        std::make_unique<FakeSocket>(channel_2_.get(), channel_1_.get());

    client_socket_ = client_context_->CreateSSLClientSocket(
        std::move(client_connection), GetHostAndPort(), client_ssl_config_);
    ASSERT_TRUE(client_socket_);

    server_socket_ =
        server_context_->CreateSSLServerSocket(std::move(server_socket));
    ASSERT_TRUE(server_socket_);
  }

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
  void ConfigureClientCertsForClient(const char* cert_file_name,
                                     const char* private_key_file_name) {
    scoped_refptr<X509Certificate> client_cert =
        ImportCertFromFile(GetTestCertsDirectory(), cert_file_name);
    ASSERT_TRUE(client_cert);

    std::unique_ptr<crypto::RSAPrivateKey> key =
        ReadTestKey(private_key_file_name);
    ASSERT_TRUE(key);

    client_context_->SetClientCertificate(
        GetHostAndPort(), std::move(client_cert),
        WrapOpenSSLPrivateKey(bssl::UpRef(key->key())));
  }

  void ConfigureClientCertsForServer() {
    server_ssl_config_.client_cert_type =
        SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;

    // "CN=B CA" - DER encoded DN of the issuer of client_1.pem
    static const uint8_t kClientCertCAName[] = {
        0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55,
        0x04, 0x03, 0x0c, 0x04, 0x42, 0x20, 0x43, 0x41};
    server_ssl_config_.cert_authorities.emplace_back(
        std::begin(kClientCertCAName), std::end(kClientCertCAName));

    scoped_refptr<X509Certificate> expected_client_cert(
        ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName));
    ASSERT_TRUE(expected_client_cert);

    client_cert_verifier_->AddResultForCert(expected_client_cert.get(), OK);

    server_ssl_config_.client_cert_verifier = client_cert_verifier_.get();
  }
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

  std::unique_ptr<crypto::RSAPrivateKey> ReadTestKey(std::string_view name) {
    base::FilePath certs_dir(GetTestCertsDirectory());
    base::FilePath key_path = certs_dir.AppendASCII(name);
    std::string key_string;
    if (!base::ReadFileToString(key_path, &key_string))
      return nullptr;
    std::vector<uint8_t> key_vector(
        reinterpret_cast<const uint8_t*>(key_string.data()),
        reinterpret_cast<const uint8_t*>(key_string.data() +
                                         key_string.length()));
    std::unique_ptr<crypto::RSAPrivateKey> key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(key_vector));
    return key;
  }

  void PumpServerToClient() {
    const int kReadBufSize = 1024;
    scoped_refptr<StringIOBuffer> write_buf =
        base::MakeRefCounted<StringIOBuffer>("testing123");
    scoped_refptr<DrainableIOBuffer> read_buf =
        base::MakeRefCounted<DrainableIOBuffer>(
            base::MakeRefCounted<IOBufferWithSize>(kReadBufSize), kReadBufSize);
    TestCompletionCallback write_callback;
    TestCompletionCallback read_callback;
    int server_ret = server_socket_->Write(write_buf.get(), write_buf->size(),
                                           write_callback.callback(),
                                           TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);
    int client_ret = client_socket_->Read(
        read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
    EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

    server_ret = write_callback.GetResult(server_ret);
    EXPECT_GT(server_ret, 0);
    client_ret = read_callback.GetResult(client_ret);
    ASSERT_GT(client_ret, 0);
  }

  std::unique_ptr<FakeDataChannel> channel_1_;
  std::unique_ptr<FakeDataChannel> channel_2_;
  std::unique_ptr<TestSSLConfigService> ssl_config_service_;
  std::unique_ptr<MockCertVerifier> cert_verifier_;
  std::unique_ptr<MockClientCertVerifier> client_cert_verifier_;
  SSLConfig client_ssl_config_;
  // Note that this has a pointer to the `cert_verifier_`, so must be destroyed
  // before that is.
  SSLServerConfig server_ssl_config_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  std::unique_ptr<SSLClientSessionCache> ssl_client_session_cache_;
  std::unique_ptr<SSLClientContext> client_context_;
  std::unique_ptr<SSLServerContext> server_context_;
  std::unique_ptr<SSLClientSocket> client_socket_;
  std::unique_ptr<SSLServerSocket> server_socket_;
  std::unique_ptr<crypto::RSAPrivateKey> server_private_key_;
  scoped_refptr<SSLPrivateKey> server_ssl_private_key_;
  scoped_refptr<X509Certificate> server_cert_;
};

class SSLServerSocketReadTest : public SSLServerSocketTest,
                                public ::testing::WithParamInterface<bool> {
 protected:
  SSLServerSocketReadTest() : read_if_ready_enabled_(GetParam()) {}

  int Read(StreamSocket* socket,
           IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) {
    if (read_if_ready_enabled()) {
      return socket->ReadIfReady(buf, buf_len, std::move(callback));
    }
    return socket->Read(buf, buf_len, std::move(callback));
  }

  bool read_if_ready_enabled() const { return read_if_ready_enabled_; }

 private:
  const bool read_if_ready_enabled_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SSLServerSocketReadTest,
                         ::testing::Bool());

// This test only executes creation of client and server sockets. This is to
// test that creation of sockets doesn't crash and have minimal code to run
// with memory leak/corruption checking tools.
TEST_F(SSLServerSocketTest, Initialize) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
}

// This test executes Connect() on SSLClientSocket and Handshake() on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully.
TEST_F(SSLServerSocketTest, Handshake) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);

  // The default cipher suite should be ECDHE and an AEAD.
  uint16_t cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, &is_tls13,
                          cipher_suite);
  EXPECT_TRUE(is_aead);
}

// This test makes sure the session cache is working.
TEST_F(SSLServerSocketTest, HandshakeCached) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_FULL);
  SSLInfo ssl_server_info;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info));
  EXPECT_EQ(ssl_server_info.handshake_type, SSLInfo::HANDSHAKE_FULL);

  // Pump client read to get new session tickets.
  PumpServerToClient();

  // Make sure the second connection is cached.
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  TestCompletionCallback handshake_callback2;
  int server_ret2 = server_socket_->Handshake(handshake_callback2.callback());

  TestCompletionCallback connect_callback2;
  int client_ret2 = client_socket_->Connect(connect_callback2.callback());

  client_ret2 = connect_callback2.GetResult(client_ret2);
  server_ret2 = handshake_callback2.GetResult(server_ret2);

  ASSERT_THAT(client_ret2, IsOk());
  ASSERT_THAT(server_ret2, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info2;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info2));
  EXPECT_EQ(ssl_info2.handshake_type, SSLInfo::HANDSHAKE_RESUME);
  SSLInfo ssl_server_info2;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info2));
  EXPECT_EQ(ssl_server_info2.handshake_type, SSLInfo::HANDSHAKE_RESUME);
}

// This test makes sure the session cache separates out by server context.
TEST_F(SSLServerSocketTest, HandshakeCachedContextSwitch) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_FULL);
  SSLInfo ssl_server_info;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info));
  EXPECT_EQ(ssl_server_info.handshake_type, SSLInfo::HANDSHAKE_FULL);

  // Make sure the second connection is NOT cached when using a new context.
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback2;
  int server_ret2 = server_socket_->Handshake(handshake_callback2.callback());

  TestCompletionCallback connect_callback2;
  int client_ret2 = client_socket_->Connect(connect_callback2.callback());

  client_ret2 = connect_callback2.GetResult(client_ret2);
  server_ret2 = handshake_callback2.GetResult(server_ret2);

  ASSERT_THAT(client_ret2, IsOk());
  ASSERT_THAT(server_ret2, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info2;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info2));
  EXPECT_EQ(ssl_info2.handshake_type, SSLInfo::HANDSHAKE_FULL);
  SSLInfo ssl_server_info2;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info2));
  EXPECT_EQ(ssl_server_info2.handshake_type, SSLInfo::HANDSHAKE_FULL);
}

// Client certificates are disabled on iOS.
#if BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
// This test executes Connect() on SSLClientSocket and Handshake() on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully, using client certificate.
TEST_F(SSLServerSocketTest, HandshakeWithClientCert) {
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForClient(
      kClientCertFileName, kClientPrivateKeyFileName));
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  client_socket_->GetSSLInfo(&ssl_info);
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);
  server_socket_->GetSSLInfo(&ssl_info);
  ASSERT_TRUE(ssl_info.cert.get());
  EXPECT_TRUE(client_cert->EqualsExcludingChain(ssl_info.cert.get()));
}

// This test executes Connect() on SSLClientSocket and Handshake() twice on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully, using client certificate. The second connection is
// expected to succeed through the session cache.
TEST_F(SSLServerSocketTest, HandshakeWithClientCertCached) {
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForClient(
      kClientCertFileName, kClientPrivateKeyFileName));
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(ssl_info.handshake_type, SSLInfo::HANDSHAKE_FULL);
  SSLInfo ssl_server_info;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info));
  ASSERT_TRUE(ssl_server_info.cert.get());
  EXPECT_TRUE(client_cert->EqualsExcludingChain(ssl_server_info.cert.get()));
  EXPECT_EQ(ssl_server_info.handshake_type, SSLInfo::HANDSHAKE_FULL);
  // Pump client read to get new session tickets.
  PumpServerToClient();
  server_socket_->Disconnect();
  client_socket_->Disconnect();

  // Create the connection again.
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  TestCompletionCallback handshake_callback2;
  int server_ret2 = server_socket_->Handshake(handshake_callback2.callback());

  TestCompletionCallback connect_callback2;
  int client_ret2 = client_socket_->Connect(connect_callback2.callback());

  client_ret2 = connect_callback2.GetResult(client_ret2);
  server_ret2 = handshake_callback2.GetResult(server_ret2);

  ASSERT_THAT(client_ret2, IsOk());
  ASSERT_THAT(server_ret2, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info2;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info2));
  EXPECT_EQ(ssl_info2.handshake_type, SSLInfo::HANDSHAKE_RESUME);
  SSLInfo ssl_server_info2;
  ASSERT_TRUE(server_socket_->GetSSLInfo(&ssl_server_info2));
  ASSERT_TRUE(ssl_server_info2.cert.get());
  EXPECT_TRUE(client_cert->EqualsExcludingChain(ssl_server_info2.cert.get()));
  EXPECT_EQ(ssl_server_info2.handshake_type, SSLInfo::HANDSHAKE_RESUME);
}

TEST_F(SSLServerSocketTest, HandshakeWithClientCertRequiredNotSupplied) {
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  // Use the default setting for the client socket, which is to not send
  // a client certificate. This will cause the client to receive an
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED error, and allow for inspecting the
  // requested cert_authorities from the CertificateRequest sent by the
  // server.

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            connect_callback.GetResult(
                client_socket_->Connect(connect_callback.callback())));

  auto request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  client_socket_->GetSSLCertRequestInfo(request_info.get());

  // Check that the authority name that arrived in the CertificateRequest
  // handshake message is as expected.
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_TRUE(client_cert);
  EXPECT_TRUE(client_cert->IsIssuedByEncoded(request_info->cert_authorities));

  client_socket_->Disconnect();

  EXPECT_THAT(handshake_callback.GetResult(server_ret),
              IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(SSLServerSocketTest, HandshakeWithClientCertRequiredNotSuppliedCached) {
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  // Use the default setting for the client socket, which is to not send
  // a client certificate. This will cause the client to receive an
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED error, and allow for inspecting the
  // requested cert_authorities from the CertificateRequest sent by the
  // server.

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            connect_callback.GetResult(
                client_socket_->Connect(connect_callback.callback())));

  auto request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  client_socket_->GetSSLCertRequestInfo(request_info.get());

  // Check that the authority name that arrived in the CertificateRequest
  // handshake message is as expected.
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_TRUE(client_cert);
  EXPECT_TRUE(client_cert->IsIssuedByEncoded(request_info->cert_authorities));

  client_socket_->Disconnect();

  EXPECT_THAT(handshake_callback.GetResult(server_ret),
              IsError(ERR_CONNECTION_CLOSED));
  server_socket_->Disconnect();

  // Below, check that the cache didn't store the result of a failed handshake.
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  TestCompletionCallback handshake_callback2;
  int server_ret2 = server_socket_->Handshake(handshake_callback2.callback());

  TestCompletionCallback connect_callback2;
  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED,
            connect_callback2.GetResult(
                client_socket_->Connect(connect_callback2.callback())));

  auto request_info2 = base::MakeRefCounted<SSLCertRequestInfo>();
  client_socket_->GetSSLCertRequestInfo(request_info2.get());

  // Check that the authority name that arrived in the CertificateRequest
  // handshake message is as expected.
  EXPECT_TRUE(client_cert->IsIssuedByEncoded(request_info2->cert_authorities));

  client_socket_->Disconnect();

  EXPECT_THAT(handshake_callback2.GetResult(server_ret2),
              IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(SSLServerSocketTest, HandshakeWithWrongClientCertSupplied) {
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_TRUE(client_cert);

  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForClient(
      kWrongClientCertFileName, kWrongClientPrivateKeyFileName));
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  // In TLS 1.3, the client cert error isn't exposed until Read is called.
  EXPECT_EQ(OK, connect_callback.GetResult(client_ret));
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT,
            handshake_callback.GetResult(server_ret));

  // Pump client read to get client cert error.
  const int kReadBufSize = 1024;
  scoped_refptr<DrainableIOBuffer> read_buf =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<IOBufferWithSize>(kReadBufSize), kReadBufSize);
  TestCompletionCallback read_callback;
  client_ret = client_socket_->Read(read_buf.get(), read_buf->BytesRemaining(),
                                    read_callback.callback());
  client_ret = read_callback.GetResult(client_ret);
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT, client_ret);
}

TEST_F(SSLServerSocketTest, HandshakeWithWrongClientCertSuppliedTLS12) {
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_TRUE(client_cert);

  client_ssl_config_.version_max_override = SSL_PROTOCOL_VERSION_TLS1_2;
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForClient(
      kWrongClientCertFileName, kWrongClientPrivateKeyFileName));
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT,
            connect_callback.GetResult(client_ret));
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT,
            handshake_callback.GetResult(server_ret));
}

TEST_F(SSLServerSocketTest, HandshakeWithWrongClientCertSuppliedCached) {
  scoped_refptr<X509Certificate> client_cert =
      ImportCertFromFile(GetTestCertsDirectory(), kClientCertFileName);
  ASSERT_TRUE(client_cert);

  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForClient(
      kWrongClientCertFileName, kWrongClientPrivateKeyFileName));
  ASSERT_NO_FATAL_FAILURE(ConfigureClientCertsForServer());
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  // In TLS 1.3, the client cert error isn't exposed until Read is called.
  EXPECT_EQ(OK, connect_callback.GetResult(client_ret));
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT,
            handshake_callback.GetResult(server_ret));

  // Pump client read to get client cert error.
  const int kReadBufSize = 1024;
  scoped_refptr<DrainableIOBuffer> read_buf =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<IOBufferWithSize>(kReadBufSize), kReadBufSize);
  TestCompletionCallback read_callback;
  client_ret = client_socket_->Read(read_buf.get(), read_buf->BytesRemaining(),
                                    read_callback.callback());
  client_ret = read_callback.GetResult(client_ret);
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT, client_ret);

  client_socket_->Disconnect();
  server_socket_->Disconnect();

  // Below, check that the cache didn't store the result of a failed handshake.
  ASSERT_NO_FATAL_FAILURE(CreateSockets());
  TestCompletionCallback handshake_callback2;
  int server_ret2 = server_socket_->Handshake(handshake_callback2.callback());

  TestCompletionCallback connect_callback2;
  int client_ret2 = client_socket_->Connect(connect_callback2.callback());

  // In TLS 1.3, the client cert error isn't exposed until Read is called.
  EXPECT_EQ(OK, connect_callback2.GetResult(client_ret2));
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT,
            handshake_callback2.GetResult(server_ret2));

  // Pump client read to get client cert error.
  client_ret = client_socket_->Read(read_buf.get(), read_buf->BytesRemaining(),
                                    read_callback.callback());
  client_ret = read_callback.GetResult(client_ret);
  EXPECT_EQ(ERR_BAD_SSL_CLIENT_AUTH_CERT, client_ret);
}
#endif  // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

TEST_P(SSLServerSocketReadTest, DataTransfer) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  // Establish connection.
  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  client_ret = connect_callback.GetResult(client_ret);
  ASSERT_THAT(client_ret, IsOk());
  server_ret = handshake_callback.GetResult(server_ret);
  ASSERT_THAT(server_ret, IsOk());

  const int kReadBufSize = 1024;
  scoped_refptr<StringIOBuffer> write_buf =
      base::MakeRefCounted<StringIOBuffer>("testing123");
  scoped_refptr<DrainableIOBuffer> read_buf =
      base::MakeRefCounted<DrainableIOBuffer>(
          base::MakeRefCounted<IOBufferWithSize>(kReadBufSize), kReadBufSize);

  // Write then read.
  TestCompletionCallback write_callback;
  TestCompletionCallback read_callback;
  server_ret = server_socket_->Write(write_buf.get(), write_buf->size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);
  client_ret = client_socket_->Read(
      read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  server_ret = write_callback.GetResult(server_ret);
  EXPECT_GT(server_ret, 0);
  client_ret = read_callback.GetResult(client_ret);
  ASSERT_GT(client_ret, 0);

  read_buf->DidConsume(client_ret);
  while (read_buf->BytesConsumed() < write_buf->size()) {
    client_ret = client_socket_->Read(
        read_buf.get(), read_buf->BytesRemaining(), read_callback.callback());
    EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);
    client_ret = read_callback.GetResult(client_ret);
    ASSERT_GT(client_ret, 0);
    read_buf->DidConsume(client_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));

  // Read then write.
  write_buf = base::MakeRefCounted<StringIOBuffer>("hello123");
  server_ret = Read(server_socket_.get(), read_buf.get(),
                    read_buf->BytesRemaining(), read_callback.callback());
  EXPECT_EQ(server_ret, ERR_IO_PENDING);
  client_ret = client_socket_->Write(write_buf.get(), write_buf->size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  server_ret = read_callback.GetResult(server_ret);
  if (read_if_ready_enabled()) {
    // ReadIfReady signals the data is available but does not consume it.
    // The data is consumed later below.
    ASSERT_EQ(server_ret, OK);
  } else {
    ASSERT_GT(server_ret, 0);
    read_buf->DidConsume(server_ret);
  }
  client_ret = write_callback.GetResult(client_ret);
  EXPECT_GT(client_ret, 0);

  while (read_buf->BytesConsumed() < write_buf->size()) {
    server_ret = Read(server_socket_.get(), read_buf.get(),
                      read_buf->BytesRemaining(), read_callback.callback());
    // All the data was written above, so the data should be synchronously
    // available out of both Read() and ReadIfReady().
    ASSERT_GT(server_ret, 0);
    read_buf->DidConsume(server_ret);
  }
  EXPECT_EQ(write_buf->size(), read_buf->BytesConsumed());
  read_buf->SetOffset(0);
  EXPECT_EQ(0, memcmp(write_buf->data(), read_buf->data(), write_buf->size()));
}

// A regression test for bug 127822 (http://crbug.com/127822).
// If the server closes the connection after the handshake is finished,
// the client's Write() call should not cause an infinite loop.
// NOTE: this is a test for SSLClientSocket rather than SSLServerSocket.
TEST_F(SSLServerSocketTest, ClientWriteAfterServerClose) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  // Establish connection.
  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  client_ret = connect_callback.GetResult(client_ret);
  ASSERT_THAT(client_ret, IsOk());
  server_ret = handshake_callback.GetResult(server_ret);
  ASSERT_THAT(server_ret, IsOk());

  scoped_refptr<StringIOBuffer> write_buf =
      base::MakeRefCounted<StringIOBuffer>("testing123");

  // The server closes the connection. The server needs to write some
  // data first so that the client's Read() calls from the transport
  // socket won't return ERR_IO_PENDING.  This ensures that the client
  // will call Read() on the transport socket again.
  TestCompletionCallback write_callback;
  server_ret = server_socket_->Write(write_buf.get(), write_buf->size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(server_ret > 0 || server_ret == ERR_IO_PENDING);

  server_ret = write_callback.GetResult(server_ret);
  EXPECT_GT(server_ret, 0);

  server_socket_->Disconnect();

  // The client writes some data. This should not cause an infinite loop.
  client_ret = client_socket_->Write(write_buf.get(), write_buf->size(),
                                     write_callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(client_ret > 0 || client_ret == ERR_IO_PENDING);

  client_ret = write_callback.GetResult(client_ret);
  EXPECT_GT(client_ret, 0);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(10));
  run_loop.Run();
}

// This test executes ExportKeyingMaterial() on the client and server sockets,
// after connecting them, and verifies that the results match.
// This test will fail if False Start is enabled (see crbug.com/90208).
TEST_F(SSLServerSocketTest, ExportKeyingMaterial) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());
  ASSERT_TRUE(client_ret == OK || client_ret == ERR_IO_PENDING);

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_TRUE(server_ret == OK || server_ret == ERR_IO_PENDING);

  if (client_ret == ERR_IO_PENDING) {
    ASSERT_THAT(connect_callback.WaitForResult(), IsOk());
  }
  if (server_ret == ERR_IO_PENDING) {
    ASSERT_THAT(handshake_callback.WaitForResult(), IsOk());
  }

  const int kKeyingMaterialSize = 32;
  const char kKeyingLabel[] = "EXPERIMENTAL-server-socket-test";
  const char kKeyingContext[] = "";
  unsigned char server_out[kKeyingMaterialSize];
  int rv = server_socket_->ExportKeyingMaterial(
      kKeyingLabel, false, kKeyingContext, server_out, sizeof(server_out));
  ASSERT_THAT(rv, IsOk());

  unsigned char client_out[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(kKeyingLabel, false, kKeyingContext,
                                            client_out, sizeof(client_out));
  ASSERT_THAT(rv, IsOk());
  EXPECT_EQ(0, memcmp(server_out, client_out, sizeof(server_out)));

  const char kKeyingLabelBad[] = "EXPERIMENTAL-server-socket-test-bad";
  unsigned char client_bad[kKeyingMaterialSize];
  rv = client_socket_->ExportKeyingMaterial(
      kKeyingLabelBad, false, kKeyingContext, client_bad, sizeof(client_bad));
  ASSERT_EQ(rv, OK);
  EXPECT_NE(0, memcmp(server_out, client_bad, sizeof(server_out)));
}

// Verifies that SSLConfig::require_ecdhe flags works properly.
TEST_F(SSLServerSocketTest, RequireEcdheFlag) {
  // Disable all ECDHE suites on the client side.
  SSLContextConfig config;
  config.disabled_cipher_suites.assign(
      kEcdheCiphers, kEcdheCiphers + std::size(kEcdheCiphers));

  // Legacy RSA key exchange ciphers only exist in TLS 1.2 and below.
  config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ssl_config_service_->UpdateSSLConfigAndNotify(config);

  // Require ECDHE on the server.
  server_ssl_config_.require_ecdhe = true;

  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
  ASSERT_THAT(server_ret, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

// This test executes Connect() on SSLClientSocket and Handshake() on
// SSLServerSocket to make sure handshaking between the two sockets is
// completed successfully. The server key is represented by SSLPrivateKey.
TEST_F(SSLServerSocketTest, HandshakeServerSSLPrivateKey) {
  ASSERT_NO_FATAL_FAILURE(CreateContextSSLPrivateKey());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // Make sure the cert status is expected.
  SSLInfo ssl_info;
  ASSERT_TRUE(client_socket_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);

  // The default cipher suite should be ECDHE and an AEAD.
  uint16_t cipher_suite =
      SSLConnectionStatusToCipherSuite(ssl_info.connection_status);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead, &is_tls13,
                          cipher_suite);
  EXPECT_TRUE(is_aead);
}

namespace {

// Helper that wraps an underlying SSLPrivateKey to allow the test to
// do some work immediately before a `Sign()` operation is performed.
class SSLPrivateKeyHook : public SSLPrivateKey {
 public:
  SSLPrivateKeyHook(scoped_refptr<SSLPrivateKey> private_key,
                    base::RepeatingClosure on_sign)
      : private_key_(std::move(private_key)), on_sign_(std::move(on_sign)) {}

  // SSLPrivateKey implementation.
  std::string GetProviderName() override {
    return private_key_->GetProviderName();
  }
  std::vector<uint16_t> GetAlgorithmPreferences() override {
    return private_key_->GetAlgorithmPreferences();
  }
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override {
    on_sign_.Run();
    private_key_->Sign(algorithm, input, std::move(callback));
  }

 private:
  ~SSLPrivateKeyHook() override = default;

  const scoped_refptr<SSLPrivateKey> private_key_;
  const base::RepeatingClosure on_sign_;
};

}  // namespace

// Verifies that if the client disconnects while during private key signing then
// the disconnection is correctly reported to the `Handshake()` completion
// callback, with `ERR_CONNECTION_CLOSED`.
// This is a regression test for crbug.com/1449461.
TEST_F(SSLServerSocketTest,
       HandshakeServerSSLPrivateKeyDisconnectDuringSigning_ReturnsError) {
  auto on_sign = base::BindLambdaForTesting([&]() {
    client_socket_->Disconnect();
    ASSERT_FALSE(client_socket_->IsConnected());
  });
  server_ssl_private_key_ = base::MakeRefCounted<SSLPrivateKeyHook>(
      std::move(server_ssl_private_key_), on_sign);
  ASSERT_NO_FATAL_FAILURE(CreateContextSSLPrivateKey());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_EQ(server_ret, net::ERR_IO_PENDING);

  TestCompletionCallback connect_callback;
  client_socket_->Connect(connect_callback.callback());

  // If resuming the handshake after private-key signing is not handled
  // correctly as per crbug.com/1449461 then the test will hang and timeout
  // at this point, due to the server-side completion callback not being
  // correctly invoked.
  server_ret = handshake_callback.GetResult(server_ret);
  EXPECT_EQ(server_ret, net::ERR_CONNECTION_CLOSED);
}

// Verifies that non-ECDHE ciphers are disabled when using SSLPrivateKey as the
// server key.
TEST_F(SSLServerSocketTest, HandshakeServerSSLPrivateKeyRequireEcdhe) {
  // Disable all ECDHE suites on the client side.
  SSLContextConfig config;
  config.disabled_cipher_suites.assign(
      kEcdheCiphers, kEcdheCiphers + std::size(kEcdheCiphers));
  // TLS 1.3 always works with SSLPrivateKey.
  config.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
  ssl_config_service_->UpdateSSLConfigAndNotify(config);

  ASSERT_NO_FATAL_FAILURE(CreateContextSSLPrivateKey());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
  ASSERT_THAT(server_ret, IsError(ERR_SSL_VERSION_OR_CIPHER_MISMATCH));
}

class SSLServerSocketAlpsTest
    : public SSLServerSocketTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SSLServerSocketAlpsTest()
      : client_alps_enabled_(std::get<0>(GetParam())),
        server_alps_enabled_(std::get<1>(GetParam())) {}
  ~SSLServerSocketAlpsTest() override = default;
  const bool client_alps_enabled_;
  const bool server_alps_enabled_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SSLServerSocketAlpsTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

TEST_P(SSLServerSocketAlpsTest, Alps) {
  const std::string server_data = "server sends some test data";
  const std::string client_data = "client also sends some data";

  server_ssl_config_.alpn_protos = {kProtoHTTP2};
  if (server_alps_enabled_) {
    server_ssl_config_.application_settings[kProtoHTTP2] =
        std::vector<uint8_t>(server_data.begin(), server_data.end());
  }

  client_ssl_config_.alpn_protos = {kProtoHTTP2};
  if (client_alps_enabled_) {
    client_ssl_config_.application_settings[kProtoHTTP2] =
        std::vector<uint8_t>(client_data.begin(), client_data.end());
  }

  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());

  client_ret = connect_callback.GetResult(client_ret);
  server_ret = handshake_callback.GetResult(server_ret);

  ASSERT_THAT(client_ret, IsOk());
  ASSERT_THAT(server_ret, IsOk());

  // ALPS is negotiated only if ALPS is enabled both on client and server.
  const auto alps_data_received_by_client =
      client_socket_->GetPeerApplicationSettings();
  const auto alps_data_received_by_server =
      server_socket_->GetPeerApplicationSettings();

  if (client_alps_enabled_ && server_alps_enabled_) {
    ASSERT_TRUE(alps_data_received_by_client.has_value());
    EXPECT_EQ(server_data, alps_data_received_by_client.value());
    ASSERT_TRUE(alps_data_received_by_server.has_value());
    EXPECT_EQ(client_data, alps_data_received_by_server.value());
  } else {
    EXPECT_FALSE(alps_data_received_by_client.has_value());
    EXPECT_FALSE(alps_data_received_by_server.has_value());
  }
}

// Test that CancelReadIfReady works.
TEST_F(SSLServerSocketTest, CancelReadIfReady) {
  ASSERT_NO_FATAL_FAILURE(CreateContext());
  ASSERT_NO_FATAL_FAILURE(CreateSockets());

  TestCompletionCallback connect_callback;
  int client_ret = client_socket_->Connect(connect_callback.callback());
  TestCompletionCallback handshake_callback;
  int server_ret = server_socket_->Handshake(handshake_callback.callback());
  ASSERT_THAT(connect_callback.GetResult(client_ret), IsOk());
  ASSERT_THAT(handshake_callback.GetResult(server_ret), IsOk());

  // Attempt to read from the server socket. There will not be anything to read.
  // Cancel the read immediately afterwards.
  TestCompletionCallback read_callback;
  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(1);
  int read_ret =
      server_socket_->ReadIfReady(read_buf.get(), 1, read_callback.callback());
  ASSERT_THAT(read_ret, IsError(ERR_IO_PENDING));
  ASSERT_THAT(server_socket_->CancelReadIfReady(), IsOk());

  // After the client writes data, the server should still not pick up a result.
  auto write_buf = base::MakeRefCounted<StringIOBuffer>("a");
  TestCompletionCallback write_callback;
  ASSERT_EQ(write_callback.GetResult(client_socket_->Write(
                write_buf.get(), write_buf->size(), write_callback.callback(),
                TRAFFIC_ANNOTATION_FOR_TESTS)),
            write_buf->size());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(read_callback.have_result());

  // After a canceled read, future reads are still possible.
  while (true) {
    TestCompletionCallback read_callback2;
    read_ret = server_socket_->ReadIfReady(read_buf.get(), 1,
                                           read_callback2.callback());
    if (read_ret != ERR_IO_PENDING) {
      break;
    }
    ASSERT_THAT(read_callback2.GetResult(read_ret), IsOk());
  }
  ASSERT_EQ(1, read_ret);
  EXPECT_EQ(read_buf->data()[0], 'a');
}

}  // namespace net
