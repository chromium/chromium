// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_server_socket_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/client_cert_verifier.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_bio_adapter.h"
#include "net/ssl/openssl_ssl_util.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

#define GotoState(s) next_handshake_state_ = s

namespace net {

namespace {

// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kSSLServerSocketNoPendingResult = 1;

class SocketDataIndex {
 public:
  static SocketDataIndex* GetInstance();
  SocketDataIndex() {
    ssl_socket_data_index_ =
        SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  }

  // This is the index used with SSL_get_ex_data to retrieve the owner
  // SSLServerSocketImpl object from an SSL instance.
  int ssl_socket_data_index_;
};

base::LazyInstance<SocketDataIndex>::Leaky g_ssl_socket_data_index_ =
    LAZY_INSTANCE_INITIALIZER;

// static
SocketDataIndex* SocketDataIndex::GetInstance() {
  return g_ssl_socket_data_index_.Pointer();
}

}  // namespace

class SSLServerContextImpl::SocketImpl : public SSLServerSocket,
                                         public SocketBIOAdapter::Delegate {
 public:
  SocketImpl(SSLServerContextImpl* context,
             std::unique_ptr<StreamSocket> socket);
  ~SocketImpl() override;

  // SSLServerSocket interface.
  int Handshake(CompletionOnceCallback callback) override;

  // SSLSocket interface.
  int ExportKeyingMaterial(const base::StringPiece& label,
                           bool has_context,
                           const base::StringPiece& context,
                           unsigned char* out,
                           unsigned int outlen) override;

  // Socket interface (via StreamSocket).
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
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  bool WasAlpnNegotiated() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  static ssl_verify_result_t CertVerifyCallback(SSL* ssl, uint8_t* out_alert);
  ssl_verify_result_t CertVerifyCallbackImpl(uint8_t* out_alert);

  static const SSL_PRIVATE_KEY_METHOD kPrivateKeyMethod;
  static ssl_private_key_result_t PrivateKeySignCallback(SSL* ssl,
                                                         uint8_t* out,
                                                         size_t* out_len,
                                                         size_t max_out,
                                                         uint16_t algorithm,
                                                         const uint8_t* in,
                                                         size_t in_len);
  static ssl_private_key_result_t PrivateKeyDecryptCallback(SSL* ssl,
                                                            uint8_t* out,
                                                            size_t* out_len,
                                                            size_t max_out,
                                                            const uint8_t* in,
                                                            size_t in_len);
  static ssl_private_key_result_t PrivateKeyCompleteCallback(SSL* ssl,
                                                             uint8_t* out,
                                                             size_t* out_len,
                                                             size_t max_out);

  ssl_private_key_result_t PrivateKeySignCallback(uint8_t* out,
                                                  size_t* out_len,
                                                  size_t max_out,
                                                  uint16_t algorithm,
                                                  const uint8_t* in,
                                                  size_t in_len);
  ssl_private_key_result_t PrivateKeyCompleteCallback(uint8_t* out,
                                                      size_t* out_len,
                                                      size_t max_out);
  void OnPrivateKeyComplete(Error error, const std::vector<uint8_t>& signature);

  static int ALPNSelectCallback(SSL* ssl,
                                const uint8_t** out,
                                uint8_t* out_len,
                                const uint8_t* in,
                                unsigned in_len,
                                void* arg);

  // SocketBIOAdapter::Delegate implementation.
  void OnReadReady() override;
  void OnWriteReady() override;

 private:
  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
  };

  void OnHandshakeIOComplete(int result);

  int DoPayloadRead(IOBuffer* buf, int buf_len);
  int DoPayloadWrite();

  int DoHandshakeLoop(int last_io_result);
  int DoHandshake();
  void DoHandshakeCallback(int result);
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  int Init();
  void ExtractClientCert();

  SSLServerContextImpl* context_;

  NetLogWithSource net_log_;

  CompletionOnceCallback user_handshake_callback_;
  CompletionOnceCallback user_read_callback_;
  CompletionOnceCallback user_write_callback_;

