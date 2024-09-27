// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/socket/ssl_client_socket_impl.h"

#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/ssl/cert_compression.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_handshake_details.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_key_logger.h"
#include "net/ssl/ssl_private_key.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/err.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kSSLClientSocketNoPendingResult = 1;
// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kCertVerifyPending = 1;

// Default size of the internal BoringSSL buffers.
const int kDefaultOpenSSLBufferSize = 17 * 1024;

base::Value::Dict NetLogPrivateKeyOperationParams(uint16_t algorithm,
                                                  SSLPrivateKey* key) {
  return base::Value::Dict()
      .Set("algorithm",
           SSL_get_signature_algorithm_name(algorithm, 0 /* exclude curve */))
      .Set("provider", key->GetProviderName());
}

base::Value::Dict NetLogSSLInfoParams(SSLClientSocketImpl* socket) {
  SSLInfo ssl_info;
  if (!socket->GetSSLInfo(&ssl_info)) {
    return base::Value::Dict();
  }

  const char* version_str;
  SSLVersionToString(&version_str,
                     SSLConnectionStatusToVersion(ssl_info.connection_status));
  return base::Value::Dict()
      .Set("version", version_str)
      .Set("is_resumed", ssl_info.handshake_type == SSLInfo::HANDSHAKE_RESUME)
      .Set("cipher_suite",
           SSLConnectionStatusToCipherSuite(ssl_info.connection_status))
      .Set("key_exchange_group", ssl_info.key_exchange_group)
      .Set("peer_signature_algorithm", ssl_info.peer_signature_algorithm)
      .Set("encrypted_client_hello", ssl_info.encrypted_client_hello)
      .Set("next_proto", NextProtoToString(socket->GetNegotiatedProtocol()));
}

base::Value::Dict NetLogSSLAlertParams(const void* bytes, size_t len) {
  return base::Value::Dict().Set("bytes", NetLogBinaryValue(bytes, len));
}

base::Value::Dict NetLogSSLMessageParams(bool is_write,
                                         const void* bytes,
                                         size_t len,
                                         NetLogCaptureMode capture_mode) {
  if (len == 0) {
    NOTREACHED_IN_MIGRATION();
    return base::Value::Dict();
  }

  base::Value::Dict dict;
  // The handshake message type is the first byte. Include it so elided messages
  // still report their type.
  uint8_t type = reinterpret_cast<const uint8_t*>(bytes)[0];
  dict.Set("type", type);

  // Elide client certificate messages unless logging socket bytes. The client
  // certificate does not contain information needed to impersonate the user
  // (that's the private key which isn't sent over the wire), but it may contain
  // information on the user's identity.
  if (!is_write || type != SSL3_MT_CERTIFICATE ||
      NetLogCaptureIncludesSocketBytes(capture_mode)) {
    dict.Set("bytes", NetLogBinaryValue(bytes, len));
  }

  return dict;
}

bool HostIsIPAddressNoBrackets(std::string_view host) {
  // Note this cannot directly call url::HostIsIPAddress, because that function
  // expects bracketed IPv6 literals. By the time hosts reach SSLClientSocket,
  // brackets have been removed.
  IPAddress unused;
  return unused.AssignFromIPLiteral(host);
}

}  // namespace

class SSLClientSocketImpl::SSLContext {
 public:
  static SSLContext* GetInstance() {
    return base::Singleton<SSLContext,
                           base::LeakySingletonTraits<SSLContext>>::get();
  }
  SSL_CTX* ssl_ctx() { return ssl_ctx_.get(); }

  SSLClientSocketImpl* GetClientSocketFromSSL(const SSL* ssl) {
    DCHECK(ssl);
    SSLClientSocketImpl* socket = static_cast<SSLClientSocketImpl*>(
        SSL_get_ex_data(ssl, ssl_socket_data_index_));
    DCHECK(socket);
    return socket;
  }

  bool SetClientSocketForSSL(SSL* ssl, SSLClientSocketImpl* socket) {
    return SSL_set_ex_data(ssl, ssl_socket_data_index_, socket) != 0;
  }

  void SetSSLKeyLogger(std::unique_ptr<SSLKeyLogger> logger) {
    net::SSLKeyLoggerManager::SetSSLKeyLogger(std::move(logger));
    SSL_CTX_set_keylog_callback(ssl_ctx_.get(),
                                SSLKeyLoggerManager::KeyLogCallback);
  }

  static const SSL_PRIVATE_KEY_METHOD kPrivateKeyMethod;

 private:
  friend struct base::DefaultSingletonTraits<SSLContext>;

  SSLContext() {
    ssl_socket_data_index_ =
        SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
    DCHECK_NE(ssl_socket_data_index_, -1);
    ssl_ctx_.reset(SSL_CTX_new(TLS_with_buffers_method()));
    SSL_CTX_set_cert_cb(ssl_ctx_.get(), ClientCertRequestCallback, nullptr);

    // Verifies the server certificate even on resumed sessions.
    SSL_CTX_set_reverify_on_resume(ssl_ctx_.get(), 1);
    SSL_CTX_set_custom_verify(ssl_ctx_.get(), SSL_VERIFY_PEER,
                              VerifyCertCallback);
    // Disable the internal session cache. Session caching is handled
    // externally (i.e. by SSLClientSessionCache).
    SSL_CTX_set_session_cache_mode(
        ssl_ctx_.get(), SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL);
    SSL_CTX_sess_set_new_cb(ssl_ctx_.get(), NewSessionCallback);
    SSL_CTX_set_timeout(ssl_ctx_.get(), 1 * 60 * 60 /* one hour */);

    SSL_CTX_set_grease_enabled(ssl_ctx_.get(), 1);

    // Deduplicate all certificates minted from the SSL_CTX in memory.
    SSL_CTX_set0_buffer_pool(ssl_ctx_.get(), x509_util::GetBufferPool());

    SSL_CTX_set_msg_callback(ssl_ctx_.get(), MessageCallback);

    ConfigureCertificateCompression(ssl_ctx_.get());
  }

  static int ClientCertRequestCallback(SSL* ssl, void* arg) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    DCHECK(socket);
    return socket->ClientCertRequestCallback(ssl);
  }

  static int NewSessionCallback(SSL* ssl, SSL_SESSION* session) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->NewSessionCallback(session);
  }

  static ssl_private_key_result_t PrivateKeySignCallback(SSL* ssl,
                                                         uint8_t* out,
                                                         size_t* out_len,
                                                         size_t max_out,
                                                         uint16_t algorithm,
                                                         const uint8_t* in,
                                                         size_t in_len) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->PrivateKeySignCallback(out, out_len, max_out, algorithm, in,
                                          in_len);
  }

  static ssl_private_key_result_t PrivateKeyCompleteCallback(SSL* ssl,
                                                             uint8_t* out,
                                                             size_t* out_len,
                                                             size_t max_out) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->PrivateKeyCompleteCallback(out, out_len, max_out);
  }

  static void MessageCallback(int is_write,
                              int version,
                              int content_type,
                              const void* buf,
                              size_t len,
                              SSL* ssl,
                              void* arg) {
    SSLClientSocketImpl* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->MessageCallback(is_write, content_type, buf, len);
  }

  // This is the index used with SSL_get_ex_data to retrieve the owner
  // SSLClientSocketImpl object from an SSL instance.
  int ssl_socket_data_index_;

  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
};

const SSL_PRIVATE_KEY_METHOD
    SSLClientSocketImpl::SSLContext::kPrivateKeyMethod = {
        &SSLClientSocketImpl::SSLContext::PrivateKeySignCallback,
        nullptr /* decrypt */,
        &SSLClientSocketImpl::SSLContext::PrivateKeyCompleteCallback,
};

