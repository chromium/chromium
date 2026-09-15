// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_
#define NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_socket_address.h"

namespace net {

class QuicChromiumPacketReader;
class QuicChromiumPacketWriter;

// The cause of connection migration.
//
// Deprecated: To be removed. Use QuicMigrationAttemptCause instead.
enum MigrationCause {
  UNKNOWN_CAUSE,
  ON_NETWORK_CONNECTED,                       // Direct migration.
  ON_NETWORK_DISCONNECTED,                    // Direct migration.
  ON_WRITE_ERROR,                             // Direct migration.
  ON_NETWORK_MADE_DEFAULT,                    // With probing.
  ON_MIGRATE_BACK_TO_DEFAULT_NETWORK,         // With probing.
  CHANGE_NETWORK_ON_PATH_DEGRADING,           // With probing.
  CHANGE_PORT_ON_PATH_DEGRADING,              // With probing.
  NEW_NETWORK_CONNECTED_POST_PATH_DEGRADING,  // With probing.
  ON_SERVER_PREFERRED_ADDRESS_AVAILABLE,      // With probing.
  MULTI_PORT_PATH,
  MIGRATION_CAUSE_MAX
};

NET_EXPORT_PRIVATE std::string MigrationCauseToString(MigrationCause cause);

// The cause of a connection migration attempt.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(QuicMigrationAttemptCause)
enum class QuicMigrationAttemptCause {
  kUnknown = 0,
  kOnNetworkDisconnected = 1,
  kOnWriteError = 2,
  kOnNetworkMadeDefault = 3,
  kOnMigrateBackToDefaultNetwork = 4,
  kChangeNetworkOnPathDegrading = 5,
  kChangePortOnPathDegrading = 6,
  kNewNetworkConnectedPostPathDegrading = 7,
  kOnServerPreferredAddressAvailable = 8,
  kMultiPortPath = 9,
  kMaxValue = kMultiPortPath,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicMigrationAttemptCause,//tools/metrics/histograms/metadata/net/histograms.xml:QuicMigrationAttemptCause)

NET_EXPORT_PRIVATE QuicMigrationAttemptCause
ToQuicMigrationAttemptCause(MigrationCause cause);

// Reasons why an eligible migration attempt failed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(QuicMigrationAttemptFailureReason)
enum class QuicMigrationAttemptFailureReason {
  kSocketConfigFailed = 0,
  kProbeTimeout = 1,
  kNoUnusedConnectionId = 2,
  kStatelessReset = 3,
  kProbeFailed = 4,
  kMaxValue = kProbeFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicMigrationAttemptFailureReason)

// Reasons why a migration attempt was ineligible.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(QuicMigrationAttemptIneligibleReason)
enum class QuicMigrationAttemptIneligibleReason {
  kIdleSession = 0,
  kAlreadyOnTargetNetwork = 1,
  kDisabledByServer = 2,
  kNoAlternateNetwork = 3,
  kDisabledByClient = 4,
  kHandshakeNotConfirmed = 5,
  kIdleMigrationPeriodExceeded = 6,
  kOnlyNonMigratableStreams = 7,
  kProxiedSession = 8,
  kSessionBecameIdleDuringProbing = 9,
  kTooManyMigrations = 10,
  kIdleMigrationPeriodExceededDuringProbing = 11,
  kDisconnectedDuringProbing = 12,
  kTooManyPacketReaders = 13,
  kSessionDestroyed = 14,
  kMaxValue = kSessionDestroyed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicMigrationAttemptIneligibleReason)

// Encapsulates the complete context and resources necessary for a single
// connection migration attempt.
class NET_EXPORT_PRIVATE QuicMigrationAttemptContext {
 public:
  enum class Outcome {
    kUnknown,
    kSuccess,
    kFailure,
    kIneligible,
    kSuperseded,
  };

  // Records an ineligible migration when an attempt is classified as such
  // without a context having been created.
  static void RecordIneligible(MigrationCause cause,
                               QuicMigrationAttemptIneligibleReason reason);

  // Note: This does not accept a `self_address` because that is not known until
  // the underlying datagram socket has been configured and connected. This
  // happens later in the connection migration process, at which point, it will
  // be set, and accessible, via the underlying socket.
  // `is_session_alive` must be non-null and is queried in the destructor to
  // determine whether the session is still alive if the attempt context is
  // destroyed while in flight. We inject this callback to avoid having an
  // explicit dependency on QuicChromiumClientSession, which simplifies unit
  // testing this class.
  QuicMigrationAttemptContext(
      MigrationCause cause,
      handles::NetworkHandle from_network,
      handles::NetworkHandle target_network,
      const quic::QuicSocketAddress& target_peer_address,
      std::unique_ptr<QuicChromiumPacketReader> reader,
      std::unique_ptr<QuicChromiumPacketWriter> writer,
      base::RepeatingCallback<bool()> is_session_alive);
  ~QuicMigrationAttemptContext();

  QuicMigrationAttemptContext(const QuicMigrationAttemptContext&) = delete;
  QuicMigrationAttemptContext& operator=(const QuicMigrationAttemptContext&) =
      delete;

  void SetSuccess();
  void SetFailure(QuicMigrationAttemptFailureReason reason);
  void SetIneligible(QuicMigrationAttemptIneligibleReason reason);
  void SetSuperseded(QuicMigrationAttemptCause cause);

  MigrationCause cause() const { return cause_; }
  handles::NetworkHandle from_network() const { return from_network_; }
  handles::NetworkHandle target_network() const { return target_network_; }
  const quic::QuicSocketAddress& target_peer_address() const {
    return target_peer_address_;
  }

  QuicChromiumPacketReader* reader() const { return reader_.get(); }
  QuicChromiumPacketWriter* writer() const { return writer_.get(); }

  std::unique_ptr<QuicChromiumPacketReader> ReleaseReader() {
    return std::move(reader_);
  }
  std::unique_ptr<QuicChromiumPacketWriter> ReleaseWriter() {
    return std::move(writer_);
  }

 private:
  const MigrationCause cause_;
  const handles::NetworkHandle from_network_;
  const handles::NetworkHandle target_network_;
  const quic::QuicSocketAddress target_peer_address_;

  Outcome outcome_ = Outcome::kUnknown;
  std::variant<std::monostate,
               QuicMigrationAttemptFailureReason,
               QuicMigrationAttemptIneligibleReason,
               QuicMigrationAttemptCause>
      outcome_details_;

  // QuicChromiumPacketWriter holds a raw pointer to the underlying socket
  // shared with, and owned by, QuicChromiumPacketReader. Therefore, `writer_`
  // must be destroyed before `reader_`.
  std::unique_ptr<QuicChromiumPacketReader> reader_;
  std::unique_ptr<QuicChromiumPacketWriter> writer_;

  base::RepeatingCallback<bool()> is_session_alive_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_