  // SSLPrivateKey signature.
  int signature_result_;
  std::vector<uint8_t> signature_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;

  // OpenSSL stuff
  bssl::UniquePtr<SSL> ssl_;

  // Whether we received any data in early data.
  bool early_data_received_;

  // StreamSocket for sending and receiving data.
  std::unique_ptr<StreamSocket> transport_socket_;
  std::unique_ptr<SocketBIOAdapter> transport_adapter_;

  // Certificate for the client.
  scoped_refptr<X509Certificate> client_cert_;

  State next_handshake_state_;
  bool completed_handshake_;

  NextProto negotiated_protocol_;

  base::WeakPtrFactory<SocketImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SocketImpl);
};

SSLServerContextImpl::SocketImpl::SocketImpl(
    SSLServerContextImpl* context,
    std::unique_ptr<StreamSocket> transport_socket)
    : context_(context),
      signature_result_(kSSLServerSocketNoPendingResult),
      user_read_buf_len_(0),
      user_write_buf_len_(0),
      early_data_received_(false),
      transport_socket_(std::move(transport_socket)),
      next_handshake_state_(STATE_NONE),
      completed_handshake_(false),
      negotiated_protocol_(kProtoUnknown) {
  ssl_.reset(SSL_new(context_->ssl_ctx_.get()));
  SSL_set_app_data(ssl_.get(), this);
  SSL_set_shed_handshake_config(ssl_.get(), 1);
}

SSLServerContextImpl::SocketImpl::~SocketImpl() {
  if (ssl_) {
    // Calling SSL_shutdown prevents the session from being marked as
    // unresumable.
    SSL_shutdown(ssl_.get());
    ssl_.reset();
  }
}

// static
const SSL_PRIVATE_KEY_METHOD
    SSLServerContextImpl::SocketImpl::kPrivateKeyMethod = {
        &SSLServerContextImpl::SocketImpl::PrivateKeySignCallback,
        &SSLServerContextImpl::SocketImpl::PrivateKeyDecryptCallback,
        &SSLServerContextImpl::SocketImpl::PrivateKeyCompleteCallback,
};

// static
ssl_private_key_result_t
SSLServerContextImpl::SocketImpl::PrivateKeySignCallback(SSL* ssl,
                                                         uint8_t* out,
                                                         size_t* out_len,
                                                         size_t max_out,
                                                         uint16_t algorithm,
                                                         const uint8_t* in,
                                                         size_t in_len) {
  DCHECK(ssl);
  SSLServerContextImpl::SocketImpl* socket =
      static_cast<SSLServerContextImpl::SocketImpl*>(SSL_get_ex_data(
          ssl, SocketDataIndex::GetInstance()->ssl_socket_data_index_));
  DCHECK(socket);
  return socket->PrivateKeySignCallback(out, out_len, max_out, algorithm, in,
                                        in_len);
}

// static
ssl_private_key_result_t
SSLServerContextImpl::SocketImpl::PrivateKeyDecryptCallback(SSL* ssl,
                                                            uint8_t* out,
                                                            size_t* out_len,
                                                            size_t max_out,
                                                            const uint8_t* in,
                                                            size_t in_len) {
  // Decrypt is not supported.
  return ssl_private_key_failure;
}

// static
ssl_private_key_result_t
SSLServerContextImpl::SocketImpl::PrivateKeyCompleteCallback(SSL* ssl,
                                                             uint8_t* out,
                                                             size_t* out_len,
                                                             size_t max_out) {
  DCHECK(ssl);
  SSLServerContextImpl::SocketImpl* socket =
      static_cast<SSLServerContextImpl::SocketImpl*>(SSL_get_ex_data(
          ssl, SocketDataIndex::GetInstance()->ssl_socket_data_index_));
  DCHECK(socket);
  return socket->PrivateKeyCompleteCallback(out, out_len, max_out);
}

