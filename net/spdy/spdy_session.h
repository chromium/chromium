// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_H_
#define NET_SPDY_SPDY_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/stream_socket_handle.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/http2_priority_dependencies.h"
#include "net/spdy/multiplexed_session.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_write_queue.h"
#include "net/ssl/ssl_config_service.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_alt_svc_wire_format.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace net {

namespace test {
class SpdyStreamTest;
}

// TLS and other layers will chunk data at 16KB. Making the max frame size too
// small will lead to increased CPU/byte cost and overhead on both client/server
// due to excessive frames to process. Making this larger has diminishing
// returns as the data will be chunked elsewhere. We also want to ensure we are
// >= 2860B (~2* MSS => 2 packets) to avoid delayed ACKs. We will also account
// for the frame header size of 9B to prevent fragmentation when this is added.
// As a result we will use a 16KB - 9B max data frame size.
const int kMaxSpdyFrameChunkSize = (16 * 1024) - 9;

// Default value of spdy::SETTINGS_INITIAL_WINDOW_SIZE per protocol
// specification. A session is always created with this initial window size.
const int32_t kDefaultInitialWindowSize = 65535;

// Maximum number of concurrent streams we will create, unless the server
// sends a SETTINGS frame with a different value.
const size_t kInitialMaxConcurrentStreams = 100;

// If more than this many bytes have been read or more than that many
// milliseconds have passed, return ERR_IO_PENDING from ReadLoop.
const int kYieldAfterBytesRead = 32 * 1024;
const int kYieldAfterDurationMilliseconds = 20;

// First and last valid stream IDs. As we always act as the client,
// start at 1 for the first stream id.
const spdy::SpdyStreamId kFirstStreamId = 1;
const spdy::SpdyStreamId kLastStreamId = 0x7fffffff;

// Maximum number of capped frames that can be queued at any time.
// We measured how many queued capped frames were ever in the
// SpdyWriteQueue at one given time between 2019-08 and 2020-02.
// The numbers showed that in 99.94% of cases it would always
// stay below 10, and that it would exceed 1000 only in
// 10^-8 of cases. Therefore we picked 10000 as a number that will
// virtually never be hit in practice, while still preventing an
// attacker from growing this queue unboundedly.
const int kSpdySessionMaxQueuedCappedFrames = 10000;

// Default time to delay sending small receive window updates (can be
// configured through SetTimeToBufferSmallWindowUpdates()). Usually window
// updates are sent when half of the receive window has been processed by
// the client but in the case of a client that consumes the data slowly,
// this strategy alone would make servers consider the connection or stream
// idle.
constexpr base::TimeDelta kDefaultTimeToBufferSmallWindowUpdates =
    base::Seconds(5);

class NetLog;
class NetworkQualityEstimator;
class SpdyStream;
class SSLInfo;
class TransportSecurityState;

// NOTE: There is an enum called SpdyProtocolErrorDetails2 (also with numeric
// suffixes) in tools/metrics/histograms/enums.xml. Be sure to add new values
// there also.
enum SpdyProtocolErrorDetails {
  // http2::Http2DecoderAdapter::SpdyFramerError mappings.
  SPDY_ERROR_NO_ERROR = 0,
  SPDY_ERROR_INVALID_STREAM_ID = 38,
  SPDY_ERROR_INVALID_CONTROL_FRAME = 1,
  SPDY_ERROR_CONTROL_PAYLOAD_TOO_LARGE = 2,
  SPDY_ERROR_DECOMPRESS_FAILURE = 5,
  SPDY_ERROR_INVALID_PADDING = 39,
  SPDY_ERROR_INVALID_DATA_FRAME_FLAGS = 8,
  SPDY_ERROR_UNEXPECTED_FRAME = 31,
  SPDY_ERROR_INTERNAL_FRAMER_ERROR = 41,
  SPDY_ERROR_INVALID_CONTROL_FRAME_SIZE = 37,
  SPDY_ERROR_OVERSIZED_PAYLOAD = 40,

  // HttpDecoder or HttpDecoderAdapter error.
  SPDY_ERROR_HPACK_INDEX_VARINT_ERROR = 43,
  SPDY_ERROR_HPACK_NAME_LENGTH_VARINT_ERROR = 44,
  SPDY_ERROR_HPACK_VALUE_LENGTH_VARINT_ERROR = 45,
  SPDY_ERROR_HPACK_NAME_TOO_LONG = 46,
  SPDY_ERROR_HPACK_VALUE_TOO_LONG = 47,
  SPDY_ERROR_HPACK_NAME_HUFFMAN_ERROR = 48,
  SPDY_ERROR_HPACK_VALUE_HUFFMAN_ERROR = 49,
  SPDY_ERROR_HPACK_MISSING_DYNAMIC_TABLE_SIZE_UPDATE = 50,
  SPDY_ERROR_HPACK_INVALID_INDEX = 51,
  SPDY_ERROR_HPACK_INVALID_NAME_INDEX = 52,
  SPDY_ERROR_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_NOT_ALLOWED = 53,
  SPDY_ERROR_HPACK_INITIAL_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_LOW_WATER_MARK =
      54,
  SPDY_ERROR_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_ACKNOWLEDGED_SETTING = 55,
  SPDY_ERROR_HPACK_TRUNCATED_BLOCK = 56,
  SPDY_ERROR_HPACK_FRAGMENT_TOO_LONG = 57,
  SPDY_ERROR_HPACK_COMPRESSED_HEADER_SIZE_EXCEEDS_LIMIT = 58,
  SPDY_ERROR_STOP_PROCESSING = 59,
  // spdy::SpdyErrorCode mappings.
  STATUS_CODE_NO_ERROR = 41,
  STATUS_CODE_PROTOCOL_ERROR = 11,
  STATUS_CODE_INTERNAL_ERROR = 16,
  STATUS_CODE_FLOW_CONTROL_ERROR = 17,
  STATUS_CODE_SETTINGS_TIMEOUT = 32,
  STATUS_CODE_STREAM_CLOSED = 12,
  STATUS_CODE_FRAME_SIZE_ERROR = 21,
  STATUS_CODE_REFUSED_STREAM = 13,
  STATUS_CODE_CANCEL = 15,
  STATUS_CODE_COMPRESSION_ERROR = 42,
  STATUS_CODE_CONNECT_ERROR = 33,
  STATUS_CODE_ENHANCE_YOUR_CALM = 34,
  STATUS_CODE_INADEQUATE_SECURITY = 35,
  STATUS_CODE_HTTP_1_1_REQUIRED = 36,
  // Deprecated SpdyRstStrreamStatus mappings.
  STATUS_CODE_UNSUPPORTED_VERSION = 14,
  STATUS_CODE_STREAM_IN_USE = 18,
  STATUS_CODE_STREAM_ALREADY_CLOSED = 19,

