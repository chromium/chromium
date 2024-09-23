// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
#define NET_HTTP_HTTP_NETWORK_TRANSACTION_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "crypto/ec_private_key.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/http/http_auth.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_request.h"
#include "net/http/http_transaction.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/connection_attempts.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

class BidirectionalStreamImpl;
class HttpAuthController;
class HttpNetworkSession;
class HttpStream;
class IOBuffer;
class ProxyInfo;
class SSLPrivateKey;
struct HttpRequestInfo;

class NET_EXPORT_PRIVATE HttpNetworkTransaction
    : public HttpTransaction,
      public HttpStreamRequest::Delegate {
 public:
  HttpNetworkTransaction(RequestPriority priority, HttpNetworkSession* session);

  HttpNetworkTransaction(const HttpNetworkTransaction&) = delete;
  HttpNetworkTransaction& operator=(const HttpNetworkTransaction&) = delete;

  ~HttpNetworkTransaction() override;

  // HttpTransaction methods:
  int Start(const HttpRequestInfo* request_info,
            CompletionOnceCallback callback,
            const NetLogWithSource& net_log) override;
  int RestartIgnoringLastError(CompletionOnceCallback callback) override;
  int RestartWithCertificate(scoped_refptr<X509Certificate> client_cert,
                             scoped_refptr<SSLPrivateKey> client_private_key,
                             CompletionOnceCallback callback) override;
  int RestartWithAuth(const AuthCredentials& credentials,
                      CompletionOnceCallback callback) override;
  bool IsReadyToRestartForAuth() override;

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  void StopCaching() override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  int64_t GetReceivedBodyBytes() const override;
  void DoneReading() override;
  const HttpResponseInfo* GetResponseInfo() const override;
  LoadState GetLoadState() const override;
  void SetQuicServerInfo(QuicServerInfo* quic_server_info) override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  bool GetRemoteEndpoint(IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;
  void SetPriority(RequestPriority priority) override;
  void SetWebSocketHandshakeStreamCreateHelper(
      WebSocketHandshakeStreamBase::CreateHelper* create_helper) override;
  void SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback callback) override;
  void SetConnectedCallback(const ConnectedCallback& callback) override;
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;
  void SetEarlyResponseHeadersCallback(
      ResponseHeadersCallback callback) override;
  void SetResponseHeadersCallback(ResponseHeadersCallback callback) override;
  void SetModifyRequestHeadersCallback(
      base::RepeatingCallback<void(HttpRequestHeaders*)> callback) override;
  void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) override;
  int ResumeNetworkStart() override;
  void CloseConnectionOnDestruction() override;
  bool IsMdlMatchForMetrics() const override;

  // HttpStreamRequest::Delegate methods:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override;
  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override;
  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override;
  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(int status, const SSLInfo& ssl_info) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response_info,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override;
  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override;

  void OnQuicBroken() override;

  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolSwitchingInfo switching_info) override;

  ConnectionAttempts GetConnectionAttempts() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest, ResetStateForRestart);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           CreateWebSocketHandshakeStream);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           SetProxyInfoInResponse_Direct);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           SetProxyInfoInResponse_Proxied);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           SetProxyInfoInResponse_Empty);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           SetProxyInfoInResponse_IpProtectionProxied);
  FRIEND_TEST_ALL_PREFIXES(HttpNetworkTransactionTest,
                           SetProxyInfoInResponse_IpProtectionDirect);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest, WindowUpdateReceived);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest, WindowUpdateSent);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest, WindowUpdateOverflow);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest, FlowControlStallResume);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest,
                           FlowControlStallResumeAfterSettings);
  FRIEND_TEST_ALL_PREFIXES(SpdyNetworkTransactionTest,
                           FlowControlNegativeSendWindowSize);

  enum State {
    STATE_NOTIFY_BEFORE_CREATE_STREAM,
    STATE_CREATE_STREAM,
    STATE_CREATE_STREAM_COMPLETE,
    STATE_INIT_STREAM,
    STATE_INIT_STREAM_COMPLETE,
    STATE_CONNECTED_CALLBACK,
    STATE_CONNECTED_CALLBACK_COMPLETE,
    STATE_GENERATE_PROXY_AUTH_TOKEN,
    STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE,
    STATE_GENERATE_SERVER_AUTH_TOKEN,
    STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE,
    STATE_INIT_REQUEST_BODY,
    STATE_INIT_REQUEST_BODY_COMPLETE,
    STATE_BUILD_REQUEST,
    STATE_BUILD_REQUEST_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_NONE
  };

  bool IsSecureRequest() const;

  // Returns true if the request is using an HTTP(S) proxy without being
  // tunneled via the CONNECT method.
  bool UsingHttpProxyWithoutTunnel() const;

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoNotifyBeforeCreateStream();
  int DoCreateStream();
  int DoCreateStreamComplete(int result);
  int DoInitStream();
  int DoInitStreamComplete(int result);
  int DoConnectedCallback();
  int DoConnectedCallbackComplete(int result);
  int DoGenerateProxyAuthToken();
  int DoGenerateProxyAuthTokenComplete(int result);
  int DoGenerateServerAuthToken();
  int DoGenerateServerAuthTokenComplete(int result);
  int DoInitRequestBody();
  int DoInitRequestBodyComplete(int result);
  int DoBuildRequest();
  int DoBuildRequestComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);
  int DoDrainBodyForAuthRestart();
  int DoDrainBodyForAuthRestartComplete(int result);

  int BuildRequestHeaders(bool using_http_proxy_without_tunnel);

