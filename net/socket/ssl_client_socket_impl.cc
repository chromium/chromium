// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket_impl.h"

#include <errno.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/url_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/sct_auditing_delegate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util.h"
#include "net/der/parse_values.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
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

#if !defined(NET_DISABLE_BROTLI)
#include "third_party/brotli/include/brotli/decode.h"
#endif

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

base::Value NetLogPrivateKeyOperationParams(uint16_t algorithm,
                                            SSLPrivateKey* key) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey("algorithm", SSL_get_signature_algorithm_name(
                                      algorithm, 0 /* exclude curve */));
  value.SetStringKey("provider", key->GetProviderName());
  return value;
}

base::Value NetLogSSLInfoParams(SSLClientSocketImpl* socket) {
  SSLInfo ssl_info;
  if (!socket->GetSSLInfo(&ssl_info))
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  const char* version_str;
  SSLVersionToString(&version_str,
                     SSLConnectionStatusToVersion(ssl_info.connection_status));
  dict.SetStringKey("version", version_str);
  dict.SetBoolKey("is_resumed",
                  ssl_info.handshake_type == SSLInfo::HANDSHAKE_RESUME);
  dict.SetIntKey("cipher_suite",
                 SSLConnectionStatusToCipherSuite(ssl_info.connection_status));
  dict.SetIntKey("key_exchange_group", ssl_info.key_exchange_group);
  dict.SetIntKey("peer_signature_algorithm", ssl_info.peer_signature_algorithm);

  dict.SetStringKey("next_proto",
                    NextProtoToString(socket->GetNegotiatedProtocol()));

  return dict;
}

base::Value NetLogSSLAlertParams(const void* bytes, size_t len) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("bytes", NetLogBinaryValue(bytes, len));
  return dict;
}

base::Value NetLogSSLMessageParams(bool is_write,
                                   const void* bytes,
                                   size_t len,
                                   NetLogCaptureMode capture_mode) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (len == 0) {
    NOTREACHED();
    return dict;
  }

  // The handshake message type is the first byte. Include it so elided messages
  // still report their type.
  uint8_t type = reinterpret_cast<const uint8_t*>(bytes)[0];
  dict.SetIntKey("type", type);

  // Elide client certificate messages unless logging socket bytes. The client
  // certificate does not contain information needed to impersonate the user
  // (that's the private key which isn't sent over the wire), but it may contain
  // information on the user's identity.
  if (!is_write || type != SSL3_MT_CERTIFICATE ||
      NetLogCaptureIncludesSocketBytes(capture_mode)) {
    dict.SetKey("bytes", NetLogBinaryValue(bytes, len));
  }

  return dict;
}

// This enum is used in histograms, so values may not be reused.
enum class RSAKeyUsage {
  // The TLS cipher suite was not RSA or ECDHE_RSA.
  kNotRSA = 0,
  // The Key Usage extension is not present, which is consistent with TLS usage.
  kOKNoExtension = 1,
  // The Key Usage extension has both the digitalSignature and keyEncipherment
  // bits, which is consistent with TLS usage.
  kOKHaveBoth = 2,
  // The Key Usage extension contains only the digitalSignature bit, which is
  // consistent with TLS usage.
  kOKHaveDigitalSignature = 3,
  // The Key Usage extension contains only the keyEncipherment bit, which is
  // consistent with TLS usage.
  kOKHaveKeyEncipherment = 4,
  // The Key Usage extension is missing the digitalSignature bit.
  kMissingDigitalSignature = 5,
  // The Key Usage extension is missing the keyEncipherment bit.
  kMissingKeyEncipherment = 6,
  // There was an error processing the certificate.
  kError = 7,

  kLastValue = kError,
};