SSLClientSocketImpl::SSLClientSocketImpl(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config)
    : pending_read_error_(kSSLClientSocketNoPendingResult),
      context_(context),
      cert_verification_result_(kCertVerifyPending),
      stream_socket_(std::move(stream_socket)),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      signature_result_(kSSLClientSocketNoPendingResult),
      net_log_(stream_socket_->NetLog()) {
  CHECK(context_);
}

SSLClientSocketImpl::~SSLClientSocketImpl() {
  Disconnect();
}

void SSLClientSocketImpl::SetSSLKeyLogger(
    std::unique_ptr<SSLKeyLogger> logger) {
  SSLContext::GetInstance()->SetSSLKeyLogger(std::move(logger));
}

std::vector<uint8_t> SSLClientSocketImpl::GetECHRetryConfigs() {
  const uint8_t* retry_configs;
  size_t retry_configs_len;
  SSL_get0_ech_retry_configs(ssl_.get(), &retry_configs, &retry_configs_len);
  return std::vector<uint8_t>(retry_configs, retry_configs + retry_configs_len);
}

int SSLClientSocketImpl::ExportKeyingMaterial(std::string_view label,
                                              bool has_context,
                                              std::string_view context,
                                              unsigned char* out,
                                              unsigned int outlen) {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (!SSL_export_keying_material(
          ssl_.get(), out, outlen, label.data(), label.size(),
          reinterpret_cast<const unsigned char*>(context.data()),
          context.length(), has_context ? 1 : 0)) {
    LOG(ERROR) << "Failed to export keying material.";
    return ERR_FAILED;
  }

  return OK;
}

int SSLClientSocketImpl::Connect(CompletionOnceCallback callback) {
  // Although StreamSocket does allow calling Connect() after Disconnect(),
  // this has never worked for layered sockets. CHECK to detect any consumers
  // reconnecting an SSL socket.
  //
  // TODO(davidben,mmenke): Remove this API feature. See
  // https://crbug.com/499289.
  CHECK(!disconnected_);

  net_log_.BeginEvent(NetLogEventType::SSL_CONNECT);

  // Set up new ssl object.
  int rv = Init();
  if (rv != OK) {
    LogConnectEndEvent(rv);
    return rv;
  }

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_.get());

  next_handshake_state_ = STATE_HANDSHAKE;
  rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = std::move(callback);
  } else {
    LogConnectEndEvent(rv);
  }

  return rv > OK ? OK : rv;
}

void SSLClientSocketImpl::Disconnect() {
  disconnected_ = true;

  // Shut down anything that may call us back.
  cert_verifier_request_.reset();
  weak_factory_.InvalidateWeakPtrs();
  transport_adapter_.reset();

  // Release user callbacks.
  user_connect_callback_.Reset();
  user_read_callback_.Reset();
  user_write_callback_.Reset();
  user_read_buf_ = nullptr;
  user_read_buf_len_ = 0;
  user_write_buf_ = nullptr;
  user_write_buf_len_ = 0;

  stream_socket_->Disconnect();
}

// ConfirmHandshake may only be called on a connected socket and, like other
// socket methods, there may only be one ConfirmHandshake operation in progress
// at once.
int SSLClientSocketImpl::ConfirmHandshake(CompletionOnceCallback callback) {
  CHECK(completed_connect_);
  CHECK(!in_confirm_handshake_);
  if (!SSL_in_early_data(ssl_.get())) {
    return OK;
  }

  net_log_.BeginEvent(NetLogEventType::SSL_CONFIRM_HANDSHAKE);
  next_handshake_state_ = STATE_HANDSHAKE;
  in_confirm_handshake_ = true;
  int rv = DoHandshakeLoop(OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = std::move(callback);
  } else {
    net_log_.EndEvent(NetLogEventType::SSL_CONFIRM_HANDSHAKE);
    in_confirm_handshake_ = false;
  }

  return rv > OK ? OK : rv;
}

bool SSLClientSocketImpl::IsConnected() const {
  // If the handshake has not yet completed or the socket has been explicitly
  // disconnected.
  if (!completed_connect_ || disconnected_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return true;

  return stream_socket_->IsConnected();
}

bool SSLClientSocketImpl::IsConnectedAndIdle() const {
  // If the handshake has not yet completed or the socket has been explicitly
  // disconnected.
  if (!completed_connect_ || disconnected_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return false;

  // If there is data read from the network that has not yet been consumed, do
  // not treat the connection as idle.
  //
  // Note that this does not check whether there is ciphertext that has not yet
  // been flushed to the network. |Write| returns early, so this can cause race
  // conditions which cause a socket to not be treated reusable when it should
  // be. See https://crbug.com/466147.
  if (transport_adapter_->HasPendingReadData())
    return false;

  return stream_socket_->IsConnectedAndIdle();
}

int SSLClientSocketImpl::GetPeerAddress(IPEndPoint* addressList) const {
  return stream_socket_->GetPeerAddress(addressList);
}

int SSLClientSocketImpl::GetLocalAddress(IPEndPoint* addressList) const {
  return stream_socket_->GetLocalAddress(addressList);
}

const NetLogWithSource& SSLClientSocketImpl::NetLog() const {
  return net_log_;
}

bool SSLClientSocketImpl::WasEverUsed() const {
  return was_ever_used_;
}

NextProto SSLClientSocketImpl::GetNegotiatedProtocol() const {
  return negotiated_protocol_;
}

std::optional<std::string_view>
SSLClientSocketImpl::GetPeerApplicationSettings() const {
  if (!SSL_has_application_settings(ssl_.get())) {
    return std::nullopt;
  }

  const uint8_t* out_data;
  size_t out_len;
  SSL_get0_peer_application_settings(ssl_.get(), &out_data, &out_len);
  return std::string_view{reinterpret_cast<const char*>(out_data), out_len};
}

bool SSLClientSocketImpl::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!server_cert_)
    return false;

  ssl_info->cert = server_cert_verify_result_.verified_cert;
  ssl_info->unverified_cert = server_cert_;
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  ssl_info->is_issued_by_known_root =
      server_cert_verify_result_.is_issued_by_known_root;
  ssl_info->pkp_bypassed = pkp_bypassed_;
  ssl_info->public_key_hashes = server_cert_verify_result_.public_key_hashes;
  ssl_info->client_cert_sent = send_client_cert_ && client_cert_.get();
  ssl_info->encrypted_client_hello = SSL_ech_accepted(ssl_.get());
  ssl_info->ocsp_result = server_cert_verify_result_.ocsp_result;
  ssl_info->is_fatal_cert_error = is_fatal_cert_error_;
  ssl_info->signed_certificate_timestamps = server_cert_verify_result_.scts;
  ssl_info->ct_policy_compliance = server_cert_verify_result_.policy_compliance;

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_.get());
  CHECK(cipher);
  // Historically, the "group" was known as "curve".
  ssl_info->key_exchange_group = SSL_get_curve_id(ssl_.get());
  ssl_info->peer_signature_algorithm =
      SSL_get_peer_signature_algorithm(ssl_.get());

  SSLConnectionStatusSetCipherSuite(SSL_CIPHER_get_protocol_id(cipher),
                                    &ssl_info->connection_status);
  SSLConnectionStatusSetVersion(GetNetSSLVersion(ssl_.get()),
                                &ssl_info->connection_status);

  ssl_info->handshake_type = SSL_session_reused(ssl_.get())
                                 ? SSLInfo::HANDSHAKE_RESUME
                                 : SSLInfo::HANDSHAKE_FULL;

  return true;
}

int64_t SSLClientSocketImpl::GetTotalReceivedBytes() const {
  return stream_socket_->GetTotalReceivedBytes();
}

