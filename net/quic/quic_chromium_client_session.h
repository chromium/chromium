// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific quic::QuicSession subclass.  This class owns the underlying
// quic::QuicConnection and QuicConnectionHelper objects.  The connection stores
// a non-owning pointer to the helper so this session needs to ensure that
// the helper outlives the connection.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_connection_logger.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http3_logger.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/spdy/http2_priority_dependencies.h"
#include "net/spdy/multiplexed_session.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_path_validator.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(ENABLE_WEBSOCKETS)
#include "net/websockets/websocket_basic_stream_adapters.h"
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

namespace net {

class CertVerifyResult;
class DatagramClientSocket;
struct ConnectionEndpointMetadata;
class QuicCryptoClientStreamFactory;
class QuicServerInfo;
class QuicSessionPool;
class SSLConfigService;
class SSLInfo;
class TransportSecurityState;

namespace test {
class QuicChromiumClientSessionPeer;
}  // namespace test

// SETTINGS_MAX_HEADERS_LIST_SIZE, the maximum size of uncompressed QUIC headers
// that the server is allowed to send.
const size_t kQuicMaxHeaderListSize = 256 * 1024;

// Result of a session migration attempt.
enum class MigrationResult {
  SUCCESS,         // Migration succeeded.
  NO_NEW_NETWORK,  // Migration failed since no new network was found.
  FAILURE,         // Migration failed for other reasons.
};

// Mode of connection migration.
enum class ConnectionMigrationMode {
  NO_MIGRATION,
  NO_MIGRATION_ON_PATH_DEGRADING_V1,
  FULL_MIGRATION_V1,
  NO_MIGRATION_ON_PATH_DEGRADING_V2,
  FULL_MIGRATION_V2
};

// Cause of a migration.
enum MigrationCause {
  UNKNOWN_CAUSE,
  ON_NETWORK_CONNECTED,                       // No probing.
  ON_NETWORK_DISCONNECTED,                    // No probing.
  ON_WRITE_ERROR,                             // No probing.
  ON_NETWORK_MADE_DEFAULT,                    // With probing.
  ON_MIGRATE_BACK_TO_DEFAULT_NETWORK,         // With probing.
  CHANGE_NETWORK_ON_PATH_DEGRADING,           // With probing.
  CHANGE_PORT_ON_PATH_DEGRADING,              // With probing.
  NEW_NETWORK_CONNECTED_POST_PATH_DEGRADING,  // With probing.
  ON_SERVER_PREFERRED_ADDRESS_AVAILABLE,      // With probing.
  MIGRATION_CAUSE_MAX
};

// Result of connection migration.
enum QuicConnectionMigrationStatus {
  MIGRATION_STATUS_NO_MIGRATABLE_STREAMS,
  MIGRATION_STATUS_ALREADY_MIGRATED,
  MIGRATION_STATUS_INTERNAL_ERROR,
  MIGRATION_STATUS_TOO_MANY_CHANGES,
  MIGRATION_STATUS_SUCCESS,
  MIGRATION_STATUS_NON_MIGRATABLE_STREAM,
  MIGRATION_STATUS_NOT_ENABLED,
  MIGRATION_STATUS_NO_ALTERNATE_NETWORK,
  MIGRATION_STATUS_ON_PATH_DEGRADING_DISABLED,
  MIGRATION_STATUS_DISABLED_BY_CONFIG,
  MIGRATION_STATUS_PATH_DEGRADING_NOT_ENABLED,
  MIGRATION_STATUS_TIMEOUT,
  MIGRATION_STATUS_ON_WRITE_ERROR_DISABLED,
  MIGRATION_STATUS_PATH_DEGRADING_BEFORE_HANDSHAKE_CONFIRMED,
  MIGRATION_STATUS_IDLE_MIGRATION_TIMEOUT,
  MIGRATION_STATUS_NO_UNUSED_CONNECTION_ID,
  MIGRATION_STATUS_MAX
};

// Result of a connectivity probing attempt.
enum class ProbingResult {
  PENDING,                          // Probing started, pending result.
  DISABLED_WITH_IDLE_SESSION,       // Probing disabled with idle session.
  DISABLED_BY_CONFIG,               // Probing disabled by config.
  DISABLED_BY_NON_MIGRABLE_STREAM,  // Probing disabled by special stream.
  INTERNAL_ERROR,                   // Probing failed for internal reason.
  FAILURE,                          // Probing failed for other reason.
};

// All possible combinations of observed ECN codepoints in a session. Several of
// these should not be sent by a well-behaved sender.
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class EcnPermutations {
  kUnknown = 0,
  kNotEct = 1,
  kEct1 = 2,
  kNotEctEct1 = 3,
  kEct0 = 4,
  kNotEctEct0 = 5,
  kEct1Ect0 = 6,
  kNotEctEct1Ect0 = 7,
  kCe = 8,
  kNotEctCe = 9,
  kEct1Ce = 10,
  kNotEctEct1Ce = 11,
  kEct0Ce = 12,
  kNotEctEct0Ce = 13,
  kEct1Ect0Ce = 14,
  kNotEctEct1Ect0Ce = 15,
  kMaxValue = kNotEctEct1Ect0Ce,
};

class NET_EXPORT_PRIVATE QuicChromiumClientSession
    : public quic::QuicSpdyClientSessionBase,
      public MultiplexedSession,
      public QuicChromiumPacketReader::Visitor,
      public QuicChromiumPacketWriter::Delegate {
 public:
  // Sets a callback that is called in the middle of a connection migration.
  // Only for testing.
  static void SetMidMigrationCallbackForTesting(base::OnceClosure callback);

  class StreamRequest;

  // An interface that when implemented and added via
  // AddConnectivityObserver(), provides notifications when connectivity
  // quality changes.
  class NET_EXPORT_PRIVATE ConnectivityObserver : public base::CheckedObserver {
   public:
    // Called when path degrading is detected on |network|.
    virtual void OnSessionPathDegrading(QuicChromiumClientSession* session,
                                        handles::NetworkHandle network) = 0;

    // Called when forward progress is made after path degrading on |network|.
    virtual void OnSessionResumedPostPathDegrading(
        QuicChromiumClientSession* session,
        handles::NetworkHandle network) = 0;

    // Called when |session| encounters write error on |network|.
    // A write error may be caused by the change in the underlying network
    // interface, and can be pre-emptive hints of connectivity quality changes
    // based on the |error_code|.
    virtual void OnSessionEncounteringWriteError(
        QuicChromiumClientSession* session,
        handles::NetworkHandle network,
        int error_code) = 0;

    // Called when |session| is closed by |source| with |error_code|
    // and handshake has been confirmed.
    virtual void OnSessionClosedAfterHandshake(
        QuicChromiumClientSession* session,
        handles::NetworkHandle network,
        quic::ConnectionCloseSource source,
        quic::QuicErrorCode error_code) = 0;

    // Called when |this| is registered to monitor the connectivity of the
    // |session|.
    virtual void OnSessionRegistered(QuicChromiumClientSession* session,
                                     handles::NetworkHandle network) = 0;

    // Called when |session| is removed.
    virtual void OnSessionRemoved(QuicChromiumClientSession* session) = 0;
  };

  // Wrapper for interacting with the session in a restricted fashion which
  // hides the details of the underlying session's lifetime. All methods of
  // the Handle are safe to use even after the underlying session is destroyed.
  class NET_EXPORT_PRIVATE Handle : public MultiplexedSessionHandle {
   public:
    // Constructs a handle to |session| which was created via the alternative
    // server |destination|.
    Handle(const base::WeakPtr<QuicChromiumClientSession>& session,
           url::SchemeHostPort destination);
    Handle(const Handle& other) = delete;
    ~Handle() override;

    // Returns true if the session is still connected.
    bool IsConnected() const;

    // Returns true if the handshake has been confirmed.
    bool OneRttKeysAvailable() const;

    // Starts a request to create a stream.  If OK is returned, then
    // |stream_| will be updated with the newly created stream.  If
    // ERR_IO_PENDING is returned, then when the request is eventuallly
    // complete |callback| will be called.
    int RequestStream(bool requires_confirmation,
                      CompletionOnceCallback callback,
                      const NetworkTrafficAnnotationTag& traffic_annotation);

    // Releases |stream_| to the caller. Returns nullptr if the underlying
    // QuicChromiumClientSession is closed.
    std::unique_ptr<QuicChromiumClientStream::Handle> ReleaseStream();

    // Returns a new packet bundler while will cause writes to be batched up
    // until a packet is full, or the last bundler is destroyed.
    std::unique_ptr<quic::QuicConnection::ScopedPacketFlusher>
    CreatePacketBundler();

    // Populates network error details for this session.
    void PopulateNetErrorDetails(NetErrorDetails* details) const;

    // Returns the connection timing for the handshake of this session.
    const LoadTimingInfo::ConnectTiming& GetConnectTiming();

    // Returns true if |other| is a handle to the same session as this handle.
    bool SharesSameSession(const Handle& other) const;

    // Returns the QUIC version used by the session.
    quic::ParsedQuicVersion GetQuicVersion() const;

    // Copies the remote udp address into |address| and returns a net error
    // code.
    int GetPeerAddress(IPEndPoint* address) const;

    // Copies the local udp address into |address| and returns a net error
    // code.
    int GetSelfAddress(IPEndPoint* address) const;

    // Returns the session's server ID.
    quic::QuicServerId server_id() const { return server_id_; }

    // Returns the alternative server used for this session.
    const url::SchemeHostPort& destination() const { return destination_; }

    // Returns the session's net log.
    const NetLogWithSource& net_log() const { return net_log_; }

    // Returns the session's connection migration mode.
    ConnectionMigrationMode connection_migration_mode() const {
      return session_->connection_migration_mode();
    }

    // Returns true if the session's connection has sent or received any bytes.
    bool WasEverUsed() const;

    // Retrieves any DNS aliases for the given session key from the map stored
    // in `session_pool_`. Includes all known aliases, e.g. from A, AAAA, or
    // HTTPS, not just from the address used for the connection, in no
    // particular order.
    const std::set<std::string>& GetDnsAliasesForSessionKey(
        const QuicSessionKey& key) const;

    // Returns the largest payload that will fit into a single MESSAGE frame at
    // any point during the connection.  This assumes the version and
    // connection ID lengths do not change. Returns zero if the session is
    // closed.
    quic::QuicPacketLength GetGuaranteedLargestMessagePayload() const;

#if BUILDFLAG(ENABLE_WEBSOCKETS)
    // This method returns nullptr on failure, such as when a new bidirectional
    // stream could not be made.
    std::unique_ptr<WebSocketQuicStreamAdapter>
    CreateWebSocketQuicStreamAdapter(
        WebSocketQuicStreamAdapter::Delegate* delegate,
        base::OnceCallback<void(std::unique_ptr<WebSocketQuicStreamAdapter>)>
            callback,
        const NetworkTrafficAnnotationTag& traffic_annotation);
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

   private:
    friend class QuicChromiumClientSession;
    friend class QuicChromiumClientSession::StreamRequest;

    // Waits for the handshake to be confirmed and invokes |callback| when
    // that happens. If the handshake has already been confirmed, returns OK.
    // If the connection has already been closed, returns a net error. If the
    // connection closes before the handshake is confirmed, |callback| will
    // be invoked with an error.
    int WaitForHandshakeConfirmation(CompletionOnceCallback callback);

    // Called when the handshake is confirmed.
    void OnCryptoHandshakeConfirmed();

    // Called when the session is closed with a net error.
    void OnSessionClosed(quic::ParsedQuicVersion quic_version,
                         int net_error,
                         quic::QuicErrorCode quic_error,
                         quic::ConnectionCloseSource source,
                         bool port_migration_detected,
                         bool quic_connection_migration_attempted,
                         bool quic_connection_migration_successful,
                         LoadTimingInfo::ConnectTiming connect_timing,
                         bool was_ever_used);

    // Called by |request| to create a stream.
    int TryCreateStream(StreamRequest* request);

    // Called by |request| to cancel stream request.
    void CancelRequest(StreamRequest* request);

    // Underlying session which may be destroyed before this handle.
    base::WeakPtr<QuicChromiumClientSession> session_;

    url::SchemeHostPort destination_;

    // Stream request created by |RequestStream()|.
    std::unique_ptr<StreamRequest> stream_request_;

    // Information saved from the session which can be used even after the
    // session is destroyed.
    NetLogWithSource net_log_;
    bool was_handshake_confirmed_;
    int net_error_ = OK;
    quic::QuicErrorCode quic_error_ = quic::QUIC_NO_ERROR;
    quic::ConnectionCloseSource source_ =
        quic::ConnectionCloseSource::FROM_SELF;
    bool port_migration_detected_ = false;
    bool quic_connection_migration_attempted_ = false;
    bool quic_connection_migration_successful_ = false;
    quic::QuicServerId server_id_;
    quic::ParsedQuicVersion quic_version_;
    LoadTimingInfo::ConnectTiming connect_timing_;

    bool was_ever_used_ = false;
  };

  // A helper class used to manage a request to create a stream.
  class NET_EXPORT_PRIVATE StreamRequest {
   public:
    StreamRequest(const StreamRequest&) = delete;
    StreamRequest& operator=(const StreamRequest&) = delete;

    // Cancels any pending stream creation request and resets |stream_| if
    // it has not yet been released.
    ~StreamRequest();

    // Starts a request to create a stream.  If OK is returned, then
    // |stream_| will be updated with the newly created stream.  If
    // ERR_IO_PENDING is returned, then when the request is eventuallly
    // complete |callback| will be called.
    int StartRequest(CompletionOnceCallback callback);

    // Releases |stream_| to the caller.
    std::unique_ptr<QuicChromiumClientStream::Handle> ReleaseStream();

    const NetworkTrafficAnnotationTag traffic_annotation() {
      return traffic_annotation_;
    }

   private:
    friend class QuicChromiumClientSession;

    enum State {
      STATE_NONE,
      STATE_WAIT_FOR_CONFIRMATION,
      STATE_WAIT_FOR_CONFIRMATION_COMPLETE,
      STATE_REQUEST_STREAM,
      STATE_REQUEST_STREAM_COMPLETE,
    };

    // |session| must outlive this request.
    StreamRequest(QuicChromiumClientSession::Handle* session,
                  bool requires_confirmation,
                  const NetworkTrafficAnnotationTag& traffic_annotation);

    void OnIOComplete(int rv);
    void DoCallback(int rv);

    int DoLoop(int rv);
    int DoWaitForConfirmation();
    int DoWaitForConfirmationComplete(int rv);
    int DoRequestStream();
    int DoRequestStreamComplete(int rv);

    // Called by |session_| for an asynchronous request when the stream
    // request has finished successfully.
    void OnRequestCompleteSuccess(
        std::unique_ptr<QuicChromiumClientStream::Handle> stream);

    // Called by |session_| for an asynchronous request when the stream
    // request has finished with an error. Also called with ERR_ABORTED
    // if |session_| is destroyed while the stream request is still pending.
    void OnRequestCompleteFailure(int rv);

    const raw_ptr<QuicChromiumClientSession::Handle> session_;
    const bool requires_confirmation_;
    CompletionOnceCallback callback_;
    std::unique_ptr<QuicChromiumClientStream::Handle> stream_;
    // For tracking how much time pending stream requests wait.
    base::TimeTicks pending_start_time_;
    State next_state_;

    const NetworkTrafficAnnotationTag traffic_annotation_;

#if BUILDFLAG(ENABLE_WEBSOCKETS)
    // For creation of streams for WebSockets over HTTP/3
    bool for_websockets_ = false;
    raw_ptr<WebSocketQuicStreamAdapter::Delegate> websocket_adapter_delegate_;
    base::OnceCallback<void(std::unique_ptr<WebSocketQuicStreamAdapter>)>
        start_websocket_callback_;
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

    base::WeakPtrFactory<StreamRequest> weak_factory_{this};
  };

  // This class contains all the context needed for path validation and
  // migration.
  class NET_EXPORT_PRIVATE QuicChromiumPathValidationContext
      : public quic::QuicPathValidationContext {
   public:
    QuicChromiumPathValidationContext(
        const quic::QuicSocketAddress& self_address,
        const quic::QuicSocketAddress& peer_address,
        handles::NetworkHandle network,
        std::unique_ptr<QuicChromiumPacketWriter> writer,
        std::unique_ptr<QuicChromiumPacketReader> reader);
    ~QuicChromiumPathValidationContext() override;

    handles::NetworkHandle network();
    quic::QuicPacketWriter* WriterToUse() override;

    // Transfer the ownership from |this| to the caller.
    std::unique_ptr<QuicChromiumPacketWriter> ReleaseWriter();
    std::unique_ptr<QuicChromiumPacketReader> ReleaseReader();

   private:
    handles::NetworkHandle network_handle_;
    std::unique_ptr<QuicChromiumPacketReader> reader_;
    std::unique_ptr<QuicChromiumPacketWriter> writer_;
  };

  // This class implements Chrome logic for path validation events associated
  // with connection migration.
  class NET_EXPORT_PRIVATE ConnectionMigrationValidationResultDelegate
      : public quic::QuicPathValidator::ResultDelegate {
   public:
    explicit ConnectionMigrationValidationResultDelegate(
        QuicChromiumClientSession* session);

    void OnPathValidationSuccess(
        std::unique_ptr<quic::QuicPathValidationContext> context,
        quic::QuicTime start_time) override;

    void OnPathValidationFailure(
        std::unique_ptr<quic::QuicPathValidationContext> context) override;

   private:
    // |session_| owns |this| and should out live |this|.
    raw_ptr<QuicChromiumClientSession> session_;
  };

  // This class implements Chrome logic for path validation events associated
  // with port migration.
  class NET_EXPORT_PRIVATE PortMigrationValidationResultDelegate
      : public quic::QuicPathValidator::ResultDelegate {
   public:
    explicit PortMigrationValidationResultDelegate(
        QuicChromiumClientSession* session);

    void OnPathValidationSuccess(
        std::unique_ptr<quic::QuicPathValidationContext> context,
        quic::QuicTime start_time) override;

    void OnPathValidationFailure(
        std::unique_ptr<quic::QuicPathValidationContext> context) override;

   private:
    // |session_| owns |this| and should out live |this|.
    raw_ptr<QuicChromiumClientSession> session_;
  };

  // This class implements Chrome logic for path validation events associated
  // with migrating to server preferred address.
  class NET_EXPORT_PRIVATE ServerPreferredAddressValidationResultDelegate
      : public quic::QuicPathValidator::ResultDelegate {
   public:
    explicit ServerPreferredAddressValidationResultDelegate(
        QuicChromiumClientSession* session);

    void OnPathValidationSuccess(
        std::unique_ptr<quic::QuicPathValidationContext> context,
        quic::QuicTime start_time) override;

    void OnPathValidationFailure(
        std::unique_ptr<quic::QuicPathValidationContext> context) override;

   private:
    // |session_| owns |this| and should out live |this|.
    raw_ptr<QuicChromiumClientSession> session_;
  };

  // This class is used to handle writer events that occur on the probing path.
  class NET_EXPORT_PRIVATE QuicChromiumPathValidationWriterDelegate
      : public QuicChromiumPacketWriter::Delegate {
   public:
    QuicChromiumPathValidationWriterDelegate(
        QuicChromiumClientSession* session,
        base::SequencedTaskRunner* task_runner);

    QuicChromiumPathValidationWriterDelegate(
        const QuicChromiumPathValidationWriterDelegate&) = delete;
    QuicChromiumPathValidationWriterDelegate& operator=(
        const QuicChromiumPathValidationWriterDelegate&) = delete;

    ~QuicChromiumPathValidationWriterDelegate();

    // QuicChromiumPacketWriter::Delegate interface.
    int HandleWriteError(
        int error_code,
        scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> last_packet)
        override;
    void OnWriteError(int error_code) override;
    void OnWriteUnblocked() override;

    void set_peer_address(const quic::QuicSocketAddress& peer_address);
    void set_network(handles::NetworkHandle network);

   private:
    void NotifySessionProbeFailed(handles::NetworkHandle network);

    // |session_| owns |this| and should out live |this|.
    raw_ptr<QuicChromiumClientSession> session_;
    // |task_owner_| should out live |this|.
    raw_ptr<base::SequencedTaskRunner> task_runner_;
    // The path validation context of the most recent probing.
    handles::NetworkHandle network_;
    quic::QuicSocketAddress peer_address_;
    base::WeakPtrFactory<QuicChromiumPathValidationWriterDelegate>
        weak_factory_{this};
  };

  // Constructs a new session which will own |connection|, but not
  // |session_pool|, which must outlive this session.
  // TODO(rch): decouple the factory from the session via a Delegate interface.
  //
  // If |require_confirmation| is true, the returned session will wait for a
  // successful QUIC handshake before vending any streams, to ensure that both
  // the server and the current network support QUIC, as HTTP fallback can't
  // trigger (or at least will take longer) after a QUIC stream has successfully
  // been created.
  //
  // For situations where no host resolution took place (such as a proxied
  // connection), the `dns_resolution_*_time` arguments should be equal and
  // the current time, and `endpoint_result` should be an empty value, with an
  // empty address list.
  // TODO(crbug.com/332924003): Delete the |report_ecn| argument when the
  // feature is deprecated.
  QuicChromiumClientSession(
      quic::QuicConnection* connection,
      std::unique_ptr<DatagramClientSocket> socket,
      QuicSessionPool* session_pool,
      QuicCryptoClientStreamFactory* crypto_client_stream_factory,
      const quic::QuicClock* clock,
      TransportSecurityState* transport_security_state,
      SSLConfigService* ssl_config_service,
      std::unique_ptr<QuicServerInfo> server_info,
      QuicSessionAliasKey session_alias_key,
      bool require_confirmation,
      bool migrate_sesion_early_v2,
      bool migrate_session_on_network_change_v2,
      handles::NetworkHandle default_network,
      quic::QuicTime::Delta retransmittable_on_wire_timeout,
      bool migrate_idle_session,
      bool allow_port_migration,
      base::TimeDelta idle_migration_period,
      int multi_port_probing_interval,
      base::TimeDelta max_time_on_non_default_network,
      int max_migrations_to_non_default_network_on_write_error,
      int max_migrations_to_non_default_network_on_path_degrading,
      int yield_after_packets,
      quic::QuicTime::Delta yield_after_duration,
      int cert_verify_flags,
      const quic::QuicConfig& config,
      std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config,
      const char* const connection_description,
      base::TimeTicks dns_resolution_start_time,
      base::TimeTicks dns_resolution_end_time,
      const base::TickClock* tick_clock,
      base::SequencedTaskRunner* task_runner,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      const ConnectionEndpointMetadata& metadata,
      bool report_ecn,
      bool enable_origin_frame,
      const NetLogWithSource& net_log);

  QuicChromiumClientSession(const QuicChromiumClientSession&) = delete;
  QuicChromiumClientSession& operator=(const QuicChromiumClientSession&) =
      delete;

  ~QuicChromiumClientSession() override;

  void Initialize() override;

  void AddHandle(Handle* handle);
  void RemoveHandle(Handle* handle);

  void AddConnectivityObserver(ConnectivityObserver* observer);
  void RemoveConnectivityObserver(ConnectivityObserver* observer);

  // Returns the session's connection migration mode.
  ConnectionMigrationMode connection_migration_mode() const;

  // Waits for the handshake to be confirmed and invokes |callback| when
  // that happens. If the handshake has already been confirmed, returns OK.
  // If the connection has already been closed, returns a net error. If the
  // connection closes before the handshake is confirmed, |callback| will
  // be invoked with an error.
  int WaitForHandshakeConfirmation(CompletionOnceCallback callback);

  // Attempts to create a new stream.  If the stream can be
  // created immediately, returns OK.  If the open stream limit
  // has been reached, returns ERR_IO_PENDING, and |request|
  // will be added to the stream requets queue and will
  // be completed asynchronously.
  // TODO(rch): remove |stream| from this and use setter on |request|
  // and fix in spdy too.
  int TryCreateStream(StreamRequest* request);

  // Cancels the pending stream creation request.
  void CancelRequest(StreamRequest* request);

  // QuicChromiumPacketWriter::Delegate override.
  int HandleWriteError(int error_code,
                       scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer>
                           last_packet) override;
  void OnWriteError(int error_code) override;
  // Called when the associated writer is unblocked. Write the cached |packet_|
  // if |packet_| is set. May send a PING packet if
  // |send_packet_after_migration_| is set and writer is not blocked after
  // writing queued packets.
  void OnWriteUnblocked() override;

  void OnConnectionMigrationProbeSucceeded(
      handles::NetworkHandle network,
      const quic::QuicSocketAddress& peer_address,
      const quic::QuicSocketAddress& self_address,
      std::unique_ptr<QuicChromiumPacketWriter> writer,
      std::unique_ptr<QuicChromiumPacketReader> reader);

  void OnPortMigrationProbeSucceeded(
      handles::NetworkHandle network,
      const quic::QuicSocketAddress& peer_address,
      const quic::QuicSocketAddress& self_address,
      std::unique_ptr<QuicChromiumPacketWriter> writer,
      std::unique_ptr<QuicChromiumPacketReader> reader);

  void OnServerPreferredAddressProbeSucceeded(
      handles::NetworkHandle network,
      const quic::QuicSocketAddress& peer_address,
      const quic::QuicSocketAddress& self_address,
      std::unique_ptr<QuicChromiumPacketWriter> writer,
      std::unique_ptr<QuicChromiumPacketReader> reader);

  void OnProbeFailed(handles::NetworkHandle network,
                     const quic::QuicSocketAddress& peer_address);

  // quic::QuicSpdySession methods:
  size_t WriteHeadersOnHeadersStream(
      quic::QuicStreamId id,
      quiche::HttpHeaderBlock headers,
      bool fin,
      const spdy::SpdyStreamPrecedence& precedence,
      quiche::QuicheReferenceCountedPointer<quic::QuicAckListenerInterface>
          ack_listener) override;
  void OnHttp3GoAway(uint64_t id) override;
  void OnAcceptChFrameReceivedViaAlps(
      const quic::AcceptChFrame& frame) override;
  void OnOriginFrame(const quic::OriginFrame& frame) override;

  // quic::QuicSession methods:
  QuicChromiumClientStream* CreateOutgoingBidirectionalStream() override;
  QuicChromiumClientStream* CreateOutgoingUnidirectionalStream() override;
  const quic::QuicCryptoClientStream* GetCryptoStream() const override;
  quic::QuicCryptoClientStream* GetMutableCryptoStream() override;
  void SetDefaultEncryptionLevel(quic::EncryptionLevel level) override;
  void OnTlsHandshakeComplete() override;
  void OnNewEncryptionKeyAvailable(
      quic::EncryptionLevel level,
      std::unique_ptr<quic::QuicEncrypter> encrypter) override;
  void OnCryptoHandshakeMessageSent(
      const quic::CryptoHandshakeMessage& message) override;
  void OnCryptoHandshakeMessageReceived(
      const quic::CryptoHandshakeMessage& message) override;
  void OnGoAway(const quic::QuicGoAwayFrame& frame) override;
  void OnCanCreateNewOutgoingStream(bool unidirectional) override;
  quic::QuicSSLConfig GetSSLConfig() const override;

  // QuicSpdyClientSessionBase methods:
  void OnProofValid(
      const quic::QuicCryptoClientConfig::CachedState& cached) override;
  void OnProofVerifyDetailsAvailable(
      const quic::ProofVerifyDetails& verify_details) override;

  // quic::QuicConnectionVisitorInterface methods:
  void OnConnectionClosed(const quic::QuicConnectionCloseFrame& frame,
                          quic::ConnectionCloseSource source) override;
  void OnSuccessfulVersionNegotiation(
      const quic::ParsedQuicVersion& version) override;
  void OnPathDegrading() override;
  void OnForwardProgressMadeAfterPathDegrading() override;
  void OnKeyUpdate(quic::KeyUpdateReason reason) override;
  void CreateContextForMultiPortPath(
      std::unique_ptr<quic::MultiPortPathContextObserver> context_observer)
      override;
  void MigrateToMultiPortPath(
      std::unique_ptr<quic::QuicPathValidationContext> context) override;

  // QuicChromiumPacketReader::Visitor methods:
  bool OnReadError(int result, const DatagramClientSocket* socket) override;
  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override;
  void OnStreamClosed(quic::QuicStreamId stream_id) override;

  // MultiplexedSession methods:
  int GetRemoteEndpoint(IPEndPoint* endpoint) override;
  bool GetSSLInfo(SSLInfo* ssl_info) const override;
  std::string_view GetAcceptChViaAlps(
      const url::SchemeHostPort& scheme_host_port) const override;

  // Helper for CreateContextForMultiPortPath. Gets the result of
  // ConnectAndConfigureSocket and uses it to create the multiport path context.
  void FinishCreateContextForMultiPortPath(
      std::unique_ptr<quic::MultiPortPathContextObserver> context_observer,
      std::unique_ptr<DatagramClientSocket> probing_socket,
      int rv);

  // Performs a crypto handshake with the server.
  int CryptoConnect(CompletionOnceCallback callback);

  // Causes the QuicConnectionHelper to start reading from all sockets
  // and passing the data along to the quic::QuicConnection.
  void StartReading();

  // Close the session because of |net_error| and notifies the factory
  // that this session has been closed, which will delete the session.
  // |behavior| will suggest whether we should send connection close packets
  // when closing the connection.
  void CloseSessionOnError(int net_error,
                           quic::QuicErrorCode quic_error,
                           quic::ConnectionCloseBehavior behavior);

  // Close the session because of |net_error| and notifies the factory
  // that this session has been closed later, which will delete the session.
  // |behavior| will suggest whether we should send connection close packets
  // when closing the connection.
  void CloseSessionOnErrorLater(int net_error,
                                quic::QuicErrorCode quic_error,
                                quic::ConnectionCloseBehavior behavior);

  base::Value::Dict GetInfoAsValue(const std::set<HostPortPair>& aliases);

  const NetLogWithSource& net_log() const { return net_log_; }

  // Returns true if the stream factory disables gQUIC 0-RTT.
  bool gquic_zero_rtt_disabled() const;

  // Returns a Handle to this session. Virtual for testing.
  virtual std::unique_ptr<QuicChromiumClientSession::Handle> CreateHandle(
      url::SchemeHostPort destination);

  // Returns the number of client hello messages that have been sent on the
  // crypto stream. If the handshake has completed then this is one greater
  // than the number of round-trips needed for the handshake.
  int GetNumSentClientHellos() const;

  // Returns true if |hostname| may be pooled onto this session.
  // |other_session_key| specifies the seession key associated with |hostname|
  // (its own hostname and port fields are ignored). If this is a secure QUIC
  // session, then |hostname| must match the certificate presented during the
  // handshake.
  bool CanPool(std::string_view hostname,
               const QuicSessionKey& other_session_key) const;

  const quic::QuicServerId& server_id() const {
    return session_key_.server_id();
  }

  const QuicSessionKey& quic_session_key() const { return session_key_; }

  const QuicSessionAliasKey& session_alias_key() const {
    return session_alias_key_;
  }

  // Attempts to migrate session when |writer| encounters a write error.
  // If |writer| is no longer actively used, abort migration.
  void MigrateSessionOnWriteError(int error_code,
                                  quic::QuicPacketWriter* writer);
  // Called when the Migrate() call from MigrateSessionOnWriteError completes.
  // Always called asynchronously.
  void FinishMigrateSessionOnWriteError(handles::NetworkHandle new_network,
                                        MigrationResult result);

  // Helper method that completes connection/server migration.
  // Unblocks packet writer on network level. If the writer becomes unblocked
  // then, OnWriteUnblocked() will be invoked to send packet after migration.
  void WriteToNewSocket();

  // Migrates session over to use |peer_address| and |network|.
  // If |network| is handles::kInvalidNetworkHandle, default network is used. If
  // the migration fails and |close_session_on_error| is true, session will be
  // closed.
  using MigrationCallback = base::OnceCallback<void(MigrationResult)>;
  void Migrate(handles::NetworkHandle network,
               IPEndPoint peer_address,
               bool close_session_on_error,
               MigrationCallback migration_callback);
  // Helper to finish session migration once a socket has been opened. Always
  // called asynchronously.
  void FinishMigrate(std::unique_ptr<DatagramClientSocket> socket,
                     IPEndPoint peer_address,
                     bool close_session_on_error,
                     MigrationCallback callback,
                     int rv);

  void DoMigrationCallback(MigrationCallback callback, MigrationResult rv);

  // Migrates session onto new socket, i.e., sets |writer| to be the new
  // default writer and post a task to write to |socket|. |reader| *must*
  // has been started reading from the socket. Returns true if
  // socket was successfully added to the session and the session was
  // successfully migrated to using the new socket. Returns true on
  // successful migration, or false if number of migrations exceeds
  // kMaxReadersPerQuicSession. Takes ownership of |socket|, |reader|,
  // and |writer|.
  bool MigrateToSocket(const quic::QuicSocketAddress& self_address,
                       const quic::QuicSocketAddress& peer_address,
                       std::unique_ptr<QuicChromiumPacketReader> reader,
                       std::unique_ptr<QuicChromiumPacketWriter> writer);

  // Called when NetworkChangeNotifier notifies observers of a newly
  // connected network. Migrates this session to the newly connected
  // network if the session has a pending migration.
  void OnNetworkConnected(handles::NetworkHandle network);

  // Called when NetworkChangeNotifier broadcasts to observers of
  // |disconnected_network|.
  void OnNetworkDisconnectedV2(handles::NetworkHandle disconnected_network);

  // Called when NetworkChangeNotifier broadcats to observers of a new default
  // network. Migrates this session to |new_network| if appropriate.
  void OnNetworkMadeDefault(handles::NetworkHandle new_network);

  // Schedules a migration alarm to wait for a new network.
  void OnNoNewNetwork();

  // Called when migration alarm fires. If migration has not occurred
  // since alarm was set, closes session with error.
  void OnMigrationTimeout(size_t num_sockets);

  // Populates network error details for this session.
  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // Returns current default socket. This is the socket over which all
  // QUIC packets are sent. This default socket can change, so do not store the
  // returned socket.
  const DatagramClientSocket* GetDefaultSocket() const;

  // Returns the network interface that is currently used to send packets.
  // If handles::NetworkHandle is not supported, always return
  // handles::kInvalidNetworkHandle.
  handles::NetworkHandle GetCurrentNetwork() const;

  // Override to validate |server_preferred_address| on a different socket.
  // Migrates to this address on validation succeeds.
  void OnServerPreferredAddressAvailable(
      const quic::QuicSocketAddress& server_preferred_address) override;

  const LoadTimingInfo::ConnectTiming& GetConnectTiming();

  quic::ParsedQuicVersion GetQuicVersion() const;

  bool require_confirmation() const { return require_confirmation_; }

  // Retrieves any DNS aliases for the given session key from the map stored
  // in `session_pool_`. Includes all known aliases, e.g. from A, AAAA, or
  // HTTPS, not just from the address used for the connection, in no particular
  // order.
  const std::set<std::string>& GetDnsAliasesForSessionKey(
      const QuicSessionKey& key) const;

  const std::set<url::SchemeHostPort>& received_origins() const {
    return received_origins_;
  }

  void SetGoingAwayForTesting(bool going_away) { going_away_ = going_away; }

 protected:
  // quic::QuicSession methods:
  bool ShouldCreateIncomingStream(quic::QuicStreamId id) override;
  bool ShouldCreateOutgoingBidirectionalStream() override;
  bool ShouldCreateOutgoingUnidirectionalStream() override;

  QuicChromiumClientStream* CreateIncomingStream(
      quic::QuicStreamId id) override;
  QuicChromiumClientStream* CreateIncomingStream(
      quic::PendingStream* pending) override;

 private:
  friend class test::QuicChromiumClientSessionPeer;

  typedef std::set<raw_ptr<Handle>> HandleSet;
  typedef std::list<raw_ptr<StreamRequest>> StreamRequestQueue;

  bool WasConnectionEverUsed();

  QuicChromiumClientStream* CreateOutgoingReliableStreamImpl(
      const NetworkTrafficAnnotationTag& traffic_annotation);
  QuicChromiumClientStream* CreateIncomingReliableStreamImpl(
      quic::QuicStreamId id,
      const NetworkTrafficAnnotationTag& traffic_annotation);
  QuicChromiumClientStream* CreateIncomingReliableStreamImpl(
      quic::PendingStream* pending,
      const NetworkTrafficAnnotationTag& traffic_annotation);
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);