RSAKeyUsage CheckRSAKeyUsage(const X509Certificate* cert,
                             const SSL_CIPHER* cipher) {
  bool need_key_encipherment = false;
  switch (SSL_CIPHER_get_kx_nid(cipher)) {
    case NID_kx_rsa:
      need_key_encipherment = true;
      break;
    case NID_kx_ecdhe:
      if (SSL_CIPHER_get_auth_nid(cipher) != NID_auth_rsa) {
        return RSAKeyUsage::kNotRSA;
      }
      break;
    default:
      return RSAKeyUsage::kNotRSA;
  }

  const CRYPTO_BUFFER* buffer = cert->cert_buffer();
  der::Input tbs_certificate_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  ParsedTbsCertificate tbs;
  if (!ParseCertificate(
          der::Input(CRYPTO_BUFFER_data(buffer), CRYPTO_BUFFER_len(buffer)),
          &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
          nullptr) ||
      !ParseTbsCertificate(tbs_certificate_tlv,
                           x509_util::DefaultParseCertificateOptions(), &tbs,
                           nullptr)) {
    return RSAKeyUsage::kError;
  }

  if (!tbs.has_extensions) {
    return RSAKeyUsage::kOKNoExtension;
  }

  std::map<der::Input, ParsedExtension> extensions;
  if (!ParseExtensions(tbs.extensions_tlv, &extensions)) {
    return RSAKeyUsage::kError;
  }
  ParsedExtension key_usage_ext;
  if (!ConsumeExtension(KeyUsageOid(), &extensions, &key_usage_ext)) {
    return RSAKeyUsage::kOKNoExtension;
  }
  der::BitString key_usage;
  if (!ParseKeyUsage(key_usage_ext.value, &key_usage)) {
    return RSAKeyUsage::kError;
  }

  bool have_digital_signature =
      key_usage.AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE);
  bool have_key_encipherment =
      key_usage.AssertsBit(KEY_USAGE_BIT_KEY_ENCIPHERMENT);
  if (have_digital_signature && have_key_encipherment) {
    return RSAKeyUsage::kOKHaveBoth;
  }

  if (need_key_encipherment) {
    return have_key_encipherment ? RSAKeyUsage::kOKHaveKeyEncipherment
                                 : RSAKeyUsage::kMissingKeyEncipherment;
  }
  return have_digital_signature ? RSAKeyUsage::kOKHaveDigitalSignature
                                : RSAKeyUsage::kMissingDigitalSignature;
}