void SSLClientSocketImpl::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  if (!ssl_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  cert_request_info->host_and_port = host_and_port_;

  cert_request_info->cert_authorities.clear();
  const STACK_OF(CRYPTO_BUFFER)* authorities =
      SSL_get0_server_requested_CAs(ssl_.get());
  for (const CRYPTO_BUFFER* ca_name : authorities) {
    cert_request_info->cert_authorities.emplace_back(
        reinterpret_cast<const char*>(CRYPTO_BUFFER_data(ca_name)),
        CRYPTO_BUFFER_len(ca_name));
  }

  const uint16_t* algorithms;
  size_t num_algorithms =
      SSL_get0_peer_verify_algorithms(ssl_.get(), &algorithms);
  cert_request_info->signature_algorithms.assign(algorithms,
                                                 algorithms + num_algorithms);
}

void SSLClientSocketImpl::ApplySocketTag(const SocketTag& tag) {
  return stream_socket_->ApplySocketTag(tag);
}

int SSLClientSocketImpl::Read(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  int rv = ReadIfReady(buf, buf_len, std::move(callback));
  if (rv == ERR_IO_PENDING) {
    user_read_buf_ = buf;
    user_read_buf_len_ = buf_len;
  }
  return rv;
}

int SSLClientSocketImpl::ReadIfReady(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  int rv = DoPayloadRead(buf, buf_len);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = std::move(callback);
  } else {
    if (rv > 0)
      was_ever_used_ = true;
  }
  return rv;
}

int SSLClientSocketImpl::CancelReadIfReady() {
  DCHECK(user_read_callback_);
  DCHECK(!user_read_buf_);

  // Cancel |user_read_callback_|, because caller does not expect the callback
  // to be invoked after they have canceled the ReadIfReady.
  //
  // We do not pass the signal on to |stream_socket_| or |transport_adapter_|.
  // Multiple operations may be waiting on a transport ReadIfReady().
  // Conversely, an SSL ReadIfReady() may be blocked on something other than a
  // transport ReadIfReady(). Instead, the underlying transport ReadIfReady()
  // will continue running (with no underlying buffer). When it completes, it
  // will signal OnReadReady(), which will notice there is no read operation to
  // progress and skip it.
  user_read_callback_.Reset();
  return OK;
}

int SSLClientSocketImpl::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoPayloadWrite();

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = std::move(callback);
  } else {
    if (rv > 0) {
      CHECK_LE(rv, buf_len);
      was_ever_used_ = true;
    }
    user_write_buf_ = nullptr;
    user_write_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketImpl::SetReceiveBufferSize(int32_t size) {
  return stream_socket_->SetReceiveBufferSize(size);
}

int SSLClientSocketImpl::SetSendBufferSize(int32_t size) {
  return stream_socket_->SetSendBufferSize(size);
}

void SSLClientSocketImpl::OnReadReady() {
  // During a renegotiation, either Read or Write calls may be blocked on a
  // transport read.
  RetryAllOperations();
}

void SSLClientSocketImpl::OnWriteReady() {
  // During a renegotiation, either Read or Write calls may be blocked on a
  // transport read.
  RetryAllOperations();
}

int SSLClientSocketImpl::Init() {
  DCHECK(!ssl_);

  SSLContext* context = SSLContext::GetInstance();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ssl_.reset(SSL_new(context->ssl_ctx()));
  if (!ssl_ || !context->SetClientSocketForSSL(ssl_.get(), this))
    return ERR_UNEXPECTED;

  const bool host_is_ip_address =
      HostIsIPAddressNoBrackets(host_and_port_.host());

  // SNI should only contain valid DNS hostnames, not IP addresses (see RFC
  // 6066, Section 3).
  //
  // TODO(rsleevi): Should this code allow hostnames that violate the LDH rule?
  // See https://crbug.com/496472 and https://crbug.com/496468 for discussion.
  if (!host_is_ip_address &&
      !SSL_set_tlsext_host_name(ssl_.get(), host_and_port_.host().c_str())) {
    return ERR_UNEXPECTED;
  }

  if (context_->config().PostQuantumKeyAgreementEnabled()) {
    const uint16_t postquantum_group =
        base::FeatureList::IsEnabled(features::kUseMLKEM)
            ? SSL_GROUP_X25519_MLKEM768
            : SSL_GROUP_X25519_KYBER768_DRAFT00;
    const uint16_t kGroups[] = {postquantum_group, SSL_GROUP_X25519,
                                SSL_GROUP_SECP256R1, SSL_GROUP_SECP384R1};
    if (!SSL_set1_group_ids(ssl_.get(), kGroups, std::size(kGroups))) {
      return ERR_UNEXPECTED;
    }
  }

  if (IsCachingEnabled()) {
    bssl::UniquePtr<SSL_SESSION> session =
        context_->ssl_client_session_cache()->Lookup(
            GetSessionCacheKey(/*dest_ip_addr=*/std::nullopt));
    if (!session) {
      // If a previous session negotiated an RSA cipher suite then it may have
      // been inserted into the cache keyed by both hostname and resolved IP
      // address. See https://crbug.com/969684.
      IPEndPoint peer_address;
      if (stream_socket_->GetPeerAddress(&peer_address) == OK) {
        session = context_->ssl_client_session_cache()->Lookup(
            GetSessionCacheKey(peer_address.address()));
      }
    }
    if (session)
      SSL_set_session(ssl_.get(), session.get());
  }

  transport_adapter_ = std::make_unique<SocketBIOAdapter>(
      stream_socket_.get(), kDefaultOpenSSLBufferSize,
      kDefaultOpenSSLBufferSize, this);
  BIO* transport_bio = transport_adapter_->bio();

  BIO_up_ref(transport_bio);  // SSL_set0_rbio takes ownership.
  SSL_set0_rbio(ssl_.get(), transport_bio);

  BIO_up_ref(transport_bio);  // SSL_set0_wbio takes ownership.
  SSL_set0_wbio(ssl_.get(), transport_bio);

  uint16_t version_min =
      ssl_config_.version_min_override.value_or(context_->config().version_min);
  uint16_t version_max =
      ssl_config_.version_max_override.value_or(context_->config().version_max);
  if (version_min < TLS1_2_VERSION || version_max < TLS1_2_VERSION) {
    // TLS versions before TLS 1.2 are no longer supported.
    return ERR_UNEXPECTED;
  }

  if (!SSL_set_min_proto_version(ssl_.get(), version_min) ||
      !SSL_set_max_proto_version(ssl_.get(), version_max)) {
    return ERR_UNEXPECTED;
  }

  SSL_set_early_data_enabled(ssl_.get(), ssl_config_.early_data_enabled);

  // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
  // set everything we care about to an absolute value.
  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);

  // TODO(joth): Set this conditionally, see http://crbug.com/55410
  options.ConfigureFlag(SSL_OP_LEGACY_SERVER_CONNECT, true);

  SSL_set_options(ssl_.get(), options.set_mask);
  SSL_clear_options(ssl_.get(), options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);
  mode.ConfigureFlag(SSL_MODE_CBC_RECORD_SPLITTING, true);

  mode.ConfigureFlag(SSL_MODE_ENABLE_FALSE_START, true);

  SSL_set_mode(ssl_.get(), mode.set_mask);
  SSL_clear_mode(ssl_.get(), mode.clear_mask);

  // Use BoringSSL defaults, but disable 3DES and HMAC-SHA1 ciphers in ECDSA.
  // These are the remaining CBC-mode ECDSA ciphers.
  std::string command("ALL:!aPSK:!ECDSA+SHA1:!3DES");

  if (ssl_config_.require_ecdhe)
    command.append(":!kRSA");

  // Remove any disabled ciphers.
  for (uint16_t id : context_->config().disabled_cipher_suites) {
    const SSL_CIPHER* cipher = SSL_get_cipher_by_value(id);
    if (cipher) {
      command.append(":!");
      command.append(SSL_CIPHER_get_name(cipher));
    }
  }

  if (!SSL_set_strict_cipher_list(ssl_.get(), command.c_str())) {
    LOG(ERROR) << "SSL_set_cipher_list('" << command << "') failed";
    return ERR_UNEXPECTED;
  }

  // Disable SHA-1 server signatures.
  // TODO(crbug.com/boringssl/699): Once the default is flipped in BoringSSL, we
  // no longer need to override it.
  static const uint16_t kVerifyPrefs[] = {
      SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_RSA_PSS_RSAE_SHA256,
      SSL_SIGN_RSA_PKCS1_SHA256,       SSL_SIGN_ECDSA_SECP384R1_SHA384,
      SSL_SIGN_RSA_PSS_RSAE_SHA384,    SSL_SIGN_RSA_PKCS1_SHA384,
      SSL_SIGN_RSA_PSS_RSAE_SHA512,    SSL_SIGN_RSA_PKCS1_SHA512,
  };
  if (!SSL_set_verify_algorithm_prefs(ssl_.get(), kVerifyPrefs,
                                      std::size(kVerifyPrefs))) {
    return ERR_UNEXPECTED;
  }

  SSL_set_alps_use_new_codepoint(
      ssl_.get(),
      base::FeatureList::IsEnabled(features::kUseNewAlpsCodepointHttp2));

  if (!ssl_config_.alpn_protos.empty()) {
    std::vector<uint8_t> wire_protos =
        SerializeNextProtos(ssl_config_.alpn_protos);
    SSL_set_alpn_protos(ssl_.get(), wire_protos.data(), wire_protos.size());

    for (NextProto proto : ssl_config_.alpn_protos) {
      auto iter = ssl_config_.application_settings.find(proto);
      if (iter != ssl_config_.application_settings.end()) {
        const char* proto_string = NextProtoToString(proto);
        if (!SSL_add_application_settings(
                ssl_.get(), reinterpret_cast<const uint8_t*>(proto_string),
                strlen(proto_string), iter->second.data(),
                iter->second.size())) {
          return ERR_UNEXPECTED;
        }
      }
    }
  }

  SSL_enable_signed_cert_timestamps(ssl_.get());
  SSL_enable_ocsp_stapling(ssl_.get());

  // Configure BoringSSL to allow renegotiations. Once the initial handshake
  // completes, if renegotiations are not allowed, the default reject value will
  // be restored. This is done in this order to permit a BoringSSL
  // optimization. See https://crbug.com/boringssl/123. Use
  // ssl_renegotiate_explicit rather than ssl_renegotiate_freely so DoPeek()
  // does not trigger renegotiations.
  SSL_set_renegotiate_mode(ssl_.get(), ssl_renegotiate_explicit);

  SSL_set_shed_handshake_config(ssl_.get(), 1);

  // TODO(crbug.com/40089326), if |ssl_config_.privacy_mode| is enabled,
  // this should always continue with no client certificate.
  if (ssl_config_.privacy_mode == PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS) {
    send_client_cert_ = true;
  } else {
    send_client_cert_ = context_->GetClientCertificate(
        host_and_port_, &client_cert_, &client_private_key_);
  }

  if (context_->config().ech_enabled) {
    // TODO(crbug.com/41482204): Enable this unconditionally.
    SSL_set_enable_ech_grease(ssl_.get(), 1);
  }
  if (!ssl_config_.ech_config_list.empty()) {
    DCHECK(context_->config().ech_enabled);
    net_log_.AddEvent(NetLogEventType::SSL_ECH_CONFIG_LIST, [&] {
      return base::Value::Dict().Set(
          "bytes", NetLogBinaryValue(ssl_config_.ech_config_list));
    });
    if (!SSL_set1_ech_config_list(ssl_.get(),
                                  ssl_config_.ech_config_list.data(),
                                  ssl_config_.ech_config_list.size())) {
      return ERR_INVALID_ECH_CONFIG_LIST;
    }
  }

  SSL_set_permute_extensions(ssl_.get(), 1);

  return OK;
}

