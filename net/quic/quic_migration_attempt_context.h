// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_
#define NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_socket_address.h"

namespace net {

class QuicChromiumPacketReader;
class QuicChromiumPacketWriter;

// The cause of connection migration.
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

// Encapsulates the complete context and resources necessary for a single
// connection migration attempt.
class NET_EXPORT_PRIVATE QuicMigrationAttemptContext {
 public:
  // Note: This does not accept a `self_address` because that is not known until
  // the underlying datagram socket has been configured and connected. This
  // happens later in the connection migration process, at which point, it will
  // be set, and accessible, via the underlying socket.
  QuicMigrationAttemptContext(
      MigrationCause cause,
      handles::NetworkHandle from_network,
      handles::NetworkHandle target_network,
      const quic::QuicSocketAddress& target_peer_address,
      std::unique_ptr<QuicChromiumPacketReader> reader,
      std::unique_ptr<QuicChromiumPacketWriter> writer);
  ~QuicMigrationAttemptContext();

  QuicMigrationAttemptContext(const QuicMigrationAttemptContext&) = delete;
  QuicMigrationAttemptContext& operator=(const QuicMigrationAttemptContext&) =
      delete;

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

  // QuicChromiumPacketWriter holds a raw pointer to the underlying socket
  // shared with, and owned by, QuicChromiumPacketReader. Therefore, `writer_`
  // must be destroyed before `reader_`.
  std::unique_ptr<QuicChromiumPacketReader> reader_;
  std::unique_ptr<QuicChromiumPacketWriter> writer_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_MIGRATION_ATTEMPT_CONTEXT_H_
