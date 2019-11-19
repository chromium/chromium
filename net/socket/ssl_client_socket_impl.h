// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_IMPL_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_verify_result.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_bio_adapter.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/openssl_ssl_util.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/ssl/ssl_client_session_cache.h"
#include "net/ssl/ssl_config.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace crypto {
class OpenSSLErrStackTracer;
}

namespace net {

class SSLCertRequestInfo;
class SSLInfo;
class SSLPrivateKey;
class SSLKeyLogger;
class X509Certificate;

class SSLClientSocketImpl : public SSLClientSocket,
                            public SocketBIOAdapter::Delegate {
 public:
  // Takes ownership of |stream_socket|, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.  |ssl_config| specifies the SSL
  // settings. The resulting socket may not outlive |context|.
  SSLClientSocketImpl(SSLClientContext* context,
                      std::unique_ptr<StreamSocket> stream_socket,
                      const HostPortPair& host_and_port,
                      const SSLConfig& ssl_config);
  ~SSLClientSocketImpl() override;

  const HostPortPair& host_and_port() const { return host_and_port_; }

  // Log SSL key material to |logger|. Must be called before any
  // SSLClientSockets are created.
  static void SetSSLKeyLogger(std::unique_ptr<SSLKeyLogger> logger);

  // SSLSocket implementation.
  int ExportKeyingMaterial(const base::StringPiece& label,
                           bool has_context,
                           const base::StringPiece& context,
                           unsigned char* out,
                           unsigned int outlen) override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  int ConfirmHandshake(CompletionOnceCallback callback) override;
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
  void DumpMemoryStats(SocketMemoryStats* stats) const override;
  void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) const override;

  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation.
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
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // SocketBIOAdapter implementation:
  void OnReadReady() override;
  void OnWriteReady() override;

 private:
  class PeerCertificateChain;
  class SSLContext;
  friend class SSLClientSocket;
  friend class SSLContext;

  int Init();
  void DoReadCallback(int result);
  void DoWriteCallback(int result);

  int DoHandshake();
  int DoHandshakeComplete(int result);
  void DoConnectCallback(int result);

  void OnVerifyComplete(int result);
  void OnHandshakeIOComplete(int result);

  int DoHandshakeLoop(int last_io_result);
  int DoPayloadRead(IOBuffer* buf, int buf_len);
  int DoPayloadWrite();
  void DoPeek();

  // Called when an asynchronous event completes which may have blocked the
  // pending Connect, Read or Write calls, if any. Retries all state machines
  // and, if complete, runs the respective callbacks.
  void RetryAllOperations();

  // Callback from the SSL layer when a certificate needs to be verified. This
  // is called when establishing new (fresh) connections and when evaluating
  // whether an existing session can be resumed.
  static ssl_verify_result_t VerifyCertCallback(SSL* ssl, uint8_t* out_alert);
  ssl_verify_result_t VerifyCert();
  ssl_verify_result_t HandleVerifyResult();
  int VerifyCT();

  // Callback from the SSL layer that indicates the remote server is requesting
  // a certificate for this client.
  int ClientCertRequestCallback(SSL* ssl);

  // Called from the SSL layer whenever a new session is established.
  int NewSessionCallback(SSL_SESSION* session);

  // Adds the Certificate Transparency info from ct_verify_result_ to
  // |ssl_info|.
  // SCTs are held in three separate vectors in ct_verify_result, each
  // vetor representing a particular verification state, this method associates
  // each of the SCTs with the corresponding SCTVerifyStatus as it adds it to
  // the |ssl_info|.signed_certificate_timestamps list.
  void AddCTInfoToSSLInfo(SSLInfo* ssl_info) const;

  // Returns a session cache key for this socket.
  SSLClientSessionCache::Key GetSessionCacheKey(
      base::Optional<IPAddress> dest_ip_addr) const;

  // Returns true if renegotiations are allowed.
  bool IsRenegotiationAllowed() const;

  // Returns true when we should be using the ssl_client_session_cache_
  bool IsCachingEnabled() const;

  // Callbacks for operations with the private key.
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

  // Called whenever BoringSSL processes a protocol message.
  void MessageCallback(int is_write,
                       int content_type,
                       const void* buf,
                       size_t len);