void SSLClientSocketImpl::DoReadCallback(int rv) {
  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  if (rv > 0)
    was_ever_used_ = true;
  user_read_buf_ = nullptr;
  user_read_buf_len_ = 0;
  std::move(user_read_callback_).Run(rv);
}

void SSLClientSocketImpl::DoWriteCallback(int rv) {
  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  if (rv > 0)
    was_ever_used_ = true;
  user_write_buf_ = nullptr;
  user_write_buf_len_ = 0;
  std::move(user_write_callback_).Run(rv);
}

int SSLClientSocketImpl::DoHandshake() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int rv = SSL_do_handshake(ssl_.get());
  int net_error = OK;
  if (rv <= 0) {
    int ssl_error = SSL_get_error(ssl_.get(), rv);
    if (ssl_error == SSL_ERROR_WANT_X509_LOOKUP && !send_client_cert_) {
      return ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    }
    if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
      DCHECK(client_private_key_);
      DCHECK_NE(kSSLClientSocketNoPendingResult, signature_result_);
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }
    if (ssl_error == SSL_ERROR_WANT_CERTIFICATE_VERIFY) {
      DCHECK(cert_verifier_request_);
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }

    OpenSSLErrorInfo error_info;
    net_error = MapLastOpenSSLError(ssl_error, err_tracer, &error_info);
    if (net_error == ERR_IO_PENDING) {
      // If not done, stay in this state
      next_handshake_state_ = STATE_HANDSHAKE;
      return ERR_IO_PENDING;
    }

    LOG(ERROR) << "handshake failed; returned " << rv << ", SSL error code "
               << ssl_error << ", net_error " << net_error;
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_HANDSHAKE_ERROR,
                       net_error, ssl_error, error_info);
  }

  next_handshake_state_ = STATE_HANDSHAKE_COMPLETE;
  return net_error;
}

