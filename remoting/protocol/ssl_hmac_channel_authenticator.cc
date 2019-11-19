// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ssl_hmac_channel_authenticator.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "crypto/secure_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_server_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/p2p_stream_socket.h"

#if defined(OS_NACL)
#include "net/socket/ssl_client_socket_impl.h"
#else
#include "net/socket/client_socket_factory.h"
#endif

namespace remoting {
namespace protocol {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ssl_hmac_channel_authenticator",
                                        R"(
        semantics {
          sender: "Ssl Hmac Channel Authenticator"
          description:
            "Performs the required authentication to start a Chrome Remote "
            "Desktop connection."
          trigger:
            "Initiating a Chrome Remote Desktop connection."
          data: "No user data."
          destination: OTHER
          destination_other:
            "The Chrome Remote Desktop client/host that user is connecting to."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented. 'RemoteAccessHostClientDomainList' and "
            "'RemoteAccessHostDomainList' policies can limit the domains to "
            "which a connection can be made, but they cannot be used to block "
            "the request to all domains. Please refer to help desk for other "
            "approaches to manage this feature."
        })");

// A CertVerifier which rejects every certificate.
class FailingCertVerifier : public net::CertVerifier {
 public:
  FailingCertVerifier() = default;
  ~FailingCertVerifier() override = default;

  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->verified_cert = params.certificate();
    verify_result->cert_status = net::CERT_STATUS_INVALID;
    return net::ERR_CERT_INVALID;
  }
  void SetConfig(const Config& config) override {}
};

// Implements net::StreamSocket interface on top of P2PStreamSocket to be passed
// to net::SSLClientSocket and net::SSLServerSocket.
class NetStreamSocketAdapter : public net::StreamSocket {
 public:
  NetStreamSocketAdapter(std::unique_ptr<P2PStreamSocket> socket)
      : socket_(std::move(socket)) {}
  ~NetStreamSocketAdapter() override = default;

  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    return socket_->Read(buf, buf_len, std::move(callback));
  }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_->Write(buf, buf_len, std::move(callback),
                          traffic_annotation);
  }

  int SetReceiveBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

  int SetSendBufferSize(int32_t size) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }

  int Connect(net::CompletionOnceCallback callback) override {
    NOTREACHED();
    return net::ERR_FAILED;
  }
  void Disconnect() override { socket_.reset(); }
  bool IsConnected() const override { return true; }
  bool IsConnectedAndIdle() const override { return true; }
  int GetPeerAddress(net::IPEndPoint* address) const override {
    // SSL sockets call this function so it must return some result.
    *address = net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0);
    return net::OK;
  }
  int GetLocalAddress(net::IPEndPoint* address) const override {
    NOTREACHED();
    return net::ERR_FAILED;
  }
  const net::NetLogWithSource& NetLog() const override { return net_log_; }
  bool WasEverUsed() const override {
    NOTREACHED();
    return true;
  }
  bool WasAlpnNegotiated() const override {
    NOTREACHED();
    return false;
  }
  net::NextProto GetNegotiatedProtocol() const override {
    NOTREACHED();
    return net::kProtoUnknown;
  }
  bool GetSSLInfo(net::SSLInfo* ssl_info) override {
    NOTREACHED();
    return false;
  }
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override {
    NOTREACHED();
  }
  void ClearConnectionAttempts() override { NOTREACHED(); }
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
    NOTREACHED();
  }
  int64_t GetTotalReceivedBytes() const override {
    NOTIMPLEMENTED();
    return 0;
  }
  void ApplySocketTag(const net::SocketTag& tag) override { NOTIMPLEMENTED(); }

 private:
  std::unique_ptr<P2PStreamSocket> socket_;
  net::NetLogWithSource net_log_;
};

}  // namespace

// Implements P2PStreamSocket interface on top of net::StreamSocket.
class SslHmacChannelAuthenticator::P2PStreamSocketAdapter
    : public P2PStreamSocket {
 public:
  P2PStreamSocketAdapter(SslSocketContext socket_context,
                         std::unique_ptr<net::StreamSocket> socket)
      : socket_context_(std::move(socket_context)),
        socket_(std::move(socket)) {}
  ~P2PStreamSocketAdapter() override = default;

  int Read(const scoped_refptr<net::IOBuffer>& buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    return socket_->Read(buf.get(), buf_len, std::move(callback));
  }
  int Write(
      const scoped_refptr<net::IOBuffer>& buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_->Write(buf.get(), buf_len, std::move(callback),
                          traffic_annotation);
  }

 private:
  // The socket_context_ must outlive any associated sockets.
  SslSocketContext socket_context_;
  std::unique_ptr<net::StreamSocket> socket_;
};

SslHmacChannelAuthenticator::SslSocketContext::SslSocketContext() = default;
SslHmacChannelAuthenticator::SslSocketContext::SslSocketContext(
    SslSocketContext&&) = default;