  void NotifyAllStreamsOfError(int net_error);
  void CloseAllHandles(int net_error);
  void CancelAllRequests(int net_error);
  void NotifyRequestsOfConfirmation(int net_error);

  // Probe on <network, peer_address>.
  // If <network, peer_addres> is identical to the current path, the probe
  // is sent on a different port.
  using ProbingCallback = base::OnceCallback<void(ProbingResult)>;
  void StartProbing(ProbingCallback probing_callback,
                    handles::NetworkHandle network,
                    const quic::QuicSocketAddress& peer_address);

  // Helper to finish network probe once socket has been opened. Always called
  // asynchronously.
  void FinishStartProbing(ProbingCallback probing_callback,
                          std::unique_ptr<DatagramClientSocket> probing_socket,
                          handles::NetworkHandle network,
                          const quic::QuicSocketAddress& peer_address,
                          int rv);

  // Perform a few checks before StartProbing. If any of those checks fails,
  // StartProbing will be skipped.
  void MaybeStartProbing(ProbingCallback probing_callback,
                         handles::NetworkHandle network,
                         const quic::QuicSocketAddress& peer_address);

  // Helper method to perform a few checks and initiate connection migration
  // attempt when path degrading is detected.
  // Called when path is degrading and there is an alternate network or a new
  // network is connected after path degrading.
  void MaybeMigrateToAlternateNetworkOnPathDegrading();