  void LogConnectEndEvent(int rv);

  // Record whether ALPN was used, and if so, the negotiated protocol,
  // in a UMA histogram.
  void RecordNegotiatedProtocol() const;

  // Returns the net error corresponding to the most recent OpenSSL
  // error. ssl_error is the output of SSL_get_error.
  int MapLastOpenSSLError(int ssl_error,
                          const crypto::OpenSSLErrStackTracer& tracer,
                          OpenSSLErrorInfo* info);

  CompletionOnceCallback user_connect_callback_;
  CompletionOnceCallback user_read_callback_;
  CompletionOnceCallback user_write_callback_;

  // Used by Read function.
  scoped_refptr<IOBuffer> user_read_buf_;
  int user_read_buf_len_;

  // Used by Write function.
  scoped_refptr<IOBuffer> user_write_buf_;
  int user_write_buf_len_;
  bool first_post_handshake_write_ = true;

  // True if we've already recorded the result of our attempt to
  // use early data.
  bool recorded_early_data_result_ = false;

  // Used by DoPayloadRead() when attempting to fill the caller's buffer with
  // as much data as possible without blocking.
  // If DoPayloadRead() encounters an error after having read some data, stores
  // the result to return on the *next* call to DoPayloadRead().  A value > 0
  // indicates there is no pending result, otherwise 0 indicates EOF and < 0
  // indicates an error.
  int pending_read_error_;

  // If there is a pending read result, the OpenSSL result code (output of
  // SSL_get_error) associated with it.
  int pending_read_ssl_error_;

  // If there is a pending read result, the OpenSSLErrorInfo associated with it.
  OpenSSLErrorInfo pending_read_error_info_;

  // Set when Connect finishes.
  scoped_refptr<X509Certificate> server_cert_;
  CertVerifyResult server_cert_verify_result_;
  bool completed_connect_;

  // Set when Read() or Write() successfully reads or writes data to or from the
  // network.
  bool was_ever_used_;

  SSLClientContext* const context_;

  std::unique_ptr<CertVerifier::Request> cert_verifier_request_;
  base::TimeTicks start_cert_verification_time_;

  // Result from Cert Verifier.
  int cert_verification_result_;

  // Certificate Transparency: Verifier and result holder.
  ct::CTVerifyResult ct_verify_result_;

  // OpenSSL stuff
  bssl::UniquePtr<SSL> ssl_;

  std::unique_ptr<StreamSocket> stream_socket_;
  std::unique_ptr<SocketBIOAdapter> transport_adapter_;
  const HostPortPair host_and_port_;
  SSLConfig ssl_config_;

  enum State {
    STATE_NONE,
    STATE_HANDSHAKE,
    STATE_HANDSHAKE_COMPLETE,
  };
  State next_handshake_state_;

  // True if we are currently confirming the handshake.
  bool in_confirm_handshake_;

  // True if the post-handshake SSL_peek has completed.
  bool peek_complete_;

  // True if the socket has been disconnected.
  bool disconnected_;

  NextProto negotiated_protocol_;

  // Set to true if a CertificateRequest was received.
  bool certificate_requested_;

  int signature_result_;
  std::vector<uint8_t> signature_;

  // pinning_failure_log contains a message produced by
  // TransportSecurityState::CheckPublicKeyPins in the event of a
  // pinning failure. It is a (somewhat) human-readable string.
  std::string pinning_failure_log_;

  // True if PKP is bypassed due to a local trust anchor.
  bool pkp_bypassed_;

  // True if there was a certificate error which should be treated as fatal,
  // and false otherwise.
  bool is_fatal_cert_error_;

  // True if the socket should respond to client certificate requests with
  // |client_cert_| and |client_private_key_|, which may be null to continue
  // with no certificate. If false, client certificate requests will result in
  // ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  bool send_client_cert_;
  scoped_refptr<X509Certificate> client_cert_;
  scoped_refptr<SSLPrivateKey> client_private_key_;

  NetLogWithSource net_log_;
  base::WeakPtrFactory<SSLClientSocketImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SSLClientSocketImpl);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_IMPL_H_