SslHmacChannelAuthenticator::SslSocketContext::~SslSocketContext() = default;
SslHmacChannelAuthenticator::SslSocketContext&
SslHmacChannelAuthenticator::SslSocketContext::operator=(SslSocketContext&&) =
    default;

// static
std::unique_ptr<SslHmacChannelAuthenticator>
SslHmacChannelAuthenticator::CreateForClient(const std::string& remote_cert,
                                             const std::string& auth_key) {
  std::unique_ptr<SslHmacChannelAuthenticator> result(
      new SslHmacChannelAuthenticator(auth_key));
  result->remote_cert_ = remote_cert;
  return result;
}

std::unique_ptr<SslHmacChannelAuthenticator>
SslHmacChannelAuthenticator::CreateForHost(const std::string& local_cert,
                                           scoped_refptr<RsaKeyPair> key_pair,
                                           const std::string& auth_key) {
  std::unique_ptr<SslHmacChannelAuthenticator> result(
      new SslHmacChannelAuthenticator(auth_key));
  result->local_cert_ = local_cert;
  result->local_key_pair_ = key_pair;
  return result;
}

SslHmacChannelAuthenticator::SslHmacChannelAuthenticator(
    const std::string& auth_key)
    : auth_key_(auth_key) {
}

SslHmacChannelAuthenticator::~SslHmacChannelAuthenticator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SslHmacChannelAuthenticator::SecureAndAuthenticate(
    std::unique_ptr<P2PStreamSocket> socket,
    const DoneCallback& done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  done_callback_ = done_callback;

  int result;
  if (is_ssl_server()) {
#if defined(OS_NACL)
    // Client plugin doesn't use server SSL sockets, and so SSLServerSocket
    // implementation is not compiled for NaCl as part of net_nacl.
    NOTREACHED();
    result = net::ERR_FAILED;
#else
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(local_cert_.data(),
                                              local_cert_.length());
    if (!cert) {
      LOG(ERROR) << "Failed to parse X509Certificate";
      NotifyError(net::ERR_FAILED);
      return;
    }

    net::SSLServerConfig ssl_config;
    ssl_config.require_ecdhe = true;

    socket_context_.server_context = net::CreateSSLServerContext(
        cert.get(), *local_key_pair_->private_key(), ssl_config);

    std::unique_ptr<net::SSLServerSocket> server_socket =
        socket_context_.server_context->CreateSSLServerSocket(
            std::make_unique<NetStreamSocketAdapter>(std::move(socket)));
    net::SSLServerSocket* raw_server_socket = server_socket.get();
    socket_ = std::move(server_socket);
    result = raw_server_socket->Handshake(base::Bind(
        &SslHmacChannelAuthenticator::OnConnected, base::Unretained(this)));
#endif
  } else {
    socket_context_.transport_security_state =
        std::make_unique<net::TransportSecurityState>();
    socket_context_.cert_verifier = std::make_unique<FailingCertVerifier>();
    socket_context_.ct_verifier = std::make_unique<net::DoNothingCTVerifier>();
    socket_context_.ct_policy_enforcer =
        std::make_unique<net::DefaultCTPolicyEnforcer>();
    socket_context_.client_context = std::make_unique<net::SSLClientContext>(
        nullptr /* default config */, socket_context_.cert_verifier.get(),
        socket_context_.transport_security_state.get(),
        socket_context_.ct_verifier.get(),
        socket_context_.ct_policy_enforcer.get(),
        nullptr /* no session caching */);

    net::SSLConfig ssl_config;
    ssl_config.require_ecdhe = true;

    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(remote_cert_.data(),
                                              remote_cert_.length());
    if (!cert) {
      LOG(ERROR) << "Failed to parse X509Certificate";
      NotifyError(net::ERR_FAILED);
      return;
    }

    ssl_config.allowed_bad_certs.emplace_back(
        std::move(cert), net::CERT_STATUS_AUTHORITY_INVALID);

    net::HostPortPair host_and_port(kSslFakeHostName, 0);
    std::unique_ptr<net::StreamSocket> stream_socket =
        std::make_unique<NetStreamSocketAdapter>(std::move(socket));
#if defined(OS_NACL)
    // net_nacl doesn't include ClientSocketFactory.
    socket_ = socket_context_.client_context->CreateSSLClientSocket(
        std::move(stream_socket), host_and_port, ssl_config);
#else
    socket_ =
        net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
            socket_context_.client_context.get(), std::move(stream_socket),
            host_and_port, ssl_config);
#endif

    result = socket_->Connect(base::Bind(
        &SslHmacChannelAuthenticator::OnConnected, base::Unretained(this)));
  }

  if (result == net::ERR_IO_PENDING)
    return;

  OnConnected(result);
}

bool SslHmacChannelAuthenticator::is_ssl_server() {
  return local_key_pair_.get() != nullptr;
}