ssl_private_key_result_t
SSLServerContextImpl::SocketImpl::PrivateKeySignCallback(uint8_t* out,
                                                         size_t* out_len,
                                                         size_t max_out,
                                                         uint16_t algorithm,
                                                         const uint8_t* in,
                                                         size_t in_len) {
  DCHECK(context_);
  DCHECK(context_->private_key_);
  signature_result_ = ERR_IO_PENDING;
  context_->private_key_->Sign(
      algorithm, base::make_span(in, in_len),
      base::BindOnce(&SSLServerContextImpl::SocketImpl::OnPrivateKeyComplete,
                     weak_factory_.GetWeakPtr()));
  return ssl_private_key_retry;
}

ssl_private_key_result_t
SSLServerContextImpl::SocketImpl::PrivateKeyCompleteCallback(uint8_t* out,
                                                             size_t* out_len,
                                                             size_t max_out) {
  if (signature_result_ == ERR_IO_PENDING)
    return ssl_private_key_retry;
  if (signature_result_ != OK) {
    OpenSSLPutNetError(FROM_HERE, signature_result_);
    return ssl_private_key_failure;
  }
  if (signature_.size() > max_out) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
    return ssl_private_key_failure;
  }
  memcpy(out, signature_.data(), signature_.size());
  *out_len = signature_.size();
  signature_.clear();
  return ssl_private_key_success;
}

void SSLServerContextImpl::SocketImpl::OnPrivateKeyComplete(
    Error error,
    const std::vector<uint8_t>& signature) {
  DCHECK_EQ(ERR_IO_PENDING, signature_result_);
  DCHECK(signature_.empty());

  signature_result_ = error;
  if (signature_result_ == OK)
    signature_ = signature;
  DoHandshakeLoop(ERR_IO_PENDING);
}

int SSLServerContextImpl::SocketImpl::ALPNSelectCallback(SSL* ssl,
                                                         const uint8_t** out,
                                                         uint8_t* out_len,
                                                         const uint8_t* in,
                                                         unsigned in_len,
                                                         void* arg) {
  SSLServerContextImpl::SocketImpl* socket =
      static_cast<SSLServerContextImpl::SocketImpl*>(SSL_get_ex_data(
          ssl, SocketDataIndex::GetInstance()->ssl_socket_data_index_));

  // Iterate over the server protocols in preference order.
  for (NextProto server_proto :
       socket->context_->ssl_server_config_.alpn_protos) {
    const char* server_proto_str = NextProtoToString(server_proto);

    // See if the client advertised the corresponding protocol.
    CBS cbs;
    CBS_init(&cbs, in, in_len);
    while (CBS_len(&cbs) != 0) {
      CBS client_proto;
      if (!CBS_get_u8_length_prefixed(&cbs, &client_proto)) {
        return SSL_TLSEXT_ERR_NOACK;
      }
      if (base::StringPiece(
              reinterpret_cast<const char*>(CBS_data(&client_proto)),
              CBS_len(&client_proto)) == server_proto_str) {
        *out = CBS_data(&client_proto);
        *out_len = CBS_len(&client_proto);
        return SSL_TLSEXT_ERR_OK;
      }
    }
  }
  return SSL_TLSEXT_ERR_NOACK;
}

int SSLServerContextImpl::SocketImpl::Handshake(
    CompletionOnceCallback callback) {
  net_log_.BeginEvent(NetLogEventType::SSL_SERVER_HANDSHAKE);

  // Set up new ssl object.
  int rv = Init();
  if (rv != OK) {
    LOG(ERROR) << "Failed to initialize OpenSSL: rv=" << rv;
    net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE,
                                      rv);
    return rv;
  }

  // Set SSL to server mode. Handshake happens in the loop below.
  SSL_set_accept_state(ssl_.get());

  GotoState(STATE_HANDSHAKE);
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_handshake_callback_ = std::move(callback);
  } else {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE,
                                      rv);
  }

  return rv > OK ? OK : rv;
}