int SSLClientSocketImpl::DoHandshakeComplete(int result) {
  if (result < 0)
    return result;

  if (in_confirm_handshake_) {
    next_handshake_state_ = STATE_NONE;
    return OK;
  }

  // If ECH overrode certificate verification to authenticate a fallback, using
  // the socket for application data would bypass server authentication.
  // BoringSSL will never complete the handshake in this case, so this should
  // not happen.
  CHECK(!used_ech_name_override_);

  const uint8_t* alpn_proto = nullptr;
  unsigned alpn_len = 0;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
  if (alpn_len > 0) {
    std::string_view proto(reinterpret_cast<const char*>(alpn_proto), alpn_len);
    negotiated_protocol_ = NextProtoFromString(proto);
  }

  RecordNegotiatedProtocol();

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_.get(), &ocsp_response_raw, &ocsp_response_len);
  set_stapled_ocsp_response_received(ocsp_response_len != 0);

  const uint8_t* sct_list;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_.get(), &sct_list, &sct_list_len);
  set_signed_cert_timestamps_received(sct_list_len != 0);

  if (!IsRenegotiationAllowed())
    SSL_set_renegotiate_mode(ssl_.get(), ssl_renegotiate_never);

  uint16_t signature_algorithm = SSL_get_peer_signature_algorithm(ssl_.get());
  if (signature_algorithm != 0) {
    base::UmaHistogramSparse("Net.SSLSignatureAlgorithm", signature_algorithm);
  }

  SSLInfo ssl_info;
  bool ok = GetSSLInfo(&ssl_info);
  // Ensure the verify callback was called, and got far enough to fill
  // in server_cert_.
  CHECK(ok);

  SSLHandshakeDetails details;
  if (SSL_version(ssl_.get()) < TLS1_3_VERSION) {
    if (SSL_session_reused(ssl_.get())) {
      details = SSLHandshakeDetails::kTLS12Resume;
    } else if (SSL_in_false_start(ssl_.get())) {
      details = SSLHandshakeDetails::kTLS12FalseStart;
    } else {
      details = SSLHandshakeDetails::kTLS12Full;
    }
  } else {
    bool used_hello_retry_request = SSL_used_hello_retry_request(ssl_.get());
    if (SSL_in_early_data(ssl_.get())) {
      DCHECK(!used_hello_retry_request);
      details = SSLHandshakeDetails::kTLS13Early;
    } else if (SSL_session_reused(ssl_.get())) {
      details = used_hello_retry_request
                    ? SSLHandshakeDetails::kTLS13ResumeWithHelloRetryRequest
                    : SSLHandshakeDetails::kTLS13Resume;
    } else {
      details = used_hello_retry_request
                    ? SSLHandshakeDetails::kTLS13FullWithHelloRetryRequest
                    : SSLHandshakeDetails::kTLS13Full;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.SSLHandshakeDetails", details);

  // Measure TLS connections that implement the renegotiation_info and EMS
  // extensions. TLS 1.3 is already patched for both bugs, so these functions
  // record true in TLS 1.3 although the extensions are not actually negotiated.
  // See https://crbug.com/850800.
  base::UmaHistogramBoolean("Net.SSLRenegotiationInfoSupported",
                            SSL_get_secure_renegotiation_support(ssl_.get()));
  base::UmaHistogramBoolean("Net.SSLExtendedMainSecretSupported",
                            SSL_get_extms_support(ssl_.get()));

  completed_connect_ = true;
  next_handshake_state_ = STATE_NONE;

  // Read from the transport immediately after the handshake, whether Read() is
  // called immediately or not. This serves several purposes:
  //
  // First, if this socket is preconnected and negotiates 0-RTT, the ServerHello
  // will not be processed. See https://crbug.com/950706
  //
  // Second, in False Start and TLS 1.3, the tickets arrive after immediately
  // after the handshake. This allows preconnected sockets to process the
  // tickets sooner. This also avoids a theoretical deadlock if the tickets are
  // too large. See
  // https://boringssl-review.googlesource.com/c/boringssl/+/34948.
  //
  // TODO(crbug.com/41456237): It is also a step in making TLS 1.3 client
  // certificate alerts less unreliable.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SSLClientSocketImpl::DoPeek, weak_factory_.GetWeakPtr()));

  return OK;
}

ssl_verify_result_t SSLClientSocketImpl::VerifyCertCallback(
    SSL* ssl,
    uint8_t* out_alert) {
  SSLClientSocketImpl* socket =
      SSLContext::GetInstance()->GetClientSocketFromSSL(ssl);
  DCHECK(socket);
  return socket->VerifyCert();
}

// This function is called by BoringSSL, so it has to return an
// ssl_verify_result_t. When specific //net errors need to be
// returned, use OpenSSLPutNetError to add them directly to the
// OpenSSL error queue.
ssl_verify_result_t SSLClientSocketImpl::VerifyCert() {
  if (cert_verification_result_ != kCertVerifyPending) {
    // The certificate verifier updates cert_verification_result_ when
    // it returns asynchronously. If there is a result in
    // cert_verification_result_, return it instead of triggering
    // another verify.
    return HandleVerifyResult();
  }

  // In this configuration, BoringSSL will perform exactly one certificate
  // verification, so there cannot be state from a previous verification.
  CHECK(!server_cert_);
  server_cert_ = x509_util::CreateX509CertificateFromBuffers(
      SSL_get0_peer_certificates(ssl_.get()));

  // OpenSSL decoded the certificate, but the X509Certificate implementation
  // could not. This is treated as a fatal SSL-level protocol error rather than
  // a certificate error. See https://crbug.com/91341.
  if (!server_cert_) {
    OpenSSLPutNetError(FROM_HERE, ERR_SSL_SERVER_CERT_BAD_FORMAT);
    return ssl_verify_invalid;
  }

  net_log_.AddEvent(NetLogEventType::SSL_CERTIFICATES_RECEIVED, [&] {
    return base::Value::Dict().Set(
        "certificates", NetLogX509CertificateList(server_cert_.get()));
  });

  // If the certificate is bad and has been previously accepted, use
  // the previous status and bypass the error.
  CertStatus cert_status;
  if (IsAllowedBadCert(server_cert_.get(), &cert_status)) {
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = cert_status;
    server_cert_verify_result_.verified_cert = server_cert_;
    cert_verification_result_ = OK;
    return HandleVerifyResult();
  }

  std::string_view ech_name_override = GetECHNameOverride();
  if (!ech_name_override.empty()) {
    // If ECH was offered but not negotiated, BoringSSL will ask to verify a
    // different name than the origin. If verification succeeds, we continue the
    // handshake, but BoringSSL will not report success from SSL_do_handshake().
    // If all else succeeds, BoringSSL will report |SSL_R_ECH_REJECTED|, mapped
    // to |ERR_R_ECH_NOT_NEGOTIATED|. |ech_name_override| is only used to
    // authenticate GetECHRetryConfigs().
    DCHECK(!ssl_config_.ech_config_list.empty());
    used_ech_name_override_ = true;

    // CertVerifier::Verify takes a string host and internally interprets it as
    // either a DNS name or IP address. However, the ECH public name is only
    // defined to be an DNS name. Thus, reject all public names that would not
    // be interpreted as IP addresses. Distinguishing IPv4 literals from DNS
    // names varies by spec, however. BoringSSL internally checks for an LDH
    // string, and that the last component is non-numeric. This should be
    // sufficient for the web, but check with Chromium's parser, in case they
    // diverge.
    //
    // See section 6.1.7 of draft-ietf-tls-esni-13.
    if (HostIsIPAddressNoBrackets(ech_name_override)) {
      NOTREACHED_IN_MIGRATION();
      OpenSSLPutNetError(FROM_HERE, ERR_INVALID_ECH_CONFIG_LIST);
      return ssl_verify_invalid;
    }
  }

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_.get(), &ocsp_response_raw, &ocsp_response_len);
  std::string_view ocsp_response(
      reinterpret_cast<const char*>(ocsp_response_raw), ocsp_response_len);

  const uint8_t* sct_list_raw;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_.get(), &sct_list_raw, &sct_list_len);
  std::string_view sct_list(reinterpret_cast<const char*>(sct_list_raw),
                            sct_list_len);

  cert_verification_result_ = context_->cert_verifier()->Verify(
      CertVerifier::RequestParams(
          server_cert_,
          ech_name_override.empty() ? host_and_port_.host() : ech_name_override,
          ssl_config_.GetCertVerifyFlags(), std::string(ocsp_response),
          std::string(sct_list)),
      &server_cert_verify_result_,
      base::BindOnce(&SSLClientSocketImpl::OnVerifyComplete,
                     base::Unretained(this)),
      &cert_verifier_request_, net_log_);

  return HandleVerifyResult();
}

void SSLClientSocketImpl::OnVerifyComplete(int result) {
  cert_verification_result_ = result;
  // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
  OnHandshakeIOComplete(OK);
}

