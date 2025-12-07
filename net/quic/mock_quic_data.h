// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_QUIC_DATA_H_
#define NET_QUIC_MOCK_QUIC_DATA_H_

#include "net/quic/quic_test_packet_printer.h"
#include "net/socket/socket_test_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"

namespace net::test {

// Helper class to encapsulate MockReads and MockWrites for QUIC.
// Simplify ownership issues and the interaction with the MockSocketFactory.
//
// To use, construct an instance, call the `Add*` methods in the desired order,
// and then call `AddSocketDataToFactory(socket_factory)` to add a socket with
// the defined behavior to the socket factory. Alternately, use
// `InitializeAndGetSequencedSocketData()` and pass the result to a mock socket
// like `MockUDPClientSocket`.
//
// The MockQuicData instance must remain live until the socket is created and
// ultimately closed.
class MockQuicData {
 public:
  explicit MockQuicData(quic::ParsedQuicVersion version);
  ~MockQuicData();

  // Makes the Connect() call return |rv| either
  // synchronusly or asynchronously based on |mode|.
  void AddConnect(IoMode mode, int rv);

  void AddConnect(MockConnectCompleter* completer);

  // Adds a read at the next sequence number which will read |packet|
  // synchronously or asynchronously based on |mode|. The QuicReceivedPacket
  // version includes an ECN codepoint.
  void AddRead(IoMode mode, std::unique_ptr<quic::QuicReceivedPacket> packet);
  void AddRead(IoMode mode, std::unique_ptr<quic::QuicEncryptedPacket> packet);

  // Adds a read at the next sequence number which will return |rv| either
  // synchronously or asynchronously based on |mode|.
  void AddRead(IoMode mode, int rv);

  // Adds a pause, meaning that reads will return ERR_IO_PENDING until
  // `Resume()` is called. Read and write cannot both be paused simultaneously.
  void AddReadPause();

  // Like `AddReadPause`, but cannot be resumed.
  void AddReadPauseForever();

  // Adds a write at the next sequence number which will write |packet|
  // synchronously or asynchronously based on |mode|.
  void AddWrite(IoMode mode, std::unique_ptr<quic::QuicEncryptedPacket> packet);

  // Adds a write at the next sequence number which will return |rv| either
  // synchronously or asynchronously based on |mode|.
  void AddWrite(IoMode mode, int rv);

  // Adds a write at the next sequence number which will write |packet|
  // synchronously or asynchronously based on |mode| and return |rv|.
  void AddWrite(IoMode mode,
                int rv,
                std::unique_ptr<quic::QuicEncryptedPacket> packet);

  // Adds a pause, meaning that writes will return ERR_IO_PENDING until
  // `Resume()` is called. Read and write cannot both be paused simultaneously.
  void AddWritePause();

  // Adds the reads and writes to |factory|.
  void AddSocketDataToFactory(MockClientSocketFactory* factory);

  // Returns true if all reads have been consumed.
  bool AllReadDataConsumed();

  // Returns true if all writes have been consumed.
  bool AllWriteDataConsumed();

  // EXPECTs that all data has been consumed, printing any un-consumed data.
  void ExpectAllReadDataConsumed();
  void ExpectAllWriteDataConsumed();

  // Resumes I/O after it is paused.
  void Resume();

  // Creates a new `SequencedSocketData` owned by this instance of
  // `MockQuicData`. Returns a pointer to the newly created
  // `SequencedSocketData`.
  SequencedSocketData* InitializeAndGetSequencedSocketData();

  // Get the `SequencedSocketData` created by `AddSocketDataToFactory` or
  // `InitializeAndGetSequencedSocketData`.
  SequencedSocketData* GetSequencedSocketData();

 private:
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> packets_;
  std::unique_ptr<MockConnect> connect_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  size_t sequence_number_ = 0;
  std::unique_ptr<SequencedSocketData> socket_data_;
  QuicPacketPrinter printer_;
};

}  // namespace net::test

#endif  // NET_QUIC_MOCK_QUIC_DATA_H_