int SSLServerContextImpl::SocketImpl::ExportKeyingMaterial(
    const base::StringPiece& label,
    bool has_context,
    const base::StringPiece& context,
    unsigned char* out,
    unsigned int outlen) {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int rv = SSL_export_keying_material(
      ssl_.get(), out, outlen, label.data(), label.size(),
      reinterpret_cast<const unsigned char*>(context.data()), context.length(),
      context.length() > 0);

  if (rv != 1) {
    int ssl_error = SSL_get_error(ssl_.get(), rv);
    LOG(ERROR) << "Failed to export keying material;"
               << " returned " << rv << ", SSL error code " << ssl_error;
    return MapOpenSSLError(ssl_error, err_tracer);
  }
  return OK;
}

int SSLServerContextImpl::SocketImpl::Read(IOBuffer* buf,
                                           int buf_len,
                                           CompletionOnceCallback callback) {
  int rv = ReadIfReady(buf, buf_len, std::move(callback));
  if (rv == ERR_IO_PENDING) {
    user_read_buf_ = buf;
    user_read_buf_len_ = buf_len;
  }
  return rv;
}

int SSLServerContextImpl::SocketImpl::ReadIfReady(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback) {
  DCHECK(user_read_callback_.is_null());
  DCHECK(user_handshake_callback_.is_null());
  DCHECK(!user_read_buf_);
  DCHECK(!callback.is_null());
  DCHECK(completed_handshake_);

  int rv = DoPayloadRead(buf, buf_len);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = std::move(callback);
  }

  return rv;
}

int SSLServerContextImpl::SocketImpl::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(user_write_callback_.is_null());
  DCHECK(!user_write_buf_);
  DCHECK(!callback.is_null());

  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoPayloadWrite();

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = std::move(callback);
  } else {
    user_write_buf_ = nullptr;
    user_write_buf_len_ = 0;
  }
  return rv;
}

int SSLServerContextImpl::SocketImpl::SetReceiveBufferSize(int32_t size) {
  return transport_socket_->SetReceiveBufferSize(size);
}

int SSLServerContextImpl::SocketImpl::SetSendBufferSize(int32_t size) {
  return transport_socket_->SetSendBufferSize(size);
}

int SSLServerContextImpl::SocketImpl::Connect(CompletionOnceCallback callback) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

void SSLServerContextImpl::SocketImpl::Disconnect() {
  transport_socket_->Disconnect();
}

bool SSLServerContextImpl::SocketImpl::IsConnected() const {
  // TODO(wtc): Find out if we should check transport_socket_->IsConnected()
  // as well.
  return completed_handshake_;
}

bool SSLServerContextImpl::SocketImpl::IsConnectedAndIdle() const {
  return completed_handshake_ && transport_socket_->IsConnectedAndIdle();
}

int SSLServerContextImpl::SocketImpl::GetPeerAddress(
    IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  return transport_socket_->GetPeerAddress(address);
}

int SSLServerContextImpl::SocketImpl::GetLocalAddress(
    IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  return transport_socket_->GetLocalAddress(address);
}

const NetLogWithSource& SSLServerContextImpl::SocketImpl::NetLog() const {
  return net_log_;
}

bool SSLServerContextImpl::SocketImpl::WasEverUsed() const {
  return transport_socket_->WasEverUsed();
}

bool SSLServerContextImpl::SocketImpl::WasAlpnNegotiated() const {
  return negotiated_protocol_ != kProtoUnknown;
}

NextProto SSLServerContextImpl::SocketImpl::GetNegotiatedProtocol() const {
  return negotiated_protocol_;
}

bool SSLServerContextImpl::SocketImpl::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!completed_handshake_)
    return false;

  ssl_info->cert = client_cert_;

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_.get());
  CHECK(cipher);

  SSLConnectionStatusSetCipherSuite(
      static_cast<uint16_t>(SSL_CIPHER_get_id(cipher)),
      &ssl_info->connection_status);
  SSLConnectionStatusSetVersion(GetNetSSLVersion(ssl_.get()),
                                &ssl_info->connection_status);

  ssl_info->early_data_received = early_data_received_;
  ssl_info->handshake_type = SSL_session_reused(ssl_.get())
                                 ? SSLInfo::HANDSHAKE_RESUME
                                 : SSLInfo::HANDSHAKE_FULL;

  return true;
}