ssl_verify_result_t SSLClientSocketImpl::HandleVerifyResult() {
  // Verification is in progress. Inform BoringSSL it should retry the
  // callback later. The next call to VerifyCertCallback will be a
  // continuation of the same verification, so leave
  // cert_verification_result_ as-is.
  if (cert_verification_result_ == ERR_IO_PENDING)
    return ssl_verify_retry;

  // In BoringSSL's calling convention for asynchronous callbacks,
  // after a callback returns a non-retry value, the operation has
  // completed. Subsequent calls are of new operations with potentially
  // different arguments. Reset cert_verification_result_ to inform
  // VerifyCertCallback not to replay the result on subsequent calls.
  int result = cert_verification_result_;
  cert_verification_result_ = kCertVerifyPending;

  cert_verifier_request_.reset();

  // If the connection was good, check HPKP and CT status simultaneously,
  // but prefer to treat the HPKP error as more serious, if there was one.
  if (result == OK) {
    int ct_result = CheckCTRequirements();
    TransportSecurityState::PKPStatus pin_validity =
        context_->transport_security_state()->CheckPublicKeyPins(
            host_and_port_, server_cert_verify_result_.is_issued_by_known_root,
            server_cert_verify_result_.public_key_hashes);
    switch (pin_validity) {
      case TransportSecurityState::PKPStatus::VIOLATED:
        server_cert_verify_result_.cert_status |=
            CERT_STATUS_PINNED_KEY_MISSING;
        result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        break;
      case TransportSecurityState::PKPStatus::BYPASSED:
        pkp_bypassed_ = true;
        [[fallthrough]];
      case TransportSecurityState::PKPStatus::OK:
        // Do nothing.
        break;
    }
    if (result != ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN && ct_result != OK)
      result = ct_result;
  }

  is_fatal_cert_error_ =
      IsCertStatusError(server_cert_verify_result_.cert_status) &&
      result != ERR_CERT_KNOWN_INTERCEPTION_BLOCKED &&
      context_->transport_security_state()->ShouldSSLErrorsBeFatal(
          host_and_port_.host());

  if (IsCertificateError(result)) {
    if (!GetECHNameOverride().empty()) {
      // Certificate exceptions are only applicable for the origin name. For
      // simplicity, we do not allow certificate exceptions for the public name
      // and map all bypassable errors to fatal ones.
      result = ERR_ECH_FALLBACK_CERTIFICATE_INVALID;
    }
    if (ssl_config_.ignore_certificate_errors) {
      result = OK;
    }
  }

  if (result == OK) {
    return ssl_verify_ok;
  }

  OpenSSLPutNetError(FROM_HERE, result);
  return ssl_verify_invalid;
}

int SSLClientSocketImpl::CheckCTRequirements() {
  TransportSecurityState::CTRequirementsStatus ct_requirement_status =
      context_->transport_security_state()->CheckCTRequirements(
          host_and_port_, server_cert_verify_result_.is_issued_by_known_root,
          server_cert_verify_result_.public_key_hashes,
          server_cert_verify_result_.verified_cert.get(),
          server_cert_verify_result_.policy_compliance);

  if (context_->sct_auditing_delegate()) {
    context_->sct_auditing_delegate()->MaybeEnqueueReport(
        host_and_port_, server_cert_verify_result_.verified_cert.get(),
        server_cert_verify_result_.scts);
  }

  switch (ct_requirement_status) {
    case TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
      server_cert_verify_result_.cert_status |=
          CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
      return ERR_CERTIFICATE_TRANSPARENCY_REQUIRED;
    case TransportSecurityState::CT_REQUIREMENTS_MET:
    case TransportSecurityState::CT_NOT_REQUIRED:
      return OK;
  }

  NOTREACHED_IN_MIGRATION();
  return OK;
}

void SSLClientSocketImpl::DoConnectCallback(int rv) {
  if (!user_connect_callback_.is_null()) {
    std::move(user_connect_callback_).Run(rv > OK ? OK : rv);
  }
}

void SSLClientSocketImpl::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    if (in_confirm_handshake_) {
      in_confirm_handshake_ = false;
      net_log_.EndEvent(NetLogEventType::SSL_CONFIRM_HANDSHAKE);
    } else {
      LogConnectEndEvent(rv);
    }
    DoConnectCallback(rv);
  }
}

int SSLClientSocketImpl::DoHandshakeLoop(int last_io_result) {
  TRACE_EVENT0(NetTracingCategory(), "SSLClientSocketImpl::DoHandshakeLoop");
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    next_handshake_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_HANDSHAKE_COMPLETE:
        rv = DoHandshakeComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED_IN_MIGRATION() << "unexpected state" << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketImpl::DoPayloadRead(IOBuffer* buf, int buf_len) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  DCHECK_LT(0, buf_len);
  DCHECK(buf);

  int rv;
  if (pending_read_error_ != kSSLClientSocketNoPendingResult) {
    rv = pending_read_error_;
    pending_read_error_ = kSSLClientSocketNoPendingResult;
    if (rv == 0) {
      net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_RECEIVED,
                                    rv, buf->data());
    } else {
      NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, rv,
                         pending_read_ssl_error_, pending_read_error_info_);
    }
    pending_read_ssl_error_ = SSL_ERROR_NONE;
    pending_read_error_info_ = OpenSSLErrorInfo();
    return rv;
  }

  int total_bytes_read = 0;
  int ssl_ret, ssl_err;
  do {
    ssl_ret = SSL_read(ssl_.get(), buf->data() + total_bytes_read,
                       buf_len - total_bytes_read);
    ssl_err = SSL_get_error(ssl_.get(), ssl_ret);
    if (ssl_ret > 0) {
      total_bytes_read += ssl_ret;
    } else if (ssl_err == SSL_ERROR_WANT_RENEGOTIATE) {
      if (!SSL_renegotiate(ssl_.get())) {
        ssl_err = SSL_ERROR_SSL;
      }
    }
    // Continue processing records as long as there is more data available
    // synchronously.
  } while (ssl_err == SSL_ERROR_WANT_RENEGOTIATE ||
           (total_bytes_read < buf_len && ssl_ret > 0 &&
            transport_adapter_->HasPendingReadData()));

  // Although only the final SSL_read call may have failed, the failure needs to
  // processed immediately, while the information still available in OpenSSL's
  // error queue.
  if (ssl_ret <= 0) {
    pending_read_ssl_error_ = ssl_err;
    if (pending_read_ssl_error_ == SSL_ERROR_ZERO_RETURN) {
      pending_read_error_ = 0;
    } else if (pending_read_ssl_error_ == SSL_ERROR_WANT_X509_LOOKUP &&
               !send_client_cert_) {
      pending_read_error_ = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    } else if (pending_read_ssl_error_ ==
               SSL_ERROR_WANT_PRIVATE_KEY_OPERATION) {
      DCHECK(client_private_key_);
      DCHECK_NE(kSSLClientSocketNoPendingResult, signature_result_);
      pending_read_error_ = ERR_IO_PENDING;
    } else {
      pending_read_error_ = MapLastOpenSSLError(
          pending_read_ssl_error_, err_tracer, &pending_read_error_info_);
    }

    // Many servers do not reliably send a close_notify alert when shutting down
    // a connection, and instead terminate the TCP connection. This is reported
    // as ERR_CONNECTION_CLOSED. Because of this, map the unclean shutdown to a
    // graceful EOF, instead of treating it as an error as it should be.
    if (pending_read_error_ == ERR_CONNECTION_CLOSED)
      pending_read_error_ = 0;
  }

  if (total_bytes_read > 0) {
    // Return any bytes read to the caller. The error will be deferred to the
    // next call of DoPayloadRead.
    rv = total_bytes_read;

    // Do not treat insufficient data as an error to return in the next call to
    // DoPayloadRead() - instead, let the call fall through to check SSL_read()
    // again. The transport may have data available by then.
    if (pending_read_error_ == ERR_IO_PENDING)
      pending_read_error_ = kSSLClientSocketNoPendingResult;
  } else {
    // No bytes were returned. Return the pending read error immediately.
    DCHECK_NE(kSSLClientSocketNoPendingResult, pending_read_error_);
    rv = pending_read_error_;
    pending_read_error_ = kSSLClientSocketNoPendingResult;
  }

  if (rv >= 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_RECEIVED,
                                  rv, buf->data());
  } else if (rv != ERR_IO_PENDING) {
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_READ_ERROR, rv,
                       pending_read_ssl_error_, pending_read_error_info_);
    pending_read_ssl_error_ = SSL_ERROR_NONE;
    pending_read_error_info_ = OpenSSLErrorInfo();
  }
  return rv;
}