  // Helper method to initiate a port migration on path degrading is detected.
  void MaybeMigrateToDifferentPortOnPathDegrading();

  // Called when there is only one possible working network: |network|, If any
  // error encountered, this session will be closed.
  // When the migration succeeds:
  //  - If no longer on the default network, set timer to migrate back to the
  //    default network;
  //  - If now on the default network, cancel timer to migrate back to default
  //    network.
  void MigrateNetworkImmediately(handles::NetworkHandle network);

  // Called when Migrate() call from MigrateNetworkImmediately completes. Always
  // called asynchronously.
  void FinishMigrateNetworkImmediately(handles::NetworkHandle network,
                                       MigrationResult result);

  void StartMigrateBackToDefaultNetworkTimer(base::TimeDelta delay);
  void CancelMigrateBackToDefaultNetworkTimer();
  void TryMigrateBackToDefaultNetwork(base::TimeDelta timeout);
  void FinishTryMigrateBackToDefaultNetwork(base::TimeDelta timeout,
                                            ProbingResult result);
  void MaybeRetryMigrateBackToDefaultNetwork();

  // If migrate idle session is enabled, returns true and post a task to close
  // the connection if session's idle time exceeds the |idle_migration_period_|.
  // If migrate idle session is not enabled, returns true and posts a task to
  // close the connection if session doesn't have outstanding streams.
  bool CheckIdleTimeExceedsIdleMigrationPeriod();