#if BUILDFLAG(ENABLE_REPORTING)
  // Processes the Report-To header, if one exists. This header configures where
  // the Reporting API (in //net/reporting) will send reports for the origin.
  void ProcessReportToHeader();

  // Processes the NEL header, if one exists. This header configures whether
  // network errors will be reported to a specified group of endpoints using the
  // Reporting API.
  void ProcessNetworkErrorLoggingHeader();

  // Calls GenerateNetworkErrorLoggingReport() if |rv| represents a NET_ERROR
  // other than ERR_IO_PENDING.
  void GenerateNetworkErrorLoggingReportIfError(int rv);

  // Generates a NEL report about this request.  The NetworkErrorLoggingService
  // will discard the report if there is no NEL policy registered for this
  // origin.
  void GenerateNetworkErrorLoggingReport(int rv);
#endif

  // Writes a log message to help debugging in the field when we block a proxy
  // response to a CONNECT request.
  void LogBlockedTunnelResponse(int response_code) const;

  // Called wherever ERR_HTTP_1_1_REQUIRED or
  // ERR_PROXY_HTTP_1_1_REQUIRED has to be handled.
  int HandleHttp11Required(int error);

  // Called to possibly handle a client authentication error. Sets next_state_
  // and returns OK if recovering from the error.  Otherwise, the same error
  // code is returned.
  int HandleSSLClientAuthError(int error);

  // Called to possibly recover from the given error.  Sets next_state_ and
  // returns OK if recovering from the error.  Otherwise, the same error code
  // is returned.
  int HandleIOError(int error);

  // Gets the response headers from the HttpStream.
  HttpResponseHeaders* GetResponseHeaders() const;

  // Called when the socket is unexpectedly closed.  Returns true if the request
  // should be resent in case of a socket reuse/close race.
  bool ShouldResendRequest() const;

  // Returns true if there have already been |kMaxRetryAttempts| retries for
  // HTTP2 or QUIC network errors, and no further retries should be attempted.
  bool HasExceededMaxRetries() const;

  // Increments the number of restarts and returns true if the restart may
  // proceed.
  bool CheckMaxRestarts();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RetryReason {
    kHttpRequestTimeout = 0,
    kHttpMisdirectedRequest = 1,
    kHttp11Required = 2,
    kSslClientAuthSignatureFailed = 3,
    kConnectionReset = 4,
    kConnectionClosed = 5,
    kConnectionAborted = 6,
    kSocketNotConnected = 7,
    kEmptyResponse = 8,
    kEarlyDataRejected = 9,
    kWrongVersionOnEarlyData = 10,
    kHttp2PingFailed = 11,
    kHttp2ServerRefusedStream = 12,
    // Entries 13, 14, 15 are removed.
    kQuicHandshakeFailed = 16,
    kQuicGoawayRequestCanBeRetried = 17,
    kQuicProtocolError = 18,
    kMaxValue = kQuicProtocolError,
  };
  static std::optional<RetryReason> GetRetryReasonForIOError(int error);

  // Resets the connection and the request headers for resend.  Called when
  // ShouldResendRequest() is true.
  void ResetConnectionAndRequestForResend(RetryReason retry_reason);

  // Sets up the state machine to restart the transaction with auth.
  void PrepareForAuthRestart(HttpAuth::Target target);

  // Called when we don't need to drain the response body or have drained it.
  // Resets |connection_| unless |keep_alive| is true, then calls
  // ResetStateForRestart.  Sets |next_state_| appropriately.
  void DidDrainBodyForAuthRestart(bool keep_alive);

  // Resets the members of the transaction so it can be restarted.
  void ResetStateForRestart();

  // Resets the members of the transaction, except |stream_|, which needs
  // to be maintained for multi-round auth.
  void ResetStateForAuthRestart();

  // Caches network error details from the stream if available
  // and resets the stream.
  void CacheNetErrorDetailsAndResetStream();

  // Returns true if we should try to add a Proxy-Authorization header
  bool ShouldApplyProxyAuth() const;

  // Returns true if we should try to add an Authorization header.
  bool ShouldApplyServerAuth() const;

  // Handles HTTP status code 401 or 407.
  // HandleAuthChallenge() returns a network error code, or OK on success.
  // May update |pending_auth_target_| or |response_.auth_challenge|.
  int HandleAuthChallenge();

  // Returns true if we have auth credentials for the given target.
  bool HaveAuth(HttpAuth::Target target) const;

  // Get the {scheme, host, path, port} for the authentication target
  GURL AuthURL(HttpAuth::Target target) const;

  // Returns true if this transaction is for a WebSocket handshake
  bool ForWebSocketHandshake() const;

  void CopyConnectionAttemptsFromStreamRequest();

  // Returns true if response "Content-Encoding" headers respect
  // "Accept-Encoding".
  bool ContentEncodingsValid() const;

  void ResumeAfterConnected(int result);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class QuicProtocolErrorRetryStatus {
    kNoRetryExceededMaxRetries = 0,
    kNoRetryHeaderReceived = 1,
    kNoRetryNoAlternativeService = 2,
    kRetryAltServiceBroken = 3,
    kRetryAltServiceNotBroken = 4,
    kMaxValue = kRetryAltServiceNotBroken,
  };

  static void SetProxyInfoInResponse(const ProxyInfo& proxy_info,
                                     HttpResponseInfo* response_info);

  scoped_refptr<HttpAuthController>
      auth_controllers_[HttpAuth::AUTH_NUM_TARGETS];

  // Whether this transaction is waiting for proxy auth, server auth, or is
  // not waiting for any auth at all. |pending_auth_target_| is read and
  // cleared by RestartWithAuth().
  HttpAuth::Target pending_auth_target_ = HttpAuth::AUTH_NONE;

  CompletionRepeatingCallback io_callback_;
  CompletionOnceCallback callback_;

  raw_ptr<HttpNetworkSession> session_;

  NetLogWithSource net_log_;

  // Reset to null at the start of the Read state machine.
  raw_ptr<const HttpRequestInfo> request_ = nullptr;

  // The requested URL.
  GURL url_;
  RequestPriority priority_;
  HttpResponseInfo response_;

  // Copied from |request_|, as it's needed after the response body has been
  // read.
  NetworkAnonymizationKey network_anonymization_key_;

  // |proxy_info_| is the ProxyInfo used by the HttpStreamRequest.
  ProxyInfo proxy_info_;

  std::unique_ptr<HttpStreamRequest> stream_request_;
  std::unique_ptr<HttpStream> stream_;

  // True if we've validated the headers that the stream parser has returned.
  bool headers_valid_ = false;

  // True if we can send the request over early data.
  bool can_send_early_data_ = false;

  // True if the client certificate for the server (rather than the proxy) was
  // configured in this transaction.
  bool configured_client_cert_for_server_ = false;

  // Previously observed bad certs when establishing a connection. If the caller
  // chooses to retry despite the error, future connection attempts will be
  // configured to ignore these errors.
  std::vector<SSLConfig::CertAndStatus> observed_bad_certs_;

  HttpRequestHeaders request_headers_;