int SSLClientSocketImpl::DoPayloadWrite() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_write(ssl_.get(), user_write_buf_->data(), user_write_buf_len_);

  if (rv >= 0) {
    CHECK_LE(rv, user_write_buf_len_);
    net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_SENT, rv,
                                  user_write_buf_->data());
    return rv;
  }

  int ssl_error = SSL_get_error(ssl_.get(), rv);
  if (ssl_error == SSL_ERROR_WANT_PRIVATE_KEY_OPERATION)
    return ERR_IO_PENDING;
  OpenSSLErrorInfo error_info;
  int net_error = MapLastOpenSSLError(ssl_error, err_tracer, &error_info);

  if (net_error != ERR_IO_PENDING) {
    NetLogOpenSSLError(net_log_, NetLogEventType::SSL_WRITE_ERROR, net_error,
                       ssl_error, error_info);
  }
  return net_error;
}

void SSLClientSocketImpl::DoPeek() {
  if (!completed_connect_) {
    return;
  }

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  if (ssl_config_.early_data_enabled && !handled_early_data_result_) {
    // |SSL_peek| will implicitly run |SSL_do_handshake| if needed, but run it
    // manually to pick up the reject reason.
    int rv = SSL_do_handshake(ssl_.get());
    int ssl_err = SSL_get_error(ssl_.get(), rv);
    int err = rv > 0 ? OK : MapOpenSSLError(ssl_err, err_tracer);
    if (err == ERR_IO_PENDING) {
      return;
    }

    // Since the two-parameter version of the macro (which asks for a max value)
    // requires that the max value sentinel be named |kMaxValue|, transform the
    // max-value sentinel into a one-past-the-end ("boundary") sentinel by
    // adding 1, in order to be able to use the three-parameter macro.
    UMA_HISTOGRAM_ENUMERATION("Net.SSLHandshakeEarlyDataReason",
                              SSL_get_early_data_reason(ssl_.get()),
                              ssl_early_data_reason_max_value + 1);
    if (IsGoogleHost(host_and_port_.host())) {
      // Most Google hosts are known to implement 0-RTT, so this gives more
      // targeted metrics as we initially roll out client support. See
      // https://crbug.com/641225.
      UMA_HISTOGRAM_ENUMERATION("Net.SSLHandshakeEarlyDataReason.Google",
                                SSL_get_early_data_reason(ssl_.get()),
                                ssl_early_data_reason_max_value + 1);
    }

    // On early data reject, clear early data on any other sessions in the
    // cache, so retries do not get stuck attempting 0-RTT. See
    // https://crbug.com/1066623.
    if (err == ERR_EARLY_DATA_REJECTED ||
        err == ERR_WRONG_VERSION_ON_EARLY_DATA) {
      context_->ssl_client_session_cache()->ClearEarlyData(
          GetSessionCacheKey(std::nullopt));
    }

    handled_early_data_result_ = true;

    if (err != OK) {
      peek_complete_ = true;
      return;
    }
  }

  if (ssl_config_.disable_post_handshake_peek_for_testing || peek_complete_) {
    return;
  }

  char byte;
  int rv = SSL_peek(ssl_.get(), &byte, 1);
  int ssl_err = SSL_get_error(ssl_.get(), rv);
  if (ssl_err != SSL_ERROR_WANT_READ && ssl_err != SSL_ERROR_WANT_WRITE) {
    peek_complete_ = true;
  }
}

void SSLClientSocketImpl::RetryAllOperations() {
  // SSL_do_handshake, SSL_read, and SSL_write may all be retried when blocked,
  // so retry all operations for simplicity. (Otherwise, SSL_get_error for each
  // operation may be remembered to retry only the blocked ones.)

  // Performing these callbacks may cause |this| to be deleted. If this
  // happens, the other callbacks should not be invoked. Guard against this by
  // holding a WeakPtr to |this| and ensuring it's still valid.
  base::WeakPtr<SSLClientSocketImpl> guard(weak_factory_.GetWeakPtr());
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase. The parameter to OnHandshakeIOComplete is unused.
    OnHandshakeIOComplete(OK);
  }

  if (!guard.get())
    return;

  DoPeek();

  int rv_read = ERR_IO_PENDING;
  int rv_write = ERR_IO_PENDING;
  if (user_read_buf_) {
    rv_read = DoPayloadRead(user_read_buf_.get(), user_read_buf_len_);
  } else if (!user_read_callback_.is_null()) {
    // ReadIfReady() is called by the user. Skip DoPayloadRead() and just let
    // the user know that read can be retried.
    rv_read = OK;
  }

  if (user_write_buf_)
    rv_write = DoPayloadWrite();

  if (rv_read != ERR_IO_PENDING)
    DoReadCallback(rv_read);

  if (!guard.get())
    return;

  if (rv_write != ERR_IO_PENDING)
    DoWriteCallback(rv_write);
}

int SSLClientSocketImpl::ClientCertRequestCallback(SSL* ssl) {
  DCHECK(ssl == ssl_.get());

  net_log_.AddEvent(NetLogEventType::SSL_CLIENT_CERT_REQUESTED);
  certificate_requested_ = true;

  // Clear any currently configured certificates.
  SSL_certs_clear(ssl_.get());

#if !BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
  LOG(WARNING) << "Client auth is not supported";
#else   // BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)
  if (!send_client_cert_) {
    // First pass: we know that a client certificate is needed, but we do not
    // have one at hand. Suspend the handshake. SSL_get_error will return
    // SSL_ERROR_WANT_X509_LOOKUP.
    return -1;
  }

  // Second pass: a client certificate should have been selected.
  if (client_cert_.get()) {
    if (!client_private_key_) {
      // The caller supplied a null private key. Fail the handshake and surface
      // an appropriate error to the caller.
      LOG(WARNING) << "Client cert found without private key";
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
      return -1;
    }

    if (!SetSSLChainAndKey(ssl_.get(), client_cert_.get(), nullptr,
                           &SSLContext::kPrivateKeyMethod)) {
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT);
      return -1;
    }

    std::vector<uint16_t> preferences =
        client_private_key_->GetAlgorithmPreferences();
    // If the key supports rsa_pkcs1_sha256, automatically add support for
    // rsa_pkcs1_sha256_legacy, for use with TLS 1.3. We convert here so that
    // not every `SSLPrivateKey` needs to implement it explicitly.
    if (base::FeatureList::IsEnabled(features::kLegacyPKCS1ForTLS13) &&
        base::Contains(preferences, SSL_SIGN_RSA_PKCS1_SHA256)) {
      preferences.push_back(SSL_SIGN_RSA_PKCS1_SHA256_LEGACY);
    }
    SSL_set_signing_algorithm_prefs(ssl_.get(), preferences.data(),
                                    preferences.size());

    net_log_.AddEventWithIntParams(
        NetLogEventType::SSL_CLIENT_CERT_PROVIDED, "cert_count",
        base::checked_cast<int>(1 +
                                client_cert_->intermediate_buffers().size()));
    return 1;
  }
#endif  // !BUILDFLAG(ENABLE_CLIENT_CERTIFICATES)

  // Send no client certificate.
  net_log_.AddEventWithIntParams(NetLogEventType::SSL_CLIENT_CERT_PROVIDED,
                                 "cert_count", 0);
  return 1;
}

int SSLClientSocketImpl::NewSessionCallback(SSL_SESSION* session) {
  if (!IsCachingEnabled())
    return 0;

  std::optional<IPAddress> ip_addr;
  if (SSL_CIPHER_get_kx_nid(SSL_SESSION_get0_cipher(session)) == NID_kx_rsa) {
    // If RSA key exchange was used, additionally key the cache with the
    // destination IP address. Of course, if a proxy is being used, the
    // semantics of this are a little complex, but we're doing our best. See
    // https://crbug.com/969684
    IPEndPoint ip_endpoint;
    if (stream_socket_->GetPeerAddress(&ip_endpoint) != OK) {
      return 0;
    }
    ip_addr = ip_endpoint.address();
  }

  // OpenSSL optionally passes ownership of |session|. Returning one signals
  // that this function has claimed it.
  context_->ssl_client_session_cache()->Insert(
      GetSessionCacheKey(ip_addr), bssl::UniquePtr<SSL_SESSION>(session));
  return 1;
}