  // SpdySession errors
  PROTOCOL_ERROR_UNEXPECTED_PING = 22,
  PROTOCOL_ERROR_RST_STREAM_FOR_NON_ACTIVE_STREAM = 23,
  PROTOCOL_ERROR_SPDY_COMPRESSION_FAILURE = 24,
  PROTOCOL_ERROR_REQUEST_FOR_SECURE_CONTENT_OVER_INSECURE_SESSION = 25,
  PROTOCOL_ERROR_SYN_REPLY_NOT_RECEIVED = 26,
  PROTOCOL_ERROR_INVALID_WINDOW_UPDATE_SIZE = 27,
  PROTOCOL_ERROR_RECEIVE_WINDOW_VIOLATION = 28,

  // Next free value.
  NUM_SPDY_PROTOCOL_ERROR_DETAILS = 60,
};
SpdyProtocolErrorDetails NET_EXPORT_PRIVATE MapFramerErrorToProtocolError(
    http2::Http2DecoderAdapter::SpdyFramerError error);
Error NET_EXPORT_PRIVATE
MapFramerErrorToNetError(http2::Http2DecoderAdapter::SpdyFramerError error);
SpdyProtocolErrorDetails NET_EXPORT_PRIVATE
MapRstStreamStatusToProtocolError(spdy::SpdyErrorCode error_code);
spdy::SpdyErrorCode NET_EXPORT_PRIVATE MapNetErrorToGoAwayStatus(Error err);

// If these compile asserts fail then SpdyProtocolErrorDetails needs
// to be updated with new values, as do the mapping functions above.
static_assert(28 == http2::Http2DecoderAdapter::LAST_ERROR,
              "SpdyProtocolErrorDetails / Spdy Errors mismatch");
static_assert(13 == spdy::SpdyErrorCode::ERROR_CODE_MAX,
              "SpdyProtocolErrorDetails / spdy::SpdyErrorCode mismatch");

// A helper class used to manage a request to create a stream.
class NET_EXPORT_PRIVATE SpdyStreamRequest {
 public:
  SpdyStreamRequest();

  SpdyStreamRequest(const SpdyStreamRequest&) = delete;
  SpdyStreamRequest& operator=(const SpdyStreamRequest&) = delete;

  // Calls CancelRequest().
  ~SpdyStreamRequest();

  // Returns the time when ConfirmHandshake() completed, if this request had to
  // wait for ConfirmHandshake().
  base::TimeTicks confirm_handshake_end() const {
    return confirm_handshake_end_;
  }

  // Starts the request to create a stream. If OK is returned, then
  // ReleaseStream() may be called. If ERR_IO_PENDING is returned,
  // then when the stream is created, |callback| will be called, at
  // which point ReleaseStream() may be called. Otherwise, the stream
  // is not created, an error is returned, and ReleaseStream() may not
  // be called.
  //
  // If |can_send_early| is true, this request is allowed to be sent over
  // TLS 1.3 0RTT without confirming the handshake.
  //
  // If OK is returned, must not be called again without
  // ReleaseStream() being called first. If ERR_IO_PENDING is
  // returned, must not be called again without CancelRequest() or
  // ReleaseStream() being called first. Otherwise, in case of an
  // immediate error, this may be called again.
  int StartRequest(SpdyStreamType type,
                   const base::WeakPtr<SpdySession>& session,
                   const GURL& url,
                   bool can_send_early,
                   RequestPriority priority,
                   const SocketTag& socket_tag,
                   const NetLogWithSource& net_log,
                   CompletionOnceCallback callback,
                   const NetworkTrafficAnnotationTag& traffic_annotation,
                   bool detect_broken_connection = false,
                   base::TimeDelta heartbeat_interval = base::Seconds(0));

  // Cancels any pending stream creation request. May be called
  // repeatedly.
  void CancelRequest();

  // Transfers the created stream (guaranteed to not be NULL) to the
  // caller. Must be called at most once after StartRequest() returns
  // OK or |callback| is called with OK. The caller must immediately
  // set a delegate for the returned stream (except for test code).
  base::WeakPtr<SpdyStream> ReleaseStream();

  // Changes the priority of the stream, or changes the priority of the queued
  // request in the session.
  void SetPriority(RequestPriority priority);

  const NetworkTrafficAnnotationTag traffic_annotation() const {
    return NetworkTrafficAnnotationTag(traffic_annotation_);
  }

 private:
  friend class SpdySession;

  void OnConfirmHandshakeComplete(int rv);

  // Called by |session_| when the stream attempt has finished
  // successfully.
  void OnRequestCompleteSuccess(const base::WeakPtr<SpdyStream>& stream);

  // Called by |session_| when the stream attempt has finished with an
  // error. Also called with ERR_ABORTED if |session_| is destroyed
  // while the stream attempt is still pending.
  void OnRequestCompleteFailure(int rv);

  // Accessors called by |session_|.
  SpdyStreamType type() const { return type_; }
  const GURL& url() const { return url_; }
  RequestPriority priority() const { return priority_; }
  const NetLogWithSource& net_log() const { return net_log_; }

  void Reset();

  SpdyStreamType type_;
  base::WeakPtr<SpdySession> session_;
  base::WeakPtr<SpdyStream> stream_;
  GURL url_;
  RequestPriority priority_;
  SocketTag socket_tag_;
  NetLogWithSource net_log_;
  CompletionOnceCallback callback_;
  MutableNetworkTrafficAnnotationTag traffic_annotation_;
  base::TimeTicks confirm_handshake_end_;
  bool detect_broken_connection_;
  base::TimeDelta heartbeat_interval_;

  base::WeakPtrFactory<SpdyStreamRequest> weak_ptr_factory_{this};
};