void SSLServerContextImpl::SocketImpl::GetConnectionAttempts(
    ConnectionAttempts* out) const {
  out->clear();
}

int64_t SSLServerContextImpl::SocketImpl::GetTotalReceivedBytes() const {
  return transport_socket_->GetTotalReceivedBytes();
}

void SSLServerContextImpl::SocketImpl::ApplySocketTag(const SocketTag& tag) {
  NOTIMPLEMENTED();
}

void SSLServerContextImpl::SocketImpl::OnReadReady() {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
    return;
  }

  // BoringSSL does not support renegotiation as a server, so the only other
  // operation blocked on Read is DoPayloadRead.
  if (!user_read_buf_) {
    if (!user_read_callback_.is_null()) {
      DoReadCallback(OK);
    }
    return;
  }

  int rv = DoPayloadRead(user_read_buf_.get(), user_read_buf_len_);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

void SSLServerContextImpl::SocketImpl::OnWriteReady() {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
    return;
  }

  // BoringSSL does not support renegotiation as a server, so the only other
  // operation blocked on Read is DoPayloadWrite.
  if (!user_write_buf_)
    return;

  int rv = DoPayloadWrite();
  if (rv != ERR_IO_PENDING)
    DoWriteCallback(rv);
}

void SSLServerContextImpl::SocketImpl::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv == ERR_IO_PENDING)
    return;

  net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_SERVER_HANDSHAKE, rv);
  if (!user_handshake_callback_.is_null())
    DoHandshakeCallback(rv);
}

int SSLServerContextImpl::SocketImpl::DoPayloadRead(IOBuffer* buf,
                                                    int buf_len) {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_read(ssl_.get(), buf->data(), buf_len);
  if (rv >= 0) {
    if (SSL_in_early_data(ssl_.get()))
      early_data_received_ = true;
    return rv;
  }
  int ssl_error = SSL_get_error(ssl_.get(), rv);
  OpenSSLErrorInfo error_info;
  int net_error =
      MapOpenSSLErrorWithDetails(ssl_error, err_tracer, &error_info);
  if (net_error != ERR_IO_PENDING) {
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, net_error,
                       ssl_error, error_info);
  }
  return net_error;
}

int SSLServerContextImpl::SocketImpl::DoPayloadWrite() {
  DCHECK(completed_handshake_);
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  DCHECK(user_write_buf_);

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_write(ssl_.get(), user_write_buf_->data(), user_write_buf_len_);
  if (rv >= 0)
    return rv;
  int ssl_error = SSL_get_error(ssl_.get(), rv);
  OpenSSLErrorInfo error_info;
  int net_error =
      MapOpenSSLErrorWithDetails(ssl_error, err_tracer, &error_info);
  if (net_error != ERR_IO_PENDING) {
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_WRITE_ERROR, net_error,
                       ssl_error, error_info);
  }
  return net_error;
}