SSLClientSessionCache::Key SSLClientSocketImpl::GetSessionCacheKey(
    std::optional<IPAddress> dest_ip_addr) const {
  SSLClientSessionCache::Key key;
  key.server = host_and_port_;
  key.dest_ip_addr = dest_ip_addr;
  if (NetworkAnonymizationKey::IsPartitioningEnabled()) {
    key.network_anonymization_key = ssl_config_.network_anonymization_key;
  }
  key.privacy_mode = ssl_config_.privacy_mode;
  return key;
}

bool SSLClientSocketImpl::IsRenegotiationAllowed() const {
  if (negotiated_protocol_ == kProtoUnknown)
    return ssl_config_.renego_allowed_default;

  for (NextProto allowed : ssl_config_.renego_allowed_for_protos) {
    if (negotiated_protocol_ == allowed)
      return true;
  }
  return false;
}

bool SSLClientSocketImpl::IsCachingEnabled() const {
  return context_->ssl_client_session_cache() != nullptr;
}

ssl_private_key_result_t SSLClientSocketImpl::PrivateKeySignCallback(
    uint8_t* out,
    size_t* out_len,
    size_t max_out,
    uint16_t algorithm,
    const uint8_t* in,
    size_t in_len) {
  DCHECK_EQ(kSSLClientSocketNoPendingResult, signature_result_);
  DCHECK(signature_.empty());
  DCHECK(client_private_key_);

  net_log_.BeginEvent(NetLogEventType::SSL_PRIVATE_KEY_OP, [&] {
    return NetLogPrivateKeyOperationParams(
        algorithm,
        // Pass the SSLPrivateKey pointer to avoid making copies of the
        // provider name in the common case with logging disabled.
        client_private_key_.get());
  });
  base::UmaHistogramSparse("Net.SSLClientCertSignatureAlgorithm", algorithm);

  // Map rsa_pkcs1_sha256_legacy back to rsa_pkcs1_sha256. We convert it here,
  // so that not every `SSLPrivateKey` needs to implement it explicitly.
  if (base::FeatureList::IsEnabled(features::kLegacyPKCS1ForTLS13) &&
      algorithm == SSL_SIGN_RSA_PKCS1_SHA256_LEGACY) {
    algorithm = SSL_SIGN_RSA_PKCS1_SHA256;
  }

  signature_result_ = ERR_IO_PENDING;
  client_private_key_->Sign(
      algorithm, base::make_span(in, in_len),
      base::BindOnce(&SSLClientSocketImpl::OnPrivateKeyComplete,
                     weak_factory_.GetWeakPtr()));
  return ssl_private_key_retry;
}

ssl_private_key_result_t SSLClientSocketImpl::PrivateKeyCompleteCallback(
    uint8_t* out,
    size_t* out_len,
    size_t max_out) {
  DCHECK_NE(kSSLClientSocketNoPendingResult, signature_result_);
  DCHECK(client_private_key_);

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

void SSLClientSocketImpl::OnPrivateKeyComplete(
    Error error,
    const std::vector<uint8_t>& signature) {
  DCHECK_EQ(ERR_IO_PENDING, signature_result_);
  DCHECK(signature_.empty());
  DCHECK(client_private_key_);

  net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_PRIVATE_KEY_OP, error);

  signature_result_ = error;
  if (signature_result_ == OK)
    signature_ = signature;

  // During a renegotiation, either Read or Write calls may be blocked on an
  // asynchronous private key operation.
  RetryAllOperations();
}

void SSLClientSocketImpl::MessageCallback(int is_write,
                                          int content_type,
                                          const void* buf,
                                          size_t len) {
  switch (content_type) {
    case SSL3_RT_ALERT:
      net_log_.AddEvent(is_write ? NetLogEventType::SSL_ALERT_SENT
                                 : NetLogEventType::SSL_ALERT_RECEIVED,
                        [&] { return NetLogSSLAlertParams(buf, len); });
      break;
    case SSL3_RT_HANDSHAKE:
      net_log_.AddEvent(
          is_write ? NetLogEventType::SSL_HANDSHAKE_MESSAGE_SENT
                   : NetLogEventType::SSL_HANDSHAKE_MESSAGE_RECEIVED,
          [&](NetLogCaptureMode capture_mode) {
            return NetLogSSLMessageParams(!!is_write, buf, len, capture_mode);
          });
      break;
    case SSL3_RT_CLIENT_HELLO_INNER:
      DCHECK(is_write);
      net_log_.AddEvent(NetLogEventType::SSL_ENCRYPTED_CLIENT_HELLO,
                        [&](NetLogCaptureMode capture_mode) {
                          return NetLogSSLMessageParams(!!is_write, buf, len,
                                                        capture_mode);
                        });
      break;
  }
}

void SSLClientSocketImpl::LogConnectEndEvent(int rv) {
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::SSL_CONNECT, rv);
    return;
  }

  net_log_.EndEvent(NetLogEventType::SSL_CONNECT,
                    [&] { return NetLogSSLInfoParams(this); });
}

void SSLClientSocketImpl::RecordNegotiatedProtocol() const {
  UMA_HISTOGRAM_ENUMERATION("Net.SSLNegotiatedAlpnProtocol",
                            negotiated_protocol_, kProtoLast + 1);
}

int SSLClientSocketImpl::MapLastOpenSSLError(
    int ssl_error,
    const crypto::OpenSSLErrStackTracer& tracer,
    OpenSSLErrorInfo* info) {
  int net_error = MapOpenSSLErrorWithDetails(ssl_error, tracer, info);

  if (ssl_error == SSL_ERROR_SSL &&
      ERR_GET_LIB(info->error_code) == ERR_LIB_SSL) {
    // TLS does not provide an alert for missing client certificates, so most
    // servers send a generic handshake_failure alert. Detect this case by
    // checking if we have received a CertificateRequest but sent no
    // certificate. See https://crbug.com/646567.
    if (ERR_GET_REASON(info->error_code) ==
            SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE &&
        certificate_requested_ && send_client_cert_ && !client_cert_) {
      net_error = ERR_BAD_SSL_CLIENT_AUTH_CERT;
    }

    // Per spec, access_denied is only for client-certificate-based access
    // control, but some buggy firewalls use it when blocking a page. To avoid a
    // confusing error, map it to a generic protocol error if no
    // CertificateRequest was sent. See https://crbug.com/630883.
    if (ERR_GET_REASON(info->error_code) == SSL_R_TLSV1_ALERT_ACCESS_DENIED &&
        !certificate_requested_) {
      net_error = ERR_SSL_PROTOCOL_ERROR;
    }

    // This error is specific to the client, so map it here.
    if (ERR_GET_REASON(info->error_code) ==
        SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS) {
      net_error = ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS;
    }
  }

  return net_error;
}

std::string_view SSLClientSocketImpl::GetECHNameOverride() const {
  const char* data;
  size_t len;
  SSL_get0_ech_name_override(ssl_.get(), &data, &len);
  return std::string_view(data, len);
}

bool SSLClientSocketImpl::IsAllowedBadCert(X509Certificate* cert,
                                           CertStatus* cert_status) const {
  if (!GetECHNameOverride().empty()) {
    // Certificate exceptions are only applicable for the origin name. For
    // simplicity, we do not allow certificate exceptions for the public name.
    return false;
  }
  return ssl_config_.IsAllowedBadCert(cert, cert_status);
}

}  // namespace net
