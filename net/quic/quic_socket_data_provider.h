// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SOCKET_DATA_PROVIDER_H_
#define NET_QUIC_QUIC_SOCKET_DATA_PROVIDER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "net/quic/quic_test_packet_printer.h"
#include "net/socket/socket_test_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_compressed_certs_cache.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_server_stream_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_simple_server_session.h"

namespace net::test {

// A `QuicSimpleServerSession` that decodes the HTTP frame received from the
// QuicSocketDataProvider.
class QuicSimpleServerSessionForTest : public quic::QuicSimpleServerSession {
 public:
  using quic::QuicSimpleServerSession::QuicSimpleServerSession;
  ~QuicSimpleServerSessionForTest() override;

  bool IsEncryptionEstablished() const override;
};

// A `SocketDataProvider` specifically designed to handle QUIC's packet-based
// nature, and to give useful errors when things do not go as planned. This
// fills the same purpose as `MockQuicData` and it should be straightforward to
// "upgrade" a use of `MockQuicData` to this class when adding or modifying
// tests.
//
// To use: create a new `QuicSocketDataProvider`, then add expected reads and
// writes to it using the `AddRead` and `AddWrite` methods. Each read or write
// must have a short, unique name that will appear in logs and error messages.
// Once the provider is populated, add it to a `MockClientSocketFactory` with
// `AddSocketDataProvider`.
//
// Each `Add` method creates an "expectation" that some event will occur on the
// socket. A write expectation signals that the system under test will call
// `Write` with a packet matching the given data. A read expectation signals
// that the SUT will call `Read`, and the data in the expectation will be
// returned.
//
// Expectations can be adjusted when they are created by chaining method calls,
// such as setting the mode. Expectations are consumed in a partial order: each
// expectation specifies the expectations which must be consumed before it can
// be consumed. By default, each expectation must come after the previously
// added expectation, but the `After` method can be used to adjust this ordering
// for cases where the order is unimportant or might vary. For example, an ACK
// might be written before or after a read of stream data.
//
// To decode HTTP packets, two server sessions are created, one for the
// expected packets and one for the actual packets. If VerifyWriteData() failed
// the accumulated logs from both sessions will be printed.
//
// When a Write expectation is not met, such as write data not matching the
// expected packet, the Write call will result in `ERR_UNEXPECTED`.
//
// Use `--vmodule=quic_socket_data_provider*=1` in the test command-line to see
// additional logging from this module.
class QuicSocketDataProvider : public SocketDataProvider {
 public:
  class Expectation {
   public:
    enum class Type { READ, WRITE, PAUSE };

    Expectation(Expectation&) = delete;
    Expectation& operator=(Expectation&) = delete;
    Expectation(Expectation&&);
    Expectation& operator=(Expectation&&);
    ~Expectation();

    // Set the mode for this expectation, where the default is ASYNC. If a `Read
    // or `Write` call occurs for a sync expectation when its preconditions have
    // not been met, the test will fail.
    Expectation& Mode(IoMode mode) {
      mode_ = mode;
      return *this;
    }
    Expectation& Sync() {
      Mode(SYNCHRONOUS);
      return *this;
    }

    // Indicate that this expectation cannot be consumed until the named
    // expectation has been consumed.
    Expectation& After(std::string name);

    // Set the TOS byte for this expectation.
    Expectation& TosByte(uint8_t tos_byte) {
      tos_byte_ = tos_byte;
      return *this;
    }

    const std::string& name() const { return name_; }
    Type type() const { return type_; }
    bool consumed() const { return consumed_; }
    const std::set<std::string>& after() const { return after_; }
    int rv() const { return rv_; }
    const std::unique_ptr<quic::QuicEncryptedPacket>& packet() const {
      return packet_;
    }
    IoMode mode() const { return mode_; }
    uint8_t tos_byte() const { return tos_byte_; }

    static std::string TypeToString(Type type);

   protected:
    friend class QuicSocketDataProvider;

    Expectation(std::string name,
                Type type,
                int rv,
                std::unique_ptr<quic::QuicEncryptedPacket> packet);

    void set_name(std::string name) { name_ = name; }
    void Consume();

   private:
    // Name for this packet, used in sequencing and logging.
    std::string name_;

    // Type of expectation.
    Type type_;

    // True when this expectation has been consumed; that is, it has been
    // matched with a call to Read or Write and that call has returned or its
    // callback has been called.
    bool consumed_ = false;

    // Expectations which must be consumed before this one, by name.
    std::set<std::string> after_;

    int rv_;
    std::unique_ptr<quic::QuicEncryptedPacket> packet_;
    IoMode mode_ = ASYNC;
    uint8_t tos_byte_ = 0;
  };

  // A PausePoint is just the index into the array of expectations.
  using PausePoint = size_t;

  explicit QuicSocketDataProvider(quic::ParsedQuicVersion version);
  ~QuicSocketDataProvider() override;

