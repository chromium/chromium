// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"
#include "third_party/webrtc/rtc_base/rtccertificate.h"

namespace blink {

namespace {

// The P2PQuicPacketWriter is a private helper class that implements the
// QuicPacketWriter using a P2PQuicPacketTransport. This allows us to
// connect our own packet transport for writing into the QuicConnection.
// The normal case is using an ICE transport (packet_transport) for writing.
class P2PQuicPacketWriter : public quic::QuicPacketWriter,
                            public P2PQuicPacketTransport::WriteObserver {
 public:
  P2PQuicPacketWriter(P2PQuicPacketTransport* packet_transport)
      : packet_transport_(packet_transport) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(packet_transport_);
    packet_transport_->SetWriteObserver(this);
  }

  // This way the packet transport knows it no longer has a write observer and
  // can DCHECK this on destruction.
  ~P2PQuicPacketWriter() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    packet_transport_->SetWriteObserver(nullptr);
  }

  // Sets the QuicConnection (which owns this packet writer). This allows us
  // to get the packet numbers of QUIC packets we write. The QuicConnection
  // is created with a quic::QuicPacketWriter, so we can't set the connection
  // in the constructor.
  void InitializeWithQuicConnection(quic::QuicConnection* connection) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(connection);
    if (packet_transport_->Writable()) {
      SetWritable();
    }
    connection_ = connection;
  }

  // quic::QuicPacketWriter overrides.

  // Writes a QUIC packet to the network with the packet number as additional
  // packet  info.
  quic::WriteResult WritePacket(const char* buffer,
                                size_t buf_len,
                                const quic::QuicIpAddress& self_address,
                                const quic::QuicSocketAddress& peer_address,
                                quic::PerPacketOptions* options) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(connection_);
    if (IsWriteBlocked()) {
      return quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK);
    }

    P2PQuicPacketTransport::QuicPacket packet;
    packet.packet_number = connection_->packet_generator().packet_number();
    packet.buffer = buffer;
    packet.buf_len = buf_len;
    int bytes_written = packet_transport_->WritePacket(packet);
    if (bytes_written <= 0) {
      writable_ = false;
      return quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK);
    }
    return quic::WriteResult(quic::WRITE_STATUS_OK, bytes_written);
  }

  bool IsWriteBlockedDataBuffered() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return false;
  }

  bool IsWriteBlocked() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return !writable_;
  }

  quic::QuicByteCount GetMaxPacketSize(
      const quic::QuicSocketAddress& peer_address) const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    // This can be configured later.
    return 1200;
  }

  void SetWritable() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    writable_ = true;
  }

  bool SupportsReleaseTime() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return false;
  }

  bool IsBatchMode() const override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return false;
  }

  char* GetNextWriteLocation(
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return nullptr;
  }

  quic::WriteResult Flush() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return quic::WriteResult(quic::WRITE_STATUS_OK, 0);
  }

  // P2PQuicPacketTransport::WriteDelegate override.
  void OnCanWrite() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    SetWritable();
    connection_->OnCanWrite();
  }

 private:
  // The packet transport is owned by the P2PQuicSession, not the
  // BlinkPacketWriter.
  P2PQuicPacketTransport* packet_transport_;
  // The QuicConnection owns this packet writer and will outlive it.
  quic::QuicConnection* connection_;

  bool writable_ = false;
  THREAD_CHECKER(thread_checker_);
};

// Creates the QuicConnection for the QuicSession. Currently this connection
// uses a dummy address and ID. The |packet_writer| is a basic implementation
// using the QuicTransportConfig::packet_transport for writing. The |helper|
// and |alarm_factory| should be chromium specific implementations.
std::unique_ptr<quic::QuicConnection> CreateQuicConnection(
    bool is_server,
    quic::QuicConnectionHelperInterface* helper,
    quic::QuicPacketWriter* packet_writer,
    quic::QuicAlarmFactory* alarm_factory) {
  quic::QuicIpAddress ip;
  ip.FromString("0.0.0.0");
  quic::QuicSocketAddress dummy_address(ip, 0 /* Port */);
  quic::Perspective perspective =
      is_server ? quic::Perspective::IS_SERVER : quic::Perspective::IS_CLIENT;
  return std::make_unique<quic::QuicConnection>(
      0 /* dummy ID */, dummy_address, helper, alarm_factory, packet_writer,
      /* owns_writer */ true, perspective, quic::CurrentSupportedVersions());
}
}  // namespace

P2PQuicTransportFactoryImpl::P2PQuicTransportFactoryImpl(
    quic::QuicClock* clock,
    std::unique_ptr<quic::QuicAlarmFactory> alarm_factory)
    : clock_(clock), alarm_factory_(std::move(alarm_factory)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// The P2PQuicTransportImpl is created with Chromium specific QUIC objects:
// QuicClock, QuicRandom, QuicConnectionHelper and QuicAlarmFactory.
std::unique_ptr<P2PQuicTransport>
P2PQuicTransportFactoryImpl::CreateQuicTransport(
    P2PQuicTransportConfig config) {
  DCHECK(config.packet_transport);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  quic::QuicRandom* quic_random = quic::QuicRandom::GetInstance();
  // The P2PQuicSession owns these chromium specific objects required
  // by the QuicConnection. These outlive the QuicConnection itself.
  std::unique_ptr<net::QuicChromiumConnectionHelper> helper =
      std::make_unique<net::QuicChromiumConnectionHelper>(clock_, quic_random);

  P2PQuicPacketWriter* packet_writer =
      new P2PQuicPacketWriter(config.packet_transport);
  std::unique_ptr<quic::QuicConnection> quic_connection = CreateQuicConnection(
      config.is_server, helper.get(), packet_writer, alarm_factory_.get());
  // It's okay for the quic::QuicConnection to have a P2PQuicPacketWriter before
  // the P2PQuicPacketWriter is initialized, because the P2QuicPacketWriter
  // won't be writable until this occurs.
  packet_writer->InitializeWithQuicConnection(quic_connection.get());

  // QUIC configurations for the session are specified here.
  quic::QuicConfig quic_config;
  return std::make_unique<P2PQuicTransportImpl>(
      std::move(config), std::move(helper), std::move(quic_connection),
      quic_config, clock_);
}
}  // namespace blink