int SSLServerContextImpl::SocketImpl::DoHandshakeLoop(int last_io_result) {
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLServerContextImpl::SocketImpl::DoHandshake() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int net_error = OK;
  int rv = SSL_do_handshake(ssl_.get());
  if (rv == 1) {
    completed_handshake_ = true;
    const STACK_OF(CRYPTO_BUFFER)* certs =
        SSL_get0_peer_certificates(ssl_.get());
    if (certs) {
      client_cert_ = x509_util::CreateX509CertificateFromBuffers(certs);
      if (!client_cert_)
        return ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT;
    }

    const uint8_t* alpn_proto = nullptr;
    unsigned alpn_len = 0;
    SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
    if (alpn_len > 0) {
      base::StringPiece proto(reinterpret_cast<const char*>(alpn_proto),
                              alpn_len);
      negotiated_protocol_ = NextProtoFromString(proto);
    }
  } else {
    int ssl_error = SSL_get_error(ssl_.get(), rv);

    if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
      DCHECK(context_->private_key_);
      GotoState(STATE_HANDSHAKE);
      return ERR_IO_PENDING;
    }

    OpenSSLErrorInfo error_info;
    net_error = MapOpenSSLErrorWithDetails(ssl_error, err_tracer, &error_info);

    // SSL_R_CERTIFICATE_VERIFY_FAILED's mapping is different between client and
    // server.
    if (ERR_GET_LIB(error_info.error_code) == ERR_LIB_SSL &&
        ERR_GET_REASON(error_info.error_code) ==
            SSL_R_CERTIFICATE_VERIFY_FAILED) {
      net_error = ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; returned " << rv << ", SSL error code "
                 << ssl_error << ", net_error " << net_error;
      NetLogOpenSSLError(net_log_, NetLogEventType::SSL_HANDSHAKE_ERROR,
                         net_error, ssl_error, error_info);
    }
  }
  return net_error;
}

void SSLServerContextImpl::SocketImpl::DoHandshakeCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  std::move(user_handshake_callback_).Run(rv > OK ? OK : rv);
}

void SSLServerContextImpl::SocketImpl::DoReadCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!user_read_callback_.is_null());

  user_read_buf_ = nullptr;
  user_read_buf_len_ = 0;
  std::move(user_read_callback_).Run(rv);
}

void SSLServerContextImpl::SocketImpl::DoWriteCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!user_write_callback_.is_null());

  user_write_buf_ = nullptr;
  user_write_buf_len_ = 0;
  std::move(user_write_callback_).Run(rv);
}

int SSLServerContextImpl::SocketImpl::Init() {
  static const int kBufferSize = 17 * 1024;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (!ssl_ ||
      !SSL_set_ex_data(ssl_.get(),
                       SocketDataIndex::GetInstance()->ssl_socket_data_index_,
                       this))
    return ERR_UNEXPECTED;

  // Set certificate and private key.
  if (context_->pkey_) {
    DCHECK(context_->cert_->cert_buffer());
    if (!SetSSLChainAndKey(ssl_.get(), context_->cert_.get(),
                           context_->pkey_.get(), nullptr)) {
      return ERR_UNEXPECTED;
    }
  } else {
    DCHECK(context_->private_key_);
    if (!SetSSLChainAndKey(ssl_.get(), context_->cert_.get(), nullptr,
                           &kPrivateKeyMethod)) {
      return ERR_UNEXPECTED;
    }
    std::vector<uint16_t> preferences =
        context_->private_key_->GetAlgorithmPreferences();
    SSL_set_signing_algorithm_prefs(ssl_.get(), preferences.data(),
                                    preferences.size());
  }

  if (context_->ssl_server_config_.signature_algorithm_for_testing
          .has_value()) {
    uint16_t id = *context_->ssl_server_config_.signature_algorithm_for_testing;
    CHECK(SSL_set_signing_algorithm_prefs(ssl_.get(), &id, 1));
  }

  transport_adapter_.reset(new SocketBIOAdapter(
      transport_socket_.get(), kBufferSize, kBufferSize, this));
  BIO* transport_bio = transport_adapter_->bio();

  BIO_up_ref(transport_bio);  // SSL_set0_rbio takes ownership.
  SSL_set0_rbio(ssl_.get(), transport_bio);

  BIO_up_ref(transport_bio);  // SSL_set0_wbio takes ownership.
  SSL_set0_wbio(ssl_.get(), transport_bio);

  return OK;
}

// static
ssl_verify_result_t SSLServerContextImpl::SocketImpl::CertVerifyCallback(
    SSL* ssl,
    uint8_t* out_alert) {
  SocketImpl* socket = reinterpret_cast<SocketImpl*>(SSL_get_app_data(ssl));
  return socket->CertVerifyCallbackImpl(out_alert);
}