  // Adds a read which will result in `packet`. A reference to the provided
  // expectation is returned, which can be used to update the settings for that
  // expectation. The more-specific version taking `QuicReceivedPacket` also
  // sets the TOS byte based on the packet's ECN codepoint.
  Expectation& AddRead(std::string name,
                       std::unique_ptr<quic::QuicReceivedPacket> packet);
  Expectation& AddRead(std::string name,
                       std::unique_ptr<quic::QuicEncryptedPacket> packet);

  // Adds a read error return. A reference to the provided expectation is
  // returned, which can be used to update the settings for that expectation.
  Expectation& AddReadError(std::string name, int rv);

  // Adds a write which will expect the given packet and return the given
  // result. A reference to the provided packet is returned, which can be used
  // to update the settings for the packet.
  Expectation& AddWrite(std::string name,
                        std::unique_ptr<quic::QuicEncryptedPacket> packet,
                        int rv = OK);

  // Adds a write error return. A reference to the provided expectation is
  // returned, which can be used to update the settings for that expectation.
  Expectation& AddWriteError(std::string name, int rv);

  // Adds a Pause point, returning a handle that can be used later to wait for
  // and resume execution. Any expectations that come "after" the pause point
  // will not be consumed until the pause is reached and execution is resumed.
  //
  // Note that this is not compatible with
  // `SequencedSocketData::RunUntilPaused()`.
  PausePoint AddPause(std::string name);

  // Checks if all data has been consumed.
  bool AllDataConsumed() const;

  // Run the main loop until the given pause point is reached. If a different
  // pause point is reached, this will fail. Note that the results of any
  // `Read` or `Write` calls before the pause point might not be complete, if
  // those results were delivered asynchronously.
  void RunUntilPause(PausePoint pause_point);

  // Resumes I/O after it is paused.
  void Resume();

  // Run the main loop until all expectations have been consumed. Note that the
  // results of any `Read` or `Write` calls might not be complete, if those
  // results were delivered asynchronously.
  void RunUntilAllConsumed();

  // SocketDataProvider implementation.
  MockRead OnRead() override;
  MockWriteResult OnWrite(const std::string& data) override;
  bool AllReadDataConsumed() const override;
  bool AllWriteDataConsumed() const override;
  void CancelPendingRead() override;
  void Reset() override;

 private:
  // Find indexes of expectations of the given type that are ready to consume.
  std::optional<size_t> FindReadyExpectations(Expectation::Type type);

  // Find a single ready operation, if any. Fails if multiple expectations of
  // the given type are ready. The corresponding expectation is marked as
  // consumed, and a task is scheduled to consume any expectations that become
  // ready as a result.
  std::optional<MockRead> ConsumeNextRead();
  std::optional<MockWriteResult> ConsumeNextWrite();

  // Consume any expectations that have become ready after a change to another
  // expectation. This is called in a task automatically after one or more calls
  // to `ExepctationsConsumed`.
  void MaybeConsumeExpectations();

  // Update state after an expectation has been consumed.
  void ExpectationConsumed();

  // Verify that the packet matches `write_pending_`.
  bool VerifyWriteData(QuicSocketDataProvider::Expectation& expectation);

  // Generate a comma-separated list of expectation names.
  std::string ExpectationList(const std::vector<size_t>& indices);

  // Generate a `QuicSimpleServerSession` for decrypting HTTP packets.
  std::unique_ptr<quic::QuicSimpleServerSession> GenSimpleServerSession();
  // Helper to print packet data with `QuicSimpleServerSession`.
  std::string PrintWithQuicSession(quic::QuicSimpleServerSession* session,
                                   std::string data);

  std::vector<Expectation> expectations_;
  bool pending_maybe_consume_expectations_ = false;
  std::map<size_t, std::set<size_t>> dependencies_;
  bool read_pending_ = false;
  std::optional<std::string> write_pending_ = std::nullopt;
  QuicPacketPrinter printer_;
  std::optional<size_t> paused_at_;
  std::unique_ptr<base::RunLoop> run_until_run_loop_;

  quic::test::MockQuicSessionVisitor owner_;
  quic::test::MockQuicCryptoServerStreamHelper stream_helper_;
  quic::QuicConfig config_;
  quic::test::MockQuicConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  quic::QuicCryptoServerConfig crypto_config_;
  quic::QuicCompressedCertsCache compressed_certs_cache_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<quic::QuicSimpleServerSession> session_for_actual_;
  std::unique_ptr<quic::QuicSimpleServerSession> session_for_expected_;
  std::string actual_log_;
  std::string expected_log_;

  base::WeakPtrFactory<QuicSocketDataProvider> weak_factory_{this};
};

}  // namespace net::test

#endif  // NET_QUIC_QUIC_SOCKET_DATA_PROVIDER_H_