#if BUILDFLAG(ENABLE_REPORTING)
  // Whether a NEL report has already been generated. Reset when restarting.
  bool network_error_logging_report_generated_ = false;
  // Cache some fields from |request_| that we'll need to construct a NEL
  // report about the request.  (NEL report construction happens after we've
  // cleared the |request_| pointer.)
  std::string request_method_;
  std::string request_referrer_;
  std::string request_user_agent_;
  int request_reporting_upload_depth_ = 0;
  base::TimeTicks start_timeticks_;
#endif

  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small error
  // page just a few hundred bytes long.
  static const int kDrainBodyBufferSize = 1024;

  // User buffer and length passed to the Read method.
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_ = 0;

  // Total number of bytes received on all destroyed HttpStreams for this
  // transaction.
  int64_t total_received_bytes_ = 0;

  // Total number of bytes sent on all destroyed HttpStreams for this
  // transaction.
  int64_t total_sent_bytes_ = 0;

  // When the transaction started creating a stream.
  base::TimeTicks create_stream_start_time_;

  // When the transaction started / finished sending the request, including
  // the body, if present. |send_start_time_| is set to |base::TimeTicks()|
  // until |SendRequest()| is called on |stream_|, and reset for auth restarts.
  base::TimeTicks send_start_time_;
  base::TimeTicks send_end_time_;

  // The next state in the state machine.
  State next_state_ = STATE_NONE;

  // True when the tunnel is in the process of being established - we can't
  // read from the socket until the tunnel is done.
  bool establishing_tunnel_ = false;

  // Enable pooling to a SpdySession with matching IP and certificate
  // even if the SpdySessionKey is different.
  bool enable_ip_based_pooling_ = true;

  // Enable using alternative services for the request.
  bool enable_alternative_services_ = true;

  // When a request is retried because of errors with the alternative service,
  // this will store the alternative service used.
  AlternativeService retried_alternative_service_;

  // The helper object to use to create WebSocketHandshakeStreamBase
  // objects. Only relevant when establishing a WebSocket connection.
  raw_ptr<WebSocketHandshakeStreamBase::CreateHelper>
      websocket_handshake_stream_base_create_helper_ = nullptr;

  BeforeNetworkStartCallback before_network_start_callback_;
  ConnectedCallback connected_callback_;
  RequestHeadersCallback request_headers_callback_;
  ResponseHeadersCallback early_response_headers_callback_;
  ResponseHeadersCallback response_headers_callback_;

  // The callback to modify the request header. They will be called just before
  // sending the request to the network.
  base::RepeatingCallback<void(HttpRequestHeaders*)> modify_headers_callbacks_;

  ConnectionAttempts connection_attempts_;
  IPEndPoint remote_endpoint_;
  // Network error details for this transaction.
  NetErrorDetails net_error_details_;

  // Number of retries made for network errors like ERR_HTTP2_PING_FAILED,
  // ERR_HTTP2_SERVER_REFUSED_STREAM, ERR_QUIC_HANDSHAKE_FAILED and
  // ERR_QUIC_PROTOCOL_ERROR. Currently we stop after 3 tries
  // (including the initial request) and fail the request.
  // This count excludes retries on reused sockets since a well
  // behaved server may time those out and thus the number
  // of times we can retry a request on reused sockets is limited.
  size_t retry_attempts_ = 0;

  // Number of times the transaction was restarted via a RestartWith* call.
  size_t num_restarts_ = 0;

  bool close_connection_on_destruction_ = false;

  // Set to true when the server required HTTP/1.1 fallback.
  bool http_1_1_was_required_ = false;

  // If set, these values are used as DNS resolution times, rather than
  // using DNS times coming from the established stream.
  base::TimeTicks dns_resolution_start_time_override_;
  base::TimeTicks dns_resolution_end_time_override_;

  base::TimeTicks blocked_initialize_stream_start_time_;
  base::TimeTicks blocked_generate_proxy_auth_token_start_time_;
  base::TimeTicks blocked_generate_server_auth_token_start_time_;

  // The number of bytes of the body received from network.
  int64_t received_body_bytes_ = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_TRANSACTION_H_