ssl_verify_result_t SSLServerContextImpl::SocketImpl::CertVerifyCallbackImpl(
    uint8_t* out_alert) {
  ClientCertVerifier* verifier =
      context_->ssl_server_config_.client_cert_verifier;
  // If a verifier was not supplied, all certificates are accepted.
  if (!verifier)
    return ssl_verify_ok;

  scoped_refptr<X509Certificate> client_cert =
      x509_util::CreateX509CertificateFromBuffers(
          SSL_get0_peer_certificates(ssl_.get()));
  if (!client_cert) {
    *out_alert = SSL_AD_BAD_CERTIFICATE;
    return ssl_verify_invalid;
  }

  // TODO(davidben): Support asynchronous verifiers. http://crbug.com/347402
  std::unique_ptr<ClientCertVerifier::Request> ignore_async;
  int res = verifier->Verify(client_cert.get(), CompletionOnceCallback(),
                             &ignore_async);
  DCHECK_NE(res, ERR_IO_PENDING);

  if (res != OK) {
    // TODO(davidben): Map from certificate verification failure to alert.
    *out_alert = SSL_AD_CERTIFICATE_UNKNOWN;
    return ssl_verify_invalid;
  }
  return ssl_verify_ok;
}

std::unique_ptr<SSLServerContext> CreateSSLServerContext(
    X509Certificate* certificate,
    EVP_PKEY* pkey,
    const SSLServerConfig& ssl_server_config) {
  return std::make_unique<SSLServerContextImpl>(certificate, pkey,
                                                ssl_server_config);
}

std::unique_ptr<SSLServerContext> CreateSSLServerContext(
    X509Certificate* certificate,
    const crypto::RSAPrivateKey& key,
    const SSLServerConfig& ssl_server_config) {
  return std::make_unique<SSLServerContextImpl>(certificate, key.key(),
                                                ssl_server_config);
}

std::unique_ptr<SSLServerContext> CreateSSLServerContext(
    X509Certificate* certificate,
    scoped_refptr<SSLPrivateKey> key,
    const SSLServerConfig& ssl_config) {
  return std::make_unique<SSLServerContextImpl>(certificate, key, ssl_config);
}

SSLServerContextImpl::SSLServerContextImpl(
    X509Certificate* certificate,
    scoped_refptr<net::SSLPrivateKey> key,
    const SSLServerConfig& ssl_server_config)
    : ssl_server_config_(ssl_server_config),
      cert_(certificate),
      private_key_(key) {
  CHECK(private_key_);
  Init();
}

SSLServerContextImpl::SSLServerContextImpl(
    X509Certificate* certificate,
    EVP_PKEY* pkey,
    const SSLServerConfig& ssl_server_config)
    : ssl_server_config_(ssl_server_config), cert_(certificate) {
  CHECK(pkey);
  pkey_ = bssl::UpRef(pkey);
  Init();
}