#if !defined(NET_DISABLE_BROTLI)
int DecompressBrotliCert(SSL* ssl,
                         CRYPTO_BUFFER** out,
                         size_t uncompressed_len,
                         const uint8_t* in,
                         size_t in_len) {
  uint8_t* data;
  bssl::UniquePtr<CRYPTO_BUFFER> decompressed(
      CRYPTO_BUFFER_alloc(&data, uncompressed_len));
  if (!decompressed) {
    return 0;
  }

  size_t output_size = uncompressed_len;
  if (BrotliDecoderDecompress(in_len, in, &output_size, data) !=
          BROTLI_DECODER_RESULT_SUCCESS ||
      output_size != uncompressed_len) {
    return 0;
  }

  *out = decompressed.release();
  return 1;
}
#endif

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
    DCHECK(!ssl_key_logger_);
    ssl_key_logger_ = std::move(logger);
    SSL_CTX_set_keylog_callback(ssl_ctx_.get(), KeyLogCallback);
  }

  static const SSL_PRIVATE_KEY_METHOD kPrivateKeyMethod;

 private:
  friend struct base::DefaultSingletonTraits<SSLContext>;

  SSLContext() {
    crypto::EnsureOpenSSLInit();
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

#if !defined(NET_DISABLE_BROTLI)
    SSL_CTX_add_cert_compression_alg(
        ssl_ctx_.get(), TLSEXT_cert_compression_brotli,
        nullptr /* compression not supported */, DecompressBrotliCert);
#endif

    if (base::FeatureList::IsEnabled(features::kPostQuantumCECPQ2)) {
      static const int kCurves[] = {NID_CECPQ2, NID_X25519,
                                    NID_X9_62_prime256v1, NID_secp384r1};
      SSL_CTX_set1_curves(ssl_ctx_.get(), kCurves, base::size(kCurves));
    }
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

  static void KeyLogCallback(const SSL* ssl, const char* line) {
    GetInstance()->ssl_key_logger_->WriteLine(line);
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

  std::unique_ptr<SSLKeyLogger> ssl_key_logger_;
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
      pending_read_ssl_error_(SSL_ERROR_NONE),
      completed_connect_(false),
      was_ever_used_(false),
      context_(context),
      cert_verification_result_(kCertVerifyPending),
      stream_socket_(std::move(stream_socket)),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      next_handshake_state_(STATE_NONE),
      in_confirm_handshake_(false),
      peek_complete_(false),
      disconnected_(false),
      negotiated_protocol_(kProtoUnknown),
      certificate_requested_(false),
      signature_result_(kSSLClientSocketNoPendingResult),
      pkp_bypassed_(false),
      is_fatal_cert_error_(false),
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

int SSLClientSocketImpl::ExportKeyingMaterial(const base::StringPiece& label,
                                              bool has_context,
                                              const base::StringPiece& context,
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

bool SSLClientSocketImpl::WasAlpnNegotiated() const {
  return negotiated_protocol_ != kProtoUnknown;
}

NextProto SSLClientSocketImpl::GetNegotiatedProtocol() const {
  return negotiated_protocol_;
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
  ssl_info->pinning_failure_log = pinning_failure_log_;
  ssl_info->ocsp_result = server_cert_verify_result_.ocsp_result;
  ssl_info->is_fatal_cert_error = is_fatal_cert_error_;
  AddCTInfoToSSLInfo(ssl_info);

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_.get());
  CHECK(cipher);
  // Historically, the "group" was known as "curve".
  ssl_info->key_exchange_group = SSL_get_curve_id(ssl_.get());
  ssl_info->peer_signature_algorithm =
      SSL_get_peer_signature_algorithm(ssl_.get());

  SSLConnectionStatusSetCipherSuite(
      static_cast<uint16_t>(SSL_CIPHER_get_id(cipher)),
      &ssl_info->connection_status);
  SSLConnectionStatusSetVersion(GetNetSSLVersion(ssl_.get()),
                                &ssl_info->connection_status);

  ssl_info->handshake_type = SSL_session_reused(ssl_.get())
                                 ? SSLInfo::HANDSHAKE_RESUME
                                 : SSLInfo::HANDSHAKE_FULL;

  return true;
}

void SSLClientSocketImpl::GetConnectionAttempts(ConnectionAttempts* out) const {
  out->clear();
}

int64_t SSLClientSocketImpl::GetTotalReceivedBytes() const {
  return stream_socket_->GetTotalReceivedBytes();
}

void SSLClientSocketImpl::DumpMemoryStats(SocketMemoryStats* stats) const {
  if (transport_adapter_)
    stats->buffer_size = transport_adapter_->GetAllocationSize();
  const STACK_OF(CRYPTO_BUFFER)* server_cert_chain =
      SSL_get0_peer_certificates(ssl_.get());
  if (server_cert_chain) {
    for (const CRYPTO_BUFFER* cert : server_cert_chain) {
      stats->cert_size += CRYPTO_BUFFER_len(cert);
    }
    stats->cert_count = sk_CRYPTO_BUFFER_num(server_cert_chain);
  }
  stats->total_size = stats->buffer_size + stats->cert_size;
}

void SSLClientSocketImpl::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  if (!ssl_) {
    NOTREACHED();
    return;
  }

  cert_request_info->host_and_port = host_and_port_;

  cert_request_info->cert_authorities.clear();
  const STACK_OF(CRYPTO_BUFFER)* authorities =
      SSL_get0_server_requested_CAs(ssl_.get());
  for (const CRYPTO_BUFFER* ca_name : authorities) {
    cert_request_info->cert_authorities.push_back(
        std::string(reinterpret_cast<const char*>(CRYPTO_BUFFER_data(ca_name)),
                    CRYPTO_BUFFER_len(ca_name)));
  }

  cert_request_info->cert_key_types.clear();
  const uint8_t* client_cert_types;
  size_t num_client_cert_types =
      SSL_get0_certificate_types(ssl_.get(), &client_cert_types);
  for (size_t i = 0; i < num_client_cert_types; i++) {
    cert_request_info->cert_key_types.push_back(
        static_cast<SSLClientCertType>(client_cert_types[i]));
  }
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
  int result = stream_socket_->CancelReadIfReady();
  // Cancel |user_read_callback_|, because caller does not expect the callback
  // to be invoked after they have canceled the ReadIfReady.
  user_read_callback_.Reset();
  return result;
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
    if (rv > 0)
      was_ever_used_ = true;
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

  // SNI should only contain valid DNS hostnames, not IP addresses (see RFC
  // 6066, Section 3).
  //
  // TODO(rsleevi): Should this code allow hostnames that violate the LDH rule?
  // See https://crbug.com/496472 and https://crbug.com/496468 for discussion.
  IPAddress unused;
  if (!unused.AssignFromIPLiteral(host_and_port_.host()) &&
      !SSL_set_tlsext_host_name(ssl_.get(), host_and_port_.host().c_str())) {
    return ERR_UNEXPECTED;
  }

  if (IsCachingEnabled()) {
    bssl::UniquePtr<SSL_SESSION> session =
        context_->ssl_client_session_cache()->Lookup(
            GetSessionCacheKey(/*dest_ip_addr=*/base::nullopt));
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

  transport_adapter_.reset(
      new SocketBIOAdapter(stream_socket_.get(), kDefaultOpenSSLBufferSize,
                           kDefaultOpenSSLBufferSize, this));
  BIO* transport_bio = transport_adapter_->bio();

  BIO_up_ref(transport_bio);  // SSL_set0_rbio takes ownership.
  SSL_set0_rbio(ssl_.get(), transport_bio);

  BIO_up_ref(transport_bio);  // SSL_set0_wbio takes ownership.
  SSL_set0_wbio(ssl_.get(), transport_bio);

  uint16_t version_min =
      ssl_config_.version_min_override.value_or(context_->config().version_min);
  uint16_t version_max =
      ssl_config_.version_max_override.value_or(context_->config().version_max);
  DCHECK_LT(SSL3_VERSION, version_min);
  DCHECK_LT(SSL3_VERSION, version_max);
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

  // Use BoringSSL defaults, but disable HMAC-SHA1 ciphers in ECDSA. These are
  // the remaining CBC-mode ECDSA ciphers.
  std::string command("ALL::!aPSK:!ECDSA+SHA1");

  if (ssl_config_.require_ecdhe)
    command.append(":!kRSA");
  if (ssl_config_.disable_legacy_crypto)
    command.append(":!3DES");

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

  if (ssl_config_.disable_legacy_crypto) {
    static const uint16_t kVerifyPrefs[] = {
        SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_RSA_PSS_RSAE_SHA256,
        SSL_SIGN_RSA_PKCS1_SHA256,       SSL_SIGN_ECDSA_SECP384R1_SHA384,
        SSL_SIGN_RSA_PSS_RSAE_SHA384,    SSL_SIGN_RSA_PKCS1_SHA384,
        SSL_SIGN_RSA_PSS_RSAE_SHA512,    SSL_SIGN_RSA_PKCS1_SHA512,
    };
    if (!SSL_set_verify_algorithm_prefs(ssl_.get(), kVerifyPrefs,
                                        base::size(kVerifyPrefs))) {
      return ERR_UNEXPECTED;
    }
  }

  if (!ssl_config_.alpn_protos.empty()) {
    std::vector<uint8_t> wire_protos =
        SerializeNextProtos(ssl_config_.alpn_protos);
    SSL_set_alpn_protos(ssl_.get(), wire_protos.data(), wire_protos.size());
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

  // TODO(https://crbug.com/775438), if |ssl_config_.privacy_mode| is enabled,
  // this should always continue with no client certificate.
  if (ssl_config_.privacy_mode == PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS) {
    send_client_cert_ = true;
  } else {
    send_client_cert_ = context_->GetClientCertificate(
        host_and_port_, &client_cert_, &client_private_key_);
  }

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

  const uint8_t* alpn_proto = nullptr;
  unsigned alpn_len = 0;
  SSL_get0_alpn_selected(ssl_.get(), &alpn_proto, &alpn_len);
  if (alpn_len > 0) {
    base::StringPiece proto(reinterpret_cast<const char*>(alpn_proto),
                            alpn_len);
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

  // See how feasible enforcing RSA key usage would be. See
  // https://crbug.com/795089.
  if (!server_cert_verify_result_.is_issued_by_known_root) {
    RSAKeyUsage rsa_key_usage = CheckRSAKeyUsage(
        server_cert_.get(), SSL_get_current_cipher(ssl_.get()));
    if (rsa_key_usage != RSAKeyUsage::kNotRSA) {
      UMA_HISTOGRAM_ENUMERATION("Net.SSLRSAKeyUsage.UnknownRoot", rsa_key_usage,
                                static_cast<int>(RSAKeyUsage::kLastValue) + 1);
    }
  }

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
  // TODO(https://crbug.com/958638): It is also a step in making TLS 1.3 client
  // certificate alerts less unreliable.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
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
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("certificates", NetLogX509CertificateList(server_cert_.get()));
    return dict;
  });

  // If the certificate is bad and has been previously accepted, use
  // the previous status and bypass the error.
  CertStatus cert_status;
  if (ssl_config_.IsAllowedBadCert(server_cert_.get(), &cert_status)) {
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = cert_status;
    server_cert_verify_result_.verified_cert = server_cert_;
    cert_verification_result_ = OK;
    return HandleVerifyResult();
  }

  start_cert_verification_time_ = base::TimeTicks::Now();

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_.get(), &ocsp_response_raw, &ocsp_response_len);
  base::StringPiece ocsp_response(
      reinterpret_cast<const char*>(ocsp_response_raw), ocsp_response_len);

  const uint8_t* sct_list_raw;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_.get(), &sct_list_raw, &sct_list_len);
  base::StringPiece sct_list(reinterpret_cast<const char*>(sct_list_raw),
                             sct_list_len);

  cert_verification_result_ = context_->cert_verifier()->Verify(
      CertVerifier::RequestParams(
          server_cert_, host_and_port_.host(), ssl_config_.GetCertVerifyFlags(),
          ocsp_response.as_string(), sct_list.as_string()),
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

  if (!start_cert_verification_time_.is_null()) {
    base::TimeDelta verify_time =
        base::TimeTicks::Now() - start_cert_verification_time_;
    if (result == OK) {
      UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTime", verify_time);
    } else {
      UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTimeError", verify_time);
    }
  }

  // Enforce keyUsage extension for RSA leaf certificates chaining up to known
  // roots.
  // TODO(crbug.com/795089): Enforce this unconditionally.
  if (server_cert_verify_result_.is_issued_by_known_root) {
    SSL_set_enforce_rsa_key_usage(ssl_.get(), 1);
  }

  // If the connection was good, check HPKP and CT status simultaneously,
  // but prefer to treat the HPKP error as more serious, if there was one.
  if (result == OK) {
    int ct_result = VerifyCT();
    TransportSecurityState::PKPStatus pin_validity =
        context_->transport_security_state()->CheckPublicKeyPins(
            host_and_port_, server_cert_verify_result_.is_issued_by_known_root,
            server_cert_verify_result_.public_key_hashes, server_cert_.get(),
            server_cert_verify_result_.verified_cert.get(),
            TransportSecurityState::ENABLE_PIN_REPORTS, &pinning_failure_log_);
    switch (pin_validity) {
      case TransportSecurityState::PKPStatus::VIOLATED:
        server_cert_verify_result_.cert_status |=
            CERT_STATUS_PINNED_KEY_MISSING;
        result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        break;
      case TransportSecurityState::PKPStatus::BYPASSED:
        pkp_bypassed_ = true;
        FALLTHROUGH;
      case TransportSecurityState::PKPStatus::OK:
        // Do nothing.
        break;
    }
    if (result != ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN && ct_result != OK)
      result = ct_result;
  }

  // If no other errors occurred, check whether the connection used a legacy TLS
  // version.
  if (result == OK &&
      SSL_version(ssl_.get()) < context_->config().version_min_warn &&
      base::FeatureList::IsEnabled(features::kLegacyTLSEnforced) &&
      !context_->ssl_config_service()->ShouldSuppressLegacyTLSWarning(
          host_and_port_.host())) {
    server_cert_verify_result_.cert_status |= CERT_STATUS_LEGACY_TLS;

    // Only set the resulting net error if it hasn't been previously bypassed.
    if (!ssl_config_.IsAllowedBadCert(server_cert_.get(), nullptr))
      result = ERR_SSL_OBSOLETE_VERSION;
  }

  is_fatal_cert_error_ =
      IsCertStatusError(server_cert_verify_result_.cert_status) &&
      result != ERR_CERT_KNOWN_INTERCEPTION_BLOCKED &&
      result != ERR_SSL_OBSOLETE_VERSION &&
      context_->transport_security_state()->ShouldSSLErrorsBeFatal(
          host_and_port_.host());

  if (IsCertificateError(result) && ssl_config_.ignore_certificate_errors) {
    result = OK;
  }

  if (result == OK) {
    return ssl_verify_ok;
  }

  OpenSSLPutNetError(FROM_HERE, result);
  return ssl_verify_invalid;
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
        NOTREACHED() << "unexpected state" << state;
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
    net_log_.AddByteTransferEvent(NetLogEventType::SSL_SOCKET_BYTES_SENT, rv,
                                  user_write_buf_->data());
    if (first_post_handshake_write_ && SSL_is_init_finished(ssl_.get())) {
      if (base::FeatureList::IsEnabled(features::kTLS13KeyUpdate) &&
          SSL_version(ssl_.get()) == TLS1_3_VERSION) {
        const int ok = SSL_key_update(ssl_.get(), SSL_KEY_UPDATE_REQUESTED);
        DCHECK(ok);
      }
      first_post_handshake_write_ = false;
    }
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

    // On early data reject, clear early data on any other sessions in the
    // cache, so retries do not get stuck attempting 0-RTT. See
    // https://crbug.com/1066623.
    if (err == ERR_EARLY_DATA_REJECTED ||
        err == ERR_WRONG_VERSION_ON_EARLY_DATA) {
      context_->ssl_client_session_cache()->ClearEarlyData(
          GetSessionCacheKey(base::nullopt));
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

int SSLClientSocketImpl::VerifyCT() {
  const uint8_t* sct_list_raw;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_.get(), &sct_list_raw, &sct_list_len);
  base::StringPiece sct_list(reinterpret_cast<const char*>(sct_list_raw),
                             sct_list_len);

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_.get(), &ocsp_response_raw, &ocsp_response_len);
  base::StringPiece ocsp_response(
      reinterpret_cast<const char*>(ocsp_response_raw), ocsp_response_len);

  // Note that this is a completely synchronous operation: The CT Log Verifier
  // gets all the data it needs for SCT verification and does not do any
  // external communication.
  context_->cert_transparency_verifier()->Verify(
      host_and_port().host(), server_cert_verify_result_.verified_cert.get(),
      ocsp_response, sct_list, &ct_verify_result_.scts, net_log_);

  ct::SCTList verified_scts =
      ct::SCTsMatchingStatus(ct_verify_result_.scts, ct::SCT_STATUS_OK);

  ct_verify_result_.policy_compliance =
      context_->ct_policy_enforcer()->CheckCompliance(
          server_cert_verify_result_.verified_cert.get(), verified_scts,
          net_log_);
  if (server_cert_verify_result_.cert_status & CERT_STATUS_IS_EV) {
    if (ct_verify_result_.policy_compliance !=
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS &&
        ct_verify_result_.policy_compliance !=
            ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
      server_cert_verify_result_.cert_status |=
          CERT_STATUS_CT_COMPLIANCE_FAILED;
      server_cert_verify_result_.cert_status &= ~CERT_STATUS_IS_EV;
    }

    // Record the CT compliance status for connections with EV certificates, to
    // distinguish how often EV status is being dropped due to failing CT
    // compliance.
    if (server_cert_verify_result_.is_issued_by_known_root) {
      UMA_HISTOGRAM_ENUMERATION("Net.CertificateTransparency.EVCompliance2.SSL",
                                ct_verify_result_.policy_compliance,
                                ct::CTPolicyCompliance::CT_POLICY_COUNT);
    }
  }

  // Record the CT compliance of every connection to get an overall picture of
  // how many connections are CT-compliant.
  if (server_cert_verify_result_.is_issued_by_known_root) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.CertificateTransparency.ConnectionComplianceStatus2.SSL",
        ct_verify_result_.policy_compliance,
        ct::CTPolicyCompliance::CT_POLICY_COUNT);
  }

  TransportSecurityState::CTRequirementsStatus ct_requirement_status =
      context_->transport_security_state()->CheckCTRequirements(
          host_and_port_, server_cert_verify_result_.is_issued_by_known_root,
          server_cert_verify_result_.public_key_hashes,
          server_cert_verify_result_.verified_cert.get(), server_cert_.get(),
          ct_verify_result_.scts,
          TransportSecurityState::ENABLE_EXPECT_CT_REPORTS,
          ct_verify_result_.policy_compliance,
          ssl_config_.network_isolation_key);
  if (ct_requirement_status != TransportSecurityState::CT_NOT_REQUIRED) {
    ct_verify_result_.policy_compliance_required = true;
    if (server_cert_verify_result_.is_issued_by_known_root) {
      // Record the CT compliance of connections for which compliance is
      // required; this helps answer the question: "Of all connections that are
      // supposed to be serving valid CT information, how many fail to do so?"
      UMA_HISTOGRAM_ENUMERATION(
          "Net.CertificateTransparency.CTRequiredConnectionComplianceStatus2."
          "SSL",
          ct_verify_result_.policy_compliance,
          ct::CTPolicyCompliance::CT_POLICY_COUNT);
    }
  } else {
    ct_verify_result_.policy_compliance_required = false;
  }

  if (context_->sct_auditing_delegate() &&
      context_->sct_auditing_delegate()->IsSCTAuditingEnabled() &&
      server_cert_verify_result_.is_issued_by_known_root) {
    context_->sct_auditing_delegate()->MaybeEnqueueReport(
        host_and_port_, server_cert_verify_result_.verified_cert.get(),
        ct_verify_result_.scts);
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

  NOTREACHED();
  return OK;
}