class NET_EXPORT SpdySession
    : public BufferedSpdyFramerVisitorInterface,
      public spdy::SpdyFramerDebugVisitorInterface,
      public MultiplexedSession,
      public HigherLayeredPool,
      public NetworkChangeNotifier::DefaultNetworkActiveObserver {
 public:
  // TODO(akalin): Use base::TickClock when it becomes available.
  typedef base::TimeTicks (*TimeFunc)();

  // Returns true if |new_hostname| can be pooled into an existing connection to
  // |old_hostname| associated with |ssl_info|.
  static bool CanPool(TransportSecurityState* transport_security_state,
                      const SSLInfo& ssl_info,
                      const SSLConfigService& ssl_config_service,
                      std::string_view old_hostname,
                      std::string_view new_hostname);

  // Create a new SpdySession.
  // |spdy_session_key| is the host/port that this session connects to, privacy
  // and proxy configuration settings that it's using.
  // |net_log| is the NetLog that we log network events to.
  SpdySession(const SpdySessionKey& spdy_session_key,
              HttpServerProperties* http_server_properties,
              TransportSecurityState* transport_security_state,
              SSLConfigService* ssl_config_service,
              const quic::ParsedQuicVersionVector& quic_supported_versions,
              bool enable_sending_initial_data,
              bool enable_ping_based_connection_checking,
              bool is_http_enabled,
              bool is_quic_enabled,
              size_t session_max_recv_window_size,
              int session_max_queued_capped_frames,
              const spdy::SettingsMap& initial_settings,
              bool enable_http2_settings_grease,
              const std::optional<SpdySessionPool::GreasedHttp2Frame>&
                  greased_http2_frame,
              bool http2_end_stream_with_data_frame,
              bool enable_priority_update,
              TimeFunc time_func,
              NetworkQualityEstimator* network_quality_estimator,
              NetLog* net_log);

  ~SpdySession() override;

  const HostPortPair& host_port_pair() const {
    return spdy_session_key_.host_port_proxy_pair().first;
  }
  const HostPortProxyPair& host_port_proxy_pair() const {
    return spdy_session_key_.host_port_proxy_pair();
  }
  const SpdySessionKey& spdy_session_key() const { return spdy_session_key_; }

  // Initialize the session with the given connection.
  //
  // |pool| is the SpdySessionPool that owns us.  Its lifetime must
  // strictly be greater than |this|.
  //
  // The session begins reading from |stream_socket_handle| on a subsequent
  // event loop iteration, so the SpdySession may close immediately afterwards
  // if the first read of |stream_socket_handle| fails.
  void InitializeWithSocketHandle(
      std::unique_ptr<StreamSocketHandle> stream_socket_handle,
      SpdySessionPool* pool);

  // Just like InitializeWithSocketHandle(), but for use when the session is not
  // on top of a socket pool, but instead directly on top of a socket, which the
  // session has sole ownership of, and is responsible for deleting directly
  // itself.
  void InitializeWithSocket(std::unique_ptr<StreamSocket> stream_socket,
                            const LoadTimingInfo::ConnectTiming& connect_timing,
                            SpdySessionPool* pool);

  // Parse ALPS application_data from TLS handshake.
  // Returns OK on success.  Return a net error code on failure, and closes the
  // connection with the same error code.
  int ParseAlps();

  // Check to see if this SPDY session can support an additional domain.
  // If the session is un-authenticated, then this call always returns true.
  // For SSL-based sessions, verifies that the server certificate in use by
  // this session provides authentication for the domain and no client
  // certificate or channel ID was sent to the original server during the SSL
  // handshake.  NOTE:  This function can have false negatives on some
  // platforms.
  // TODO(wtc): rename this function and the Net.SpdyIPPoolDomainMatch
  // histogram because this function does more than verifying domain
  // authentication now.
  bool VerifyDomainAuthentication(std::string_view domain) const;

  // Pushes the given producer into the write queue for
  // |stream|. |stream| is guaranteed to be activated before the
  // producer is used to produce its frame.
  void EnqueueStreamWrite(const base::WeakPtr<SpdyStream>& stream,
                          spdy::SpdyFrameType frame_type,
                          std::unique_ptr<SpdyBufferProducer> producer);

  // Returns true if this session is configured to send greased HTTP/2 frames.
  // For more details on greased frames, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  bool GreasedFramesEnabled() const;

  // Returns true if HEADERS frames on request streams should not have the
  // END_STREAM flag set, but instead an empty DATA frame with END_STREAM should
  // be sent afterwards to close the stream.  Does not apply to bidirectional or
  // proxy streams.
  bool EndStreamWithDataFrame() const {
    return http2_end_stream_with_data_frame_;
  }

  // Send greased frame, that is, a frame of reserved type.
  void EnqueueGreasedFrame(const base::WeakPtr<SpdyStream>& stream);

  // Returns whether HTTP/2 style priority information (stream dependency and
  // weight fields in HEADERS frames, and PRIORITY frames) should be sent.  True
  // unless |enable_priority_update_| is true and
  // SETTINGS_DEPRECATE_HTTP2_PRIORITIES with value 1 has been received from
  // server.  In particular, if it returns false, it will always return false
  // afterwards.
  bool ShouldSendHttp2Priority() const;

  // Returns whether PRIORITY_UPDATE frames should be sent.  False if
  // |enable_priority_update_| is false.  Otherwise, true before SETTINGS frame
  // is received from server, and true after SETTINGS frame is received if it
  // contained SETTINGS_DEPRECATE_HTTP2_PRIORITIES with value 1.  In particular,
  // if it returns false, it will always return false afterwards.
  bool ShouldSendPriorityUpdate() const;

  // Runs the handshake to completion to confirm the handshake with the server.
  // If ERR_IO_PENDING is returned, then when the handshake is confirmed,
  // |callback| will be called.
  int ConfirmHandshake(CompletionOnceCallback callback);

  // Creates and returns a HEADERS frame for |stream_id|.
  std::unique_ptr<spdy::SpdySerializedFrame> CreateHeaders(
      spdy::SpdyStreamId stream_id,
      RequestPriority priority,
      spdy::SpdyControlFlags flags,
      quiche::HttpHeaderBlock headers,
      NetLogSource source_dependency);

  // Creates and returns a SpdyBuffer holding a data frame with the given data.
  // Sets |*effective_len| to number of bytes sent, and |*end_stream| to the
  // value of the END_STREAM (also known as fin) flag.  Returns nullptr if
  // session is draining or if session or stream is stalled by flow control.
  std::unique_ptr<SpdyBuffer> CreateDataBuffer(spdy::SpdyStreamId stream_id,
                                               IOBuffer* data,
                                               int len,
                                               spdy::SpdyDataFlags flags,
                                               int* effective_len,
                                               bool* end_stream);

  // Send PRIORITY frames according to the new priority of an existing stream.
  void UpdateStreamPriority(SpdyStream* stream,
                            RequestPriority old_priority,
                            RequestPriority new_priority);

  // Close the stream with the given ID, which must exist and be
  // active. Note that that stream may hold the last reference to the
  // session.
  void CloseActiveStream(spdy::SpdyStreamId stream_id, int status);

  // Close the given created stream, which must exist but not yet be
  // active. Note that |stream| may hold the last reference to the
  // session.
  void CloseCreatedStream(const base::WeakPtr<SpdyStream>& stream, int status);

  // Send a RST_STREAM frame with the given status code and close the
  // stream with the given ID, which must exist and be active. Note
  // that that stream may hold the last reference to the session.
  void ResetStream(spdy::SpdyStreamId stream_id,
                   int error,
                   const std::string& description);

  // Check if a stream is active.
  bool IsStreamActive(spdy::SpdyStreamId stream_id) const;

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;

  // MultiplexedSession methods:
  int GetRemoteEndpoint(IPEndPoint* endpoint) override;
  bool GetSSLInfo(SSLInfo* ssl_info) const override;
  std::string_view GetAcceptChViaAlps(
      const url::SchemeHostPort& scheme_host_port) const override;

  // Returns the protocol negotiated via ALPN for the underlying socket.
  NextProto GetNegotiatedProtocol() const;

  // Send a WINDOW_UPDATE frame for a stream. Called by a stream
  // whenever receive window size is increased.
  void SendStreamWindowUpdate(spdy::SpdyStreamId stream_id,
                              uint32_t delta_window_size);

  // Configure the amount of time that small receive window updates should
  // be accumulated over (defaults to kDefaultTimeToBufferSmallWindowUpdates).
  void SetTimeToBufferSmallWindowUpdates(const base::TimeDelta buffer_time) {
    time_to_buffer_small_window_updates_ = buffer_time;
  }

  // Returns the configured time that small receive window updates should
  // be accumulated over.
  base::TimeDelta TimeToBufferSmallWindowUpdates() const {
    return time_to_buffer_small_window_updates_;
  }

  // Accessors for the session's availability state.
  bool IsAvailable() const { return availability_state_ == STATE_AVAILABLE; }
  bool IsGoingAway() const { return availability_state_ == STATE_GOING_AWAY; }
  bool IsDraining() const { return availability_state_ == STATE_DRAINING; }

  // Closes this session. This will close all active streams and mark
  // the session as permanently closed. Callers must assume that the
  // session is destroyed after this is called. (However, it may not
  // be destroyed right away, e.g. when a SpdySession function is
  // present in the call stack.)
  //
  // |err| should be < ERR_IO_PENDING; this function is intended to be
  // called on error.
  // |description| indicates the reason for the error.
  void CloseSessionOnError(Error err, const std::string& description);

  // Mark this session as unavailable, meaning that it will not be used to
  // service new streams. Unlike when a GOAWAY frame is received, this function
  // will not close any streams.
  void MakeUnavailable();

  // Closes all active streams with stream id's greater than
  // |last_good_stream_id|, as well as any created or pending
  // streams. Must be called only when |availability_state_| >=
  // STATE_GOING_AWAY. After this function, DcheckGoingAway() will
  // pass. May be called multiple times.
  void StartGoingAway(spdy::SpdyStreamId last_good_stream_id, Error status);

  // Must be called only when going away (i.e., DcheckGoingAway()
  // passes). If there are no more active streams and the session
  // isn't closed yet, close it.
  void MaybeFinishGoingAway();

  // Retrieves information on the current state of the SPDY session as a
  // Value.
  base::Value::Dict GetInfoAsValue() const;

  // Indicates whether the session is being reused after having successfully
  // used to send/receive data in the past or if the underlying socket was idle
  // before being used for a SPDY session.
  bool IsReused() const;

  // Returns true if the underlying transport socket ever had any reads or
  // writes.
  bool WasEverUsed() const { return socket_->WasEverUsed(); }

  // Returns the load timing information from the perspective of the given
  // stream.  If it's not the first stream, the connection is considered reused
  // for that stream.
  //
  // This uses a different notion of reuse than IsReused().  This function
  // sets |socket_reused| to false only if |stream_id| is the ID of the first
  // stream using the session.  IsReused(), on the other hand, indicates if the
  // session has been used to send/receive data at all.
  bool GetLoadTimingInfo(spdy::SpdyStreamId stream_id,
                         LoadTimingInfo* load_timing_info) const;

  // Returns true if session is currently active.
  bool is_active() const {
    return !active_streams_.empty() || !created_streams_.empty();
  }

  // True if the server supports WebSocket protocol.
  bool support_websocket() const { return support_websocket_; }

  // Returns true if no stream in the session can send data due to
  // session flow control.
  bool IsSendStalled() const { return session_send_window_size_ == 0; }

  const NetLogWithSource& net_log() const { return net_log_; }

  int GetPeerAddress(IPEndPoint* address) const;
  int GetLocalAddress(IPEndPoint* address) const;

  // Adds |alias| to set of aliases associated with this session.
  void AddPooledAlias(const SpdySessionKey& alias_key);

  // Removes |alias| from set of aliases associated with this session.
  void RemovePooledAlias(const SpdySessionKey& alias_key);

  // Returns the set of aliases associated with this session.
  const std::set<SpdySessionKey>& pooled_aliases() const {
    return pooled_aliases_;
  }

  // https://http2.github.io/http2-spec/#TLSUsage mandates minimum security
  // standards for TLS.
  bool HasAcceptableTransportSecurity() const;

  // Must be used only by |pool_|.
  base::WeakPtr<SpdySession> GetWeakPtr();

  // HigherLayeredPool implementation:
  bool CloseOneIdleConnection() override;

  // Change this session's socket tag to |new_tag|. Returns true on success.
  bool ChangeSocketTag(const SocketTag& new_tag);

  // Whether connection status monitoring is active or not.
  bool IsBrokenConnectionDetectionEnabled() const;

 private:
  friend class test::SpdyStreamTest;
  friend class base::RefCounted<SpdySession>;
  friend class HttpNetworkTransactionTest;
  friend class HttpProxyClientSocketPoolTest;
  friend class SpdyHttpStreamTest;
  friend class SpdyNetworkTransactionTest;
  friend class SpdyProxyClientSocketTest;
  friend class SpdySessionPoolTest;
  friend class SpdySessionTest;
  friend class SpdyStreamRequest;

  using PendingStreamRequestQueue =
      base::circular_deque<base::WeakPtr<SpdyStreamRequest>>;
  using ActiveStreamMap = std::map<spdy::SpdyStreamId, SpdyStream*>;
  using CreatedStreamSet = std::set<raw_ptr<SpdyStream>>;

  enum AvailabilityState {
    // The session is available in its socket pool and can be used
    // freely.
    STATE_AVAILABLE,
    // The session can process data on existing streams but will
    // refuse to create new ones.
    STATE_GOING_AWAY,
    // The session is draining its write queue in preparation of closing.
    // Further writes will not be queued, and further reads will not be issued
    // (though the remainder of a current read may be processed). The session
    // will be destroyed by its write loop once the write queue is drained.
    STATE_DRAINING,
  };

  enum ReadState {
    READ_STATE_DO_READ,
    READ_STATE_DO_READ_COMPLETE,
  };

  enum WriteState {
    // There is no in-flight write and the write queue is empty.
    WRITE_STATE_IDLE,
    WRITE_STATE_DO_WRITE,
    WRITE_STATE_DO_WRITE_COMPLETE,
  };

  // Has the shared logic for the other two Initialize methods that call it.
  void InitializeInternal(SpdySessionPool* pool);

  // Called by SpdyStreamRequest to start a request to create a
  // stream. If OK is returned, then |stream| will be filled in with a
  // valid stream. If ERR_IO_PENDING is returned, then
  // |request->OnRequestComplete{Success,Failure}()| will be called
  // when the stream is created (unless it is cancelled). Otherwise,
  // no stream is created and the error is returned.
  int TryCreateStream(const base::WeakPtr<SpdyStreamRequest>& request,
                      base::WeakPtr<SpdyStream>* stream);

  // Actually create a stream into |stream|. Returns OK if successful;
  // otherwise, returns an error and |stream| is not filled.
  int CreateStream(const SpdyStreamRequest& request,
                   base::WeakPtr<SpdyStream>* stream);

  // Called by SpdyStreamRequest to remove |request| from the stream
  // creation queue. Returns whether a request was removed from the queue.
  bool CancelStreamRequest(const base::WeakPtr<SpdyStreamRequest>& request);

  // Removes |request| from the stream creation queue and reinserts it into the
  // queue at the new |priority|.
  void ChangeStreamRequestPriority(
      const base::WeakPtr<SpdyStreamRequest>& request,
      RequestPriority priority);

  // Returns the next pending stream request to process, or NULL if
  // there is none.
  base::WeakPtr<SpdyStreamRequest> GetNextPendingStreamRequest();

  // Called when there is room to create more streams (e.g., a stream
  // was closed). Processes as many pending stream requests as
  // possible.
  void ProcessPendingStreamRequests();

  // Close the stream pointed to by the given iterator. Note that that
  // stream may hold the last reference to the session.
  void CloseActiveStreamIterator(ActiveStreamMap::iterator it, int status);

  // Close the stream pointed to by the given iterator. Note that that
  // stream may hold the last reference to the session.
  void CloseCreatedStreamIterator(CreatedStreamSet::iterator it, int status);

  // Calls EnqueueResetStreamFrame() and then
  // CloseActiveStreamIterator().
  void ResetStreamIterator(ActiveStreamMap::iterator it,
                           int status,
                           const std::string& description);

  // Send a RST_STREAM frame with the given parameters. There should
  // either be no active stream with the given ID, or that active
  // stream should be closed shortly after this function is called.
  void EnqueueResetStreamFrame(spdy::SpdyStreamId stream_id,
                               RequestPriority priority,
                               spdy::SpdyErrorCode error_code,
                               const std::string& description);

  // Send a PRIORITY frame with the given parameters.
  void EnqueuePriorityFrame(spdy::SpdyStreamId stream_id,
                            spdy::SpdyStreamId dependency_id,
                            int weight,
                            bool exclusive);

  // Calls DoReadLoop. Use this function instead of DoReadLoop when
  // posting a task to pump the read loop.
  void PumpReadLoop(ReadState expected_read_state, int result);

  // Advance the ReadState state machine. |expected_read_state| is the
  // expected starting read state.
  //
  // This function must always be called via PumpReadLoop().
  int DoReadLoop(ReadState expected_read_state, int result);
  // The implementations of the states of the ReadState state machine.
  int DoRead();
  int DoReadComplete(int result);

  // Calls DoWriteLoop. If |availability_state_| is STATE_DRAINING and no
  // writes remain, the session is removed from the session pool and
  // destroyed.
  //
  // Use this function instead of DoWriteLoop when posting a task to
  // pump the write loop.
  void PumpWriteLoop(WriteState expected_write_state, int result);

  // Iff the write loop is not currently active, posts a callback into
  // PumpWriteLoop().
  void MaybePostWriteLoop();

  // Advance the WriteState state machine. |expected_write_state| is
  // the expected starting write state.
  //
  // This function must always be called via PumpWriteLoop().
  int DoWriteLoop(WriteState expected_write_state, int result);
  // The implementations of the states of the WriteState state machine.
  int DoWrite();
  int DoWriteComplete(int result);

  void NotifyRequestsOfConfirmation(int rv);

  // TODO(akalin): Rename the Send* and Write* functions below to
  // Enqueue*.

  // Send initial data. Called when a connection is successfully
  // established in InitializeWithSocket() and
  // |enable_sending_initial_data_| is true.
  void SendInitialData();

  // Handle SETTING.  Either when we send settings, or when we receive a
  // SETTINGS control frame, update our SpdySession accordingly.
  void HandleSetting(uint32_t id, uint32_t value);

  // Adjust the send window size of all ActiveStreams and PendingStreamRequests.
  void UpdateStreamsSendWindowSize(int32_t delta_window_size);

  // Checks the connection status in an energy efficient manner:
  // * If the radio is in full power mode, send the PING immediately
  // * If the radio is in standby, record the event and send the PING once the
  //   radio wakes up
  // The radio status check is currently only implemented for Android devices,
  // on all other platforms the radio is assumed to be always active (i.e., no
  // batching happens).
  void MaybeCheckConnectionStatus();
  // Always checks the connection status and schedules the next check.
  void CheckConnectionStatus();
  // Send PING frame if all previous PING frames have been ACKed,
  // all posted CheckPingStatus() tasks have been executed,
  // and too long time has passed since last read from server.
  void MaybeSendPrefacePing();

  // Send a single WINDOW_UPDATE frame.
  void SendWindowUpdateFrame(spdy::SpdyStreamId stream_id,
                             uint32_t delta_window_size,
                             RequestPriority priority);

  // Send the PING frame.
  void WritePingFrame(spdy::SpdyPingId unique_id, bool is_ack);

  // Post a CheckPingStatus call after delay. Don't post if there is already
  // CheckPingStatus running.
  void PlanToCheckPingStatus();

  // Check the status of the connection. It calls |CloseSessionOnError| if we
  // haven't received any data in |kHungInterval| time period.
  void CheckPingStatus(base::TimeTicks last_check_time);

  // Get a new stream id.
  spdy::SpdyStreamId GetNewStreamId();

  // Pushes the given frame with the given priority into the write
  // queue for the session.
  void EnqueueSessionWrite(RequestPriority priority,
                           spdy::SpdyFrameType frame_type,
                           std::unique_ptr<spdy::SpdySerializedFrame> frame);

  // Puts |producer| associated with |stream| onto the write queue
  // with the given priority.
  void EnqueueWrite(RequestPriority priority,
                    spdy::SpdyFrameType frame_type,
                    std::unique_ptr<SpdyBufferProducer> producer,
                    const base::WeakPtr<SpdyStream>& stream,
                    const NetworkTrafficAnnotationTag& traffic_annotation);

  // Inserts a newly-created stream into |created_streams_|.
  void InsertCreatedStream(std::unique_ptr<SpdyStream> stream);

  // Activates |stream| (which must be in |created_streams_|) by
  // assigning it an ID and returns it.
  std::unique_ptr<SpdyStream> ActivateCreatedStream(SpdyStream* stream);

  // Inserts a newly-activated stream into |active_streams_|.
  void InsertActivatedStream(std::unique_ptr<SpdyStream> stream);

  // Remove all internal references to |stream|, call OnClose() on it,
  // and process any pending stream requests before deleting it.  Note
  // that |stream| may hold the last reference to the session.
  void DeleteStream(std::unique_ptr<SpdyStream> stream, int status);

  void RecordHistograms();
  void RecordProtocolErrorHistogram(SpdyProtocolErrorDetails details);

  // DCHECKs that |availability_state_| >= STATE_GOING_AWAY, that
  // there are no pending stream creation requests, and that there are
  // no created streams.
  void DcheckGoingAway() const;

  // Calls DcheckGoingAway(), then DCHECKs that |availability_state_|
  // == STATE_DRAINING and |error_on_close_| has a valid value.
  void DcheckDraining() const;

  // If the session is already draining, does nothing. Otherwise, moves
  // the session to the draining state.
  void DoDrainSession(Error err, const std::string& description);

  // Called right before closing a (possibly-inactive) stream for a
  // reason other than being requested to by the stream.
  void LogAbandonedStream(SpdyStream* stream, Error status);

  // Called right before closing an active stream for a reason other
  // than being requested to by the stream.
  void LogAbandonedActiveStream(ActiveStreamMap::const_iterator it,
                                Error status);

  // Invokes a user callback for stream creation.  We provide this method so it
  // can be deferred to the MessageLoop, so we avoid re-entrancy problems.
  void CompleteStreamRequest(
      const base::WeakPtr<SpdyStreamRequest>& pending_request);

  // BufferedSpdyFramerVisitorInterface:
  void OnError(
      http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error) override;
  void OnStreamError(spdy::SpdyStreamId stream_id,
                     const std::string& description) override;
  void OnPing(spdy::SpdyPingId unique_id, bool is_ack) override;
  void OnRstStream(spdy::SpdyStreamId stream_id,
                   spdy::SpdyErrorCode error_code) override;
  void OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                spdy::SpdyErrorCode error_code,
                std::string_view debug_data) override;
  void OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                         size_t length,
                         bool fin) override;
  void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                         const char* data,
                         size_t len) override;
  void OnStreamEnd(spdy::SpdyStreamId stream_id) override;
  void OnStreamPadding(spdy::SpdyStreamId stream_id, size_t len) override;
  void OnSettings() override;
  void OnSettingsAck() override;
  void OnSetting(spdy::SpdySettingsId id, uint32_t value) override;
  void OnSettingsEnd() override;
  void OnWindowUpdate(spdy::SpdyStreamId stream_id,
                      int delta_window_size) override;
  void OnPushPromise(spdy::SpdyStreamId stream_id,
                     spdy::SpdyStreamId promised_stream_id,
                     quiche::HttpHeaderBlock headers) override;
  void OnHeaders(spdy::SpdyStreamId stream_id,
                 bool has_priority,
                 int weight,
                 spdy::SpdyStreamId parent_stream_id,
                 bool exclusive,
                 bool fin,
                 quiche::HttpHeaderBlock headers,
                 base::TimeTicks recv_first_byte_time) override;
  void OnAltSvc(spdy::SpdyStreamId stream_id,
                std::string_view origin,
                const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector&
                    altsvc_vector) override;
  bool OnUnknownFrame(spdy::SpdyStreamId stream_id,
                      uint8_t frame_type) override;

  // spdy::SpdyFramerDebugVisitorInterface
  void OnSendCompressedFrame(spdy::SpdyStreamId stream_id,
                             spdy::SpdyFrameType type,
                             size_t payload_len,
                             size_t frame_len) override;
  void OnReceiveCompressedFrame(spdy::SpdyStreamId stream_id,
                                spdy::SpdyFrameType type,
                                size_t frame_len) override;

  // Called when bytes are consumed from a SpdyBuffer for a DATA frame
  // that is to be written or is being written. Increases the send
  // window size accordingly if some or all of the SpdyBuffer is being
  // discarded.
  //
  // If session flow control is turned off, this must not be called.
  void OnWriteBufferConsumed(size_t frame_payload_size,
                             size_t consume_size,
                             SpdyBuffer::ConsumeSource consume_source);

  // Called when the radio goes into full power mode. Currently implemented only
  // for Android devices.
  void OnDefaultNetworkActive() override;

  // Called by OnWindowUpdate() (which is in turn called by the
  // framer) to increase this session's send window size by
  // |delta_window_size| from a WINDOW_UPDATE frome, which must be at
  // least 1. If |delta_window_size| would cause this session's send
  // window size to overflow, does nothing.
  //
  // If session flow control is turned off, this must not be called.
  void IncreaseSendWindowSize(int delta_window_size);

  // If session flow control is turned on, called by CreateDataFrame()
  // (which is in turn called by a stream) to decrease this session's
  // send window size by |delta_window_size|, which must be at least 1
  // and at most kMaxSpdyFrameChunkSize.  |delta_window_size| must not
  // cause this session's send window size to go negative.
  //
  // If session flow control is turned off, this must not be called.
  void DecreaseSendWindowSize(int32_t delta_window_size);

  // Called when bytes are consumed by the delegate from a SpdyBuffer
  // containing received data. Increases the receive window size
  // accordingly.
  //
  // If session flow control is turned off, this must not be called.
  void OnReadBufferConsumed(size_t consume_size,
                            SpdyBuffer::ConsumeSource consume_source);

  // Called by OnReadBufferConsume to increase this session's receive
  // window size by |delta_window_size|, which must be at least 1 and
  // must not cause this session's receive window size to overflow,
  // possibly also sending a WINDOW_UPDATE frame. Also called during
  // initialization to set the initial receive window size.
  //
  // If session flow control is turned off, this must not be called.
  void IncreaseRecvWindowSize(int32_t delta_window_size);

  // Called by OnStreamFrameData (which is in turn called by the
  // framer) to decrease this session's receive window size by
  // |delta_window_size|, which must be at least 1 and must not cause
  // this session's receive window size to go negative.
  //
  // If session flow control is turned off, this must not be called.
  void DecreaseRecvWindowSize(int32_t delta_window_size);

  // Queue a send-stalled stream for possibly resuming once we're not
  // send-stalled anymore.
  void QueueSendStalledStream(const SpdyStream& stream);

  // Go through the queue of send-stalled streams and try to resume as
  // many as possible.
  void ResumeSendStalledStreams();

  // Returns the next stream to possibly resume, or 0 if the queue is
  // empty.
  spdy::SpdyStreamId PopStreamToPossiblyResume();

  // Enables connection status monitoring, causing the session to periodically
  // send a PING frame.
  // This must be called at most once for each stream requiring it. If called,
  // MaybeDisableBrokenConnectionDetection() will need to be called before
  // closing the requesting stream.
  // Note: `heartbeat_interval` should be considered a suggestion. The
  // implementation, for example, could either:
  // * Avoid sending a PING, if one has recently been transmitted or is
  //   already in flight
  // * Delay sending a PING, to avoid waking up the radio on mobile platforms
  // Only the first value of `heartbeat_interval` is taken into account.
  void EnableBrokenConnectionDetection(base::TimeDelta heartbeat_interval);

  // Requests to disable connection status monitoring. The service is disabled
  // only if no other active stream also requires it (an internal counter keeps
  // track of that).
  // This must be called once for each stream that requested it previously.
  void MaybeDisableBrokenConnectionDetection();

  // Whether Do{Read,Write}Loop() is in the call stack. Useful for
  // making sure we don't destroy ourselves prematurely in that case.
  bool in_io_loop_ = false;

  // The key used to identify this session.
  SpdySessionKey spdy_session_key_;

  // Set set of SpdySessionKeys for which this session has serviced
  // requests.
  std::set<SpdySessionKey> pooled_aliases_;

  // |pool_| owns us, therefore its lifetime must exceed ours.
  raw_ptr<SpdySessionPool> pool_ = nullptr;
  raw_ptr<HttpServerProperties> http_server_properties_;

  raw_ptr<TransportSecurityState> transport_security_state_;
  raw_ptr<SSLConfigService> ssl_config_service_;

  // One of these two owns the socket for this session, which is stored in
  // |socket_|. If |stream_socket_handle_| is non-null, this session is on top
  // of a socket in a socket pool. If |owned_stream_socket_| is non-null, this
  // session is directly on top of a socket, which is not in a socket pool.
  std::unique_ptr<StreamSocketHandle> stream_socket_handle_;
  std::unique_ptr<StreamSocket> owned_stream_socket_;

  // This is non-null only if |owned_stream_socket_| is non-null.
  std::unique_ptr<LoadTimingInfo::ConnectTiming> connect_timing_;

  // The socket for this session.
  raw_ptr<StreamSocket> socket_ = nullptr;

  // The read buffer used to read data from the socket.
  // Non-null if there is a Read() pending.
  scoped_refptr<IOBuffer> read_buffer_;

  spdy::SpdyStreamId stream_hi_water_mark_;  // The next stream id to use.

  // Queue, for each priority, of pending stream requests that have
  // not yet been satisfied.
  PendingStreamRequestQueue pending_create_stream_queues_[NUM_PRIORITIES];

  // Map from stream id to all active streams.  Streams are active in the sense
  // that they have a consumer (typically HttpNetworkTransaction and regardless
  // of whether or not there is currently any ongoing IO) or there are still
  // network events incoming even though the consumer has already gone away
  // (cancellation).
  //
  // |active_streams_| owns all its SpdyStream objects.
  //
  // TODO(willchan): Perhaps we should separate out cancelled streams and move
  // them into a separate ActiveStreamMap, and not deliver network events to
  // them?
  ActiveStreamMap active_streams_;

  // Set of all created streams but that have not yet sent any frames.
  //
  // |created_streams_| owns all its SpdyStream objects.
  CreatedStreamSet created_streams_;

  // The write queue.
  SpdyWriteQueue write_queue_;

  // Data for the frame we are currently sending.

  // The buffer we're currently writing.
  std::unique_ptr<SpdyBuffer> in_flight_write_;
  // The type of the frame in |in_flight_write_|.
  spdy::SpdyFrameType in_flight_write_frame_type_ = spdy::SpdyFrameType::DATA;
  // The size of the frame in |in_flight_write_|.
  size_t in_flight_write_frame_size_ = 0;
  // The stream to notify when |in_flight_write_| has been written to
  // the socket completely.
  base::WeakPtr<SpdyStream> in_flight_write_stream_;

  // Traffic annotation for the write in progress.
  MutableNetworkTrafficAnnotationTag in_flight_write_traffic_annotation_;

  // Spdy Frame state.
  std::unique_ptr<BufferedSpdyFramer> buffered_spdy_framer_;

  // The state variables.
  AvailabilityState availability_state_ = STATE_AVAILABLE;
  ReadState read_state_ = READ_STATE_DO_READ;
  WriteState write_state_ = WRITE_STATE_IDLE;

  // If the session is closing (i.e., |availability_state_| is STATE_DRAINING),
  // then |error_on_close_| holds the error with which it was closed, which
  // may be OK (upon a polite GOAWAY) or an error < ERR_IO_PENDING otherwise.
  // Initialized to OK.
  Error error_on_close_ = OK;

  // Settings that are sent in the initial SETTINGS frame
  // (if |enable_sending_initial_data_| is true),
  // and also control SpdySession parameters like initial receive window size
  // and maximum HPACK dynamic table size.
  const spdy::SettingsMap initial_settings_;

  // If true, a setting parameter with reserved identifier will be sent in every
  // initial SETTINGS frame, see
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  // The setting identifier and value will be drawn independently for each
  // connection to prevent tracking of the client.
  const bool enable_http2_settings_grease_;

  // If set, an HTTP/2 frame with a reserved frame type will be sent after
  // every HTTP/2 SETTINGS frame and before every HTTP/2 DATA frame. See
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00.
  const std::optional<SpdySessionPool::GreasedHttp2Frame> greased_http2_frame_;

  // If set, the HEADERS frame carrying a request without body will not have the
  // END_STREAM flag set.  The stream will be closed by a subsequent empty DATA
  // frame with END_STREAM.  Does not affect bidirectional or proxy streams.
  // If unset, the HEADERS frame will have the END_STREAM flag set on.
  // This is useful in conjuction with |greased_http2_frame_| so that a frame
  // of reserved type can be sent out even on requests without a body.
  const bool http2_end_stream_with_data_frame_;

  // If true, enable sending PRIORITY_UPDATE frames until SETTINGS frame
  // arrives.  After SETTINGS frame arrives, do not send PRIORITY_UPDATE frames
  // any longer if SETTINGS_DEPRECATE_HTTP2_PRIORITIES is missing or has zero 0,
  // but continue and also stop sending HTTP/2-style priority information in
  // HEADERS frames and PRIORITY frames if it has value 1.
  const bool enable_priority_update_;

  // The value of the last received SETTINGS_DEPRECATE_HTTP2_PRIORITIES, with 0
  // mapping to false and 1 to true.  Initial value is false.
  bool deprecate_http2_priorities_ = false;

  // True if at least one SETTINGS frame has been received.
  bool settings_frame_received_ = false;

  // The callbacks to notify a request that the handshake has been confirmed.
  std::vector<CompletionOnceCallback> waiting_for_confirmation_callbacks_;

  // True if there is an ongoing handshake confirmation with outstanding
  // requests.
  bool in_confirm_handshake_ = false;

  // Limits
  size_t max_concurrent_streams_;

  // Some statistics counters for the session.
  int streams_initiated_count_ = 0;

  int streams_abandoned_count_ = 0;

  // True if there has been a ping sent for which we have not received a
  // response yet.  There is always at most one ping in flight.
  bool ping_in_flight_ = false;

  // Triggers periodic connection status checks.
  base::OneShotTimer heartbeat_timer_;

  // Period used by the connection status monitoring mechanism.
  base::TimeDelta heartbeat_interval_;

  // True if the connection status should be checked once the radio wakes up.
  bool check_connection_on_radio_wakeup_ = false;

  // This is the next ping_id (unique_id) to be sent in PING frame.
  spdy::SpdyPingId next_ping_id_ = 1;

  // This is the last time we have sent a PING.
  base::TimeTicks last_ping_sent_time_;

  // This is the last time we had read activity in the session.
  base::TimeTicks last_read_time_;

  // This is the length of the last compressed frame.
  size_t last_compressed_frame_len_ = 0;

  // True if there is a CheckPingStatus() task posted on the message loop.
  bool check_ping_status_pending_ = false;

  // Current send window size.  Zero unless session flow control is turned on.
  int32_t session_send_window_size_ = 0;

  // Maximum receive window size.  Each time a WINDOW_UPDATE is sent, it
  // restores the receive window size to this value.  Zero unless session flow
  // control is turned on.
  int32_t session_max_recv_window_size_;

  // Maximum number of capped frames that can be queued at any time.
  // Every time we try to enqueue a capped frame, we check that there aren't
  // more than this amount already queued, and close the connection if so.
  int session_max_queued_capped_frames_;

  // Number of active requests which asked for connection status monitoring.
  int broken_connection_detection_requests_ = 0;

  // Sum of |session_unacked_recv_window_bytes_| and current receive window
  // size.  Zero unless session flow control is turned on.
  // TODO(bnc): Rename or change semantics so that |window_size_| is actual
  // window size.
  int32_t session_recv_window_size_ = 0;

  // When bytes are consumed, SpdyIOBuffer destructor calls back to SpdySession,
  // and this member keeps count of them until the corresponding WINDOW_UPDATEs
  // are sent.  Zero unless session flow control is turned on.
  int32_t session_unacked_recv_window_bytes_ = 0;

  // Time of the last WINDOW_UPDATE for the receive window.
  base::TimeTicks last_recv_window_update_;

  // Time to accumilate small receive window updates for.
  base::TimeDelta time_to_buffer_small_window_updates_;

  // Initial send window size for this session's streams. Can be
  // changed by an arriving SETTINGS frame. Newly created streams use
  // this value for the initial send window size.
  int32_t stream_initial_send_window_size_;

  // The maximum HPACK dynamic table size the server is allowed to set.
  uint32_t max_header_table_size_;

  // Initial receive window size for this session's streams. There are
  // plans to add a command line switch that would cause a SETTINGS
  // frame with window size announcement to be sent on startup. Newly
  // created streams will use this value for the initial receive
  // window size.
  int32_t stream_max_recv_window_size_;

  // A queue of stream IDs that have been send-stalled at some point
  // in the past.
  base::circular_deque<spdy::SpdyStreamId>
      stream_send_unstall_queue_[NUM_PRIORITIES];

  NetLogWithSource net_log_;

  // Versions of QUIC which may be used.
  const quic::ParsedQuicVersionVector quic_supported_versions_;

  // Outside of tests, these should always be true.
  const bool enable_sending_initial_data_;
  const bool enable_ping_based_connection_checking_;

  const bool is_http2_enabled_;
  const bool is_quic_enabled_;

  // True if the server has advertised WebSocket support via
  // spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL, see
  // https://tools.ietf.org/html/draft-ietf-httpbis-h2-websockets-00.
  bool support_websocket_ = false;

  // |connection_at_risk_of_loss_time_| is an optimization to avoid sending
  // wasteful preface pings (when we just got some data).
  //
  // If it is zero (the most conservative figure), then we always send the
  // preface ping (when none are in flight).
  //
  // It is common for TCP/IP sessions to time out in about 3-5 minutes.
  // Certainly if it has been more than 3 minutes, we do want to send a preface
  // ping.
  //
  // We don't think any connection will time out in under about 10 seconds. So
  // this might as well be set to something conservative like 10 seconds. Later,
  // we could adjust it to send fewer pings perhaps.
  base::TimeDelta connection_at_risk_of_loss_time_;

  // The amount of time that we are willing to tolerate with no activity (of any
  // form), while there is a ping in flight, before we declare the connection to
  // be hung. TODO(rtenneti): When hung, instead of resetting connection, race
  // to build a new connection, and see if that completes before we (finally)
  // get a PING response (http://crbug.com/127812).
  base::TimeDelta hung_interval_;

  TimeFunc time_func_;

  Http2PriorityDependencies priority_dependency_state_;

  // Map of origin to Accept-CH header field values received via ALPS.
  base::flat_map<url::SchemeHostPort, std::string>
      accept_ch_entries_received_via_alps_;

  // Network quality estimator to which the ping RTTs should be reported. May be
  // nullptr.
  raw_ptr<NetworkQualityEstimator> network_quality_estimator_;

  // Used for accessing the SpdySession from asynchronous tasks. An asynchronous
  // must check if its WeakPtr<SpdySession> is valid before accessing it, to
  // correctly handle the case where it became unavailable and was deleted.
  base::WeakPtrFactory<SpdySession> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_H_
