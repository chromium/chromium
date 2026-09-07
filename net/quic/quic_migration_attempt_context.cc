// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_migration_attempt_context.h"

#include <utility>

#include "base/check.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"

namespace net {

QuicMigrationAttemptContext::QuicMigrationAttemptContext(
    MigrationCause cause,
    handles::NetworkHandle from_network,
    handles::NetworkHandle target_network,
    const quic::QuicSocketAddress& target_peer_address,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer)
    : cause_(cause),
      from_network_(from_network),
      target_network_(target_network),
      target_peer_address_(target_peer_address),
      reader_(std::move(reader)),
      writer_(std::move(writer)) {
  CHECK(reader_);
  CHECK(writer_);
}

QuicMigrationAttemptContext::~QuicMigrationAttemptContext() = default;

}  // namespace net