  // Close non-migratable streams in both directions by sending reset stream to
  // peer when connection migration attempts to migrate to the alternate
  // network.
  void ResetNonMigratableStreams();
  void LogMetricsOnNetworkDisconnected();
  void LogMetricsOnNetworkMadeDefault();
  void LogMigrationResultToHistogram(QuicConnectionMigrationStatus status);
  void LogHandshakeStatusOnMigrationSignal() const;
  void HistogramAndLogMigrationFailure(QuicConnectionMigrationStatus status,
                                       quic::QuicConnectionId connection_id,
                                       const char* reason);
  void HistogramAndLogMigrationSuccess(quic::QuicConnectionId connection_id);

  // Notifies the factory that this session is going away and no more streams
  // should be created from it.  This needs to be called before closing any
  // streams, because closing a stream may cause a new stream to be created.
  void NotifyFactoryOfSessionGoingAway();

  // Posts a task to notify the factory that this session has been closed.
  void NotifyFactoryOfSessionClosedLater();

  // Notifies the factory that this session has been closed which will
  // delete |this|.
  void NotifyFactoryOfSessionClosed();

  // Called when default encryption level switches to forward secure.
  void OnCryptoHandshakeComplete();

  void LogZeroRttStats();

#if BUILDFLAG(ENABLE_WEBSOCKETS)
  std::unique_ptr<WebSocketQuicStreamAdapter>
  CreateWebSocketQuicStreamAdapterImpl(
      WebSocketQuicStreamAdapter::Delegate* delegate);