int SSLClientSocketImpl::ClientCertRequestCallback(SSL* ssl) {
  DCHECK(ssl == ssl_.get());

  net_log_.AddEvent(NetLogEventType::SSL_CLIENT_CERT_REQUESTED);
  certificate_requested_ = true;

  // Clear any currently configured certificates.
  SSL_certs_clear(ssl_.get());

#if defined(OS_IOS)
  // TODO(droger): Support client auth on iOS. See http://crbug.com/145954).
  LOG(WARNING) << "Client auth is not supported";
#else   // !defined(OS_IOS)
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
    SSL_set_signing_algorithm_prefs(ssl_.get(), preferences.data(),
                                    preferences.size());

    net_log_.AddEventWithIntParams(
        NetLogEventType::SSL_CLIENT_CERT_PROVIDED, "cert_count",
        base::checked_cast<int>(1 +
                                client_cert_->intermediate_buffers().size()));
    return 1;
  }
#endif  // defined(OS_IOS)

  // Send no client certificate.
  net_log_.AddEventWithIntParams(NetLogEventType::SSL_CLIENT_CERT_PROVIDED,
                                 "cert_count", 0);
  return 1;
}

int SSLClientSocketImpl::NewSessionCallback(SSL_SESSION* session) {
  if (!IsCachingEnabled())
    return 0;

  base::Optional<IPAddress> ip_addr;
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

void SSLClientSocketImpl::AddCTInfoToSSLInfo(SSLInfo* ssl_info) const {
  ssl_info->UpdateCertificateTransparencyInfo(ct_verify_result_);
}

SSLClientSessionCache::Key SSLClientSocketImpl::GetSessionCacheKey(
    base::Optional<IPAddress> dest_ip_addr) const {
  SSLClientSessionCache::Key key;
  key.server = host_and_port_;
  key.dest_ip_addr = dest_ip_addr;
  if (base::FeatureList::IsEnabled(
          features::kPartitionSSLSessionsByNetworkIsolationKey)) {
    key.network_isolation_key = ssl_config_.network_isolation_key;
  }
  key.privacy_mode = ssl_config_.privacy_mode;
  key.disable_legacy_crypto = ssl_config_.disable_legacy_crypto;
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
    default:
      return;
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

}  // namespace net