void SslHmacChannelAuthenticator::OnConnected(int result) {
  if (result != net::OK) {
    LOG(WARNING) << "Failed to establish SSL connection.  Error: "
                 << net::ErrorToString(result);
    NotifyError(result);
    return;
  }

  // Generate authentication digest to write to the socket.
  std::string auth_bytes = GetAuthBytes(
      socket_.get(), is_ssl_server() ?
      kHostAuthSslExporterLabel : kClientAuthSslExporterLabel, auth_key_);
  if (auth_bytes.empty()) {
    NotifyError(net::ERR_FAILED);
    return;
  }

  // Allocate a buffer to write the digest.
  auth_write_buf_ = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(auth_bytes), auth_bytes.size());

  // Read an incoming token.
  auth_read_buf_ = base::MakeRefCounted<net::GrowableIOBuffer>();
  auth_read_buf_->SetCapacity(kAuthDigestLength);

  // If WriteAuthenticationBytes() results in |done_callback_| being
  // called then we must not do anything else because this object may
  // be destroyed at that point.
  bool callback_called = false;
  WriteAuthenticationBytes(&callback_called);
  if (!callback_called)
    ReadAuthenticationBytes();
}

void SslHmacChannelAuthenticator::WriteAuthenticationBytes(
    bool* callback_called) {
  while (true) {
    int result = socket_->Write(
        auth_write_buf_.get(), auth_write_buf_->BytesRemaining(),
        base::Bind(&SslHmacChannelAuthenticator::OnAuthBytesWritten,
                   base::Unretained(this)),
        kTrafficAnnotation);
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesWritten(result, callback_called))
      break;
  }
}

void SslHmacChannelAuthenticator::OnAuthBytesWritten(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (HandleAuthBytesWritten(result, nullptr))
    WriteAuthenticationBytes(nullptr);
}

bool SslHmacChannelAuthenticator::HandleAuthBytesWritten(
    int result, bool* callback_called) {
  if (result <= 0) {
    LOG(ERROR) << "Error writing authentication: " << result;
    if (callback_called)
      *callback_called = false;
    NotifyError(result);
    return false;
  }

  auth_write_buf_->DidConsume(result);
  if (auth_write_buf_->BytesRemaining() > 0)
    return true;

  auth_write_buf_ = nullptr;
  CheckDone(callback_called);
  return false;
}

void SslHmacChannelAuthenticator::ReadAuthenticationBytes() {
  while (true) {
    int result =
        socket_->Read(auth_read_buf_.get(),
                      auth_read_buf_->RemainingCapacity(),
                      base::Bind(&SslHmacChannelAuthenticator::OnAuthBytesRead,
                                 base::Unretained(this)));
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesRead(result))
      break;
  }
}

void SslHmacChannelAuthenticator::OnAuthBytesRead(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (HandleAuthBytesRead(result))
    ReadAuthenticationBytes();
}

bool SslHmacChannelAuthenticator::HandleAuthBytesRead(int read_result) {
  if (read_result <= 0) {
    NotifyError(read_result);
    return false;
  }

  auth_read_buf_->set_offset(auth_read_buf_->offset() + read_result);
  if (auth_read_buf_->RemainingCapacity() > 0)
    return true;

  if (!VerifyAuthBytes(std::string(
          auth_read_buf_->StartOfBuffer(),
          auth_read_buf_->StartOfBuffer() + kAuthDigestLength))) {
    LOG(WARNING) << "Mismatched authentication";
    NotifyError(net::ERR_FAILED);
    return false;
  }

  auth_read_buf_ = nullptr;
  CheckDone(nullptr);
  return false;
}

bool SslHmacChannelAuthenticator::VerifyAuthBytes(
    const std::string& received_auth_bytes) {
  DCHECK(received_auth_bytes.length() == kAuthDigestLength);

  // Compute expected auth bytes.
  std::string auth_bytes = GetAuthBytes(
      socket_.get(), is_ssl_server() ?
      kClientAuthSslExporterLabel : kHostAuthSslExporterLabel, auth_key_);
  if (auth_bytes.empty())
    return false;

  return crypto::SecureMemEqual(received_auth_bytes.data(),
                                &(auth_bytes[0]), kAuthDigestLength);
}

void SslHmacChannelAuthenticator::CheckDone(bool* callback_called) {
  if (auth_write_buf_.get() == nullptr && auth_read_buf_.get() == nullptr) {
    DCHECK(socket_.get() != nullptr);
    if (callback_called)
      *callback_called = true;

    std::move(done_callback_)
        .Run(net::OK, std::make_unique<P2PStreamSocketAdapter>(
                          std::move(socket_context_), std::move(socket_)));
  }
}

void SslHmacChannelAuthenticator::NotifyError(int error) {
  std::move(done_callback_).Run(error, nullptr);
}

}  // namespace protocol
}  // namespace remoting