  std::unique_ptr<WebSocketQuicStreamAdapter> CreateWebSocketQuicStreamAdapter(
      WebSocketQuicStreamAdapter::Delegate* delegate,
      base::OnceCallback<void(std::unique_ptr<WebSocketQuicStreamAdapter>)>
          callback,
      StreamRequest* stream_request);
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)

  const QuicSessionAliasKey session_alias_key_;
  QuicSessionKey session_key_;
  bool require_confirmation_;
  bool migrate_session_early_v2_;
  bool migrate_session_on_network_change_v2_;
  // True when session migration has started from MigrateSessionOnWriteError.
  bool pending_migrate_session_on_write_error_ = false;
  // True when a session migration starts from MigrateNetworkImmediately.
  bool pending_migrate_network_immediately_ = false;
  bool migrate_idle_session_;
  bool allow_port_migration_;
  // Session can be migrated if its idle time is within this period.
  base::TimeDelta idle_migration_period_;
  base::TimeDelta max_time_on_non_default_network_;
  // Maximum allowed number of migrations to non-default network triggered by
  // packet write error per default network.
  int max_migrations_to_non_default_network_on_write_error_;
  int current_migrations_to_non_default_network_on_write_error_ = 0;
  // Maximum allowed number of migrations to non-default network triggered by
  // path degrading per default network.
  int max_migrations_to_non_default_network_on_path_degrading_;
  int current_migrations_to_non_default_network_on_path_degrading_ = 0;
  raw_ptr<const quic::QuicClock> clock_;  // Unowned.
  int yield_after_packets_;
  quic::QuicTime::Delta yield_after_duration_;

  base::TimeTicks most_recent_path_degrading_timestamp_;
  base::TimeTicks most_recent_network_disconnected_timestamp_;
  raw_ptr<const base::TickClock> tick_clock_;
  base::TimeTicks most_recent_stream_close_time_;

  int most_recent_write_error_ = 0;
  base::TimeTicks most_recent_write_error_timestamp_;

  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config_;

  std::unique_ptr<quic::QuicCryptoClientStream> crypto_stream_;
  raw_ptr<QuicSessionPool> session_pool_;
  base::ObserverList<ConnectivityObserver> connectivity_observer_list_;
  std::vector<std::unique_ptr<QuicChromiumPacketReader>> packet_readers_;
  raw_ptr<TransportSecurityState> transport_security_state_;
  raw_ptr<SSLConfigService> ssl_config_service_;
  std::unique_ptr<QuicServerInfo> server_info_;
  std::unique_ptr<CertVerifyResult> cert_verify_result_;
  bool pkp_bypassed_ = false;
  bool is_fatal_cert_error_ = false;
  bool report_ecn_;
  const bool enable_origin_frame_;
  HandleSet handles_;
  StreamRequestQueue stream_requests_;
  std::vector<CompletionOnceCallback> waiting_for_confirmation_callbacks_;
  CompletionOnceCallback callback_;
  size_t num_total_streams_ = 0;
  raw_ptr<base::SequencedTaskRunner> task_runner_;
  NetLogWithSource net_log_;
  LoadTimingInfo::ConnectTiming connect_timing_;
  std::unique_ptr<QuicConnectionLogger> logger_;
  std::unique_ptr<QuicHttp3Logger> http3_logger_;
  // True when the session is going away, and streams may no longer be created
  // on this session. Existing stream will continue to be processed.
  bool going_away_ = false;
  // Connection close source
  quic::ConnectionCloseSource source_ = quic::ConnectionCloseSource::FROM_SELF;
  // True when the session receives a go away from server due to port migration.
  bool port_migration_detected_ = false;
  bool quic_connection_migration_attempted_ = false;
  bool quic_connection_migration_successful_ = false;
  // Stores the packet that witnesses socket write error. This packet will be
  // written to an alternate socket when the migration completes and the
  // alternate socket is unblocked.
  scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> packet_;
  // Stores the latest default network platform marks if migration is enabled.
  // Otherwise, stores the network interface that is used by the connection.
  handles::NetworkHandle default_network_;
  int retry_migrate_back_count_ = 0;
  base::OneShotTimer migrate_back_to_default_timer_;
  MigrationCause current_migration_cause_ = UNKNOWN_CAUSE;
  // True if a packet needs to be sent when packet writer is unblocked to
  // complete connection migration. The packet can be a cached packet if
  // |packet_| is set, a queued packet, or a PING packet.
  bool send_packet_after_migration_ = false;
  // True if migration is triggered, and there is no alternate network to
  // migrate to.
  bool wait_for_new_network_ = false;
  // True if read errors should be ignored. Set when migration on write error is
  // posted and unset until the first packet is written after migration.
  bool ignore_read_error_ = false;

  bool attempted_zero_rtt_ = false;

  size_t num_migrations_ = 0;

  // The reason for the last 1-RTT key update on the connection. Will be
  // kInvalid if no key updates have occurred.
  quic::KeyUpdateReason last_key_update_reason_ =
      quic::KeyUpdateReason::kInvalid;

  QuicChromiumPathValidationWriterDelegate path_validation_writer_delegate_;

  // Map of origin to Accept-CH header field values received via ALPS.
  base::flat_map<url::SchemeHostPort, std::string>
      accept_ch_entries_received_via_alps_;

  // Stores origins received in ORIGIN frame.
  std::set<url::SchemeHostPort> received_origins_;

  std::vector<uint8_t> ech_config_list_;

  // Bitmap of incoming IP ECN marks observed on this session. Bit 0 = Not-ECT,
  // Bit 1 = ECT(1), Bit 2 = ECT(0), Bit 3 = CE. Reported to metrics at the
  // end of the session.
  uint8_t observed_incoming_ecn_ = 0;

  // The number of incoming packets in this session before it observes a change
  // in the incoming packet ECN marking.
  uint64_t incoming_packets_before_ecn_transition_ = 0;

  // When true, the session has observed a transition and can stop incrementing
  // incoming_packets_before_ecn_transition_.
  bool observed_ecn_transition_ = false;

  base::WeakPtrFactory<QuicChromiumClientSession> weak_factory_{this};
};

namespace features {

// When enabled, network disconnect signals don't trigger immediate migration
// when there is an ongoing migration with probing.
NET_EXPORT BASE_DECLARE_FEATURE(
    kQuicMigrationIgnoreDisconnectSignalDuringProbing);

}  // namespace features

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_H_