void SSLServerContextImpl::Init() {
  crypto::EnsureOpenSSLInit();
  ssl_ctx_.reset(SSL_CTX_new(TLS_with_buffers_method()));
  SSL_CTX_set_session_cache_mode(ssl_ctx_.get(), SSL_SESS_CACHE_SERVER);
  uint8_t session_ctx_id = 0;
  SSL_CTX_set_session_id_context(ssl_ctx_.get(), &session_ctx_id,
                                 sizeof(session_ctx_id));
  // Deduplicate all certificates minted from the SSL_CTX in memory.
  SSL_CTX_set0_buffer_pool(ssl_ctx_.get(), x509_util::GetBufferPool());

  int verify_mode = 0;
  switch (ssl_server_config_.client_cert_type) {
    case SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT:
      verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
      FALLTHROUGH;
    case SSLServerConfig::ClientCertType::OPTIONAL_CLIENT_CERT:
      verify_mode |= SSL_VERIFY_PEER;
      SSL_CTX_set_custom_verify(ssl_ctx_.get(), verify_mode,
                                SocketImpl::CertVerifyCallback);
      break;
    case SSLServerConfig::ClientCertType::NO_CLIENT_CERT:
      break;
  }

  SSL_CTX_set_early_data_enabled(ssl_ctx_.get(),
                                 ssl_server_config_.early_data_enabled);
  DCHECK_LT(SSL3_VERSION, ssl_server_config_.version_min);
  DCHECK_LT(SSL3_VERSION, ssl_server_config_.version_max);
  CHECK(SSL_CTX_set_min_proto_version(ssl_ctx_.get(),
                                      ssl_server_config_.version_min));
  CHECK(SSL_CTX_set_max_proto_version(ssl_ctx_.get(),
                                      ssl_server_config_.version_max));

  // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
  // set everything we care about to an absolute value.
  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);

  SSL_CTX_set_options(ssl_ctx_.get(), options.set_mask);
  SSL_CTX_clear_options(ssl_ctx_.get(), options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);

  SSL_CTX_set_mode(ssl_ctx_.get(), mode.set_mask);
  SSL_CTX_clear_mode(ssl_ctx_.get(), mode.clear_mask);

  if (ssl_server_config_.cipher_suite_for_testing.has_value()) {
    const SSL_CIPHER* cipher =
        SSL_get_cipher_by_value(*ssl_server_config_.cipher_suite_for_testing);
    CHECK(cipher);
    CHECK(SSL_CTX_set_strict_cipher_list(ssl_ctx_.get(),
                                         SSL_CIPHER_get_name(cipher)));
  } else {
    // See SSLServerConfig::disabled_cipher_suites for description of the suites
    // disabled by default. Note that !SHA256 and !SHA384 only remove
    // HMAC-SHA256 and HMAC-SHA384 cipher suites, not GCM cipher suites with
    // SHA256 or SHA384 as the handshake hash.
    std::string command("DEFAULT:!AESGCM+AES256:!aPSK");

    // SSLPrivateKey only supports ECDHE-based ciphers because it lacks decrypt.
    if (ssl_server_config_.require_ecdhe || (!pkey_ && private_key_))
      command.append(":!kRSA");

    // Remove any disabled ciphers.
    for (uint16_t id : ssl_server_config_.disabled_cipher_suites) {
      const SSL_CIPHER* cipher = SSL_get_cipher_by_value(id);
      if (cipher) {
        command.append(":!");
        command.append(SSL_CIPHER_get_name(cipher));
      }
    }

    CHECK(SSL_CTX_set_strict_cipher_list(ssl_ctx_.get(), command.c_str()));
  }

  if (ssl_server_config_.client_cert_type !=
          SSLServerConfig::ClientCertType::NO_CLIENT_CERT &&
      !ssl_server_config_.cert_authorities.empty()) {
    bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> stack(sk_CRYPTO_BUFFER_new_null());
    for (const auto& authority : ssl_server_config_.cert_authorities) {
      sk_CRYPTO_BUFFER_push(stack.get(),
                            x509_util::CreateCryptoBuffer(authority).release());
    }
    SSL_CTX_set0_client_CAs(ssl_ctx_.get(), stack.release());
  }

  SSL_CTX_set_alpn_select_cb(ssl_ctx_.get(), &SocketImpl::ALPNSelectCallback,
                             nullptr);

  if (!ssl_server_config_.ocsp_response.empty()) {
    SSL_CTX_set_ocsp_response(ssl_ctx_.get(),
                              ssl_server_config_.ocsp_response.data(),
                              ssl_server_config_.ocsp_response.size());
  }
}

SSLServerContextImpl::~SSLServerContextImpl() = default;

std::unique_ptr<SSLServerSocket> SSLServerContextImpl::CreateSSLServerSocket(
    std::unique_ptr<StreamSocket> socket) {
  return std::make_unique<SocketImpl>(this, std::move(socket));
}

}  // namespace net
