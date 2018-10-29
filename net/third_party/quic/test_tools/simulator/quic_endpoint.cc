// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simulator/quic_endpoint.h"

#include "base/sha1.h"
#include "net/third_party/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_test_output.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"

namespace quic {
namespace simulator {

const QuicStreamId kDataStream = 3;
const QuicByteCount kWriteChunkSize = 128 * 1024;
const char kStreamDataContents = 'Q';

// Takes a SHA-1 hash of the name and converts it into five 32-bit integers.
static std::vector<uint32_t> HashNameIntoFive32BitIntegers(QuicString name) {
  const QuicString hash = test::Sha1Hash(name);

  std::vector<uint32_t> output;
  uint32_t current_number = 0;
  for (size_t i = 0; i < hash.size(); i++) {
    current_number = (current_number << 8) + hash[i];
    if (i % 4 == 3) {
      output.push_back(i);
      current_number = 0;
    }
  }

  return output;
}

QuicSocketAddress GetAddressFromName(QuicString name) {
  const std::vector<uint32_t> hash = HashNameIntoFive32BitIntegers(name);

  // Generate a random port between 1025 and 65535.
  const uint16_t port = 1025 + hash[0] % (65535 - 1025 + 1);

  // Generate a random 10.x.x.x address, where x is between 1 and 254.
  QuicString ip_address{"\xa\0\0\0", 4};
  for (size_t i = 1; i < 4; i++) {
    ip_address[i] = 1 + hash[i] % 254;
  }
  QuicIpAddress host;
  host.FromPackedString(ip_address.c_str(), ip_address.length());
  return QuicSocketAddress(host, port);
}

QuicEndpoint::QuicEndpoint(Simulator* simulator,
                           QuicString name,
                           QuicString peer_name,
                           Perspective perspective,
                           QuicConnectionId connection_id)
    : Endpoint(simulator, name),
      peer_name_(peer_name),
      writer_(this),
      nic_tx_queue_(simulator,
                    QuicStringPrintf("%s (TX Queue)", name.c_str()),
                    kMaxPacketSize * kTxQueueSize),
      connection_(connection_id,
                  GetAddressFromName(peer_name),
                  simulator,
                  simulator->GetAlarmFactory(),
                  &writer_,
                  false,
                  perspective,
                  ParsedVersionOfIndex(CurrentSupportedVersions(), 0)),
      bytes_to_transfer_(0),
      bytes_transferred_(0),
      write_blocked_count_(0),
      wrong_data_received_(false),
      drop_next_packet_(false),
      notifier_(nullptr) {
  nic_tx_queue_.set_listener_interface(this);

  connection_.SetSelfAddress(GetAddressFromName(name));
  connection_.set_visitor(this);
  connection_.SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                           QuicMakeUnique<NullEncrypter>(perspective));
  connection_.SetDecrypter(ENCRYPTION_FORWARD_SECURE,
                           QuicMakeUnique<NullDecrypter>(perspective));
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  if (perspective == Perspective::IS_SERVER) {
    // Skip version negotiation.
    test::QuicConnectionPeer::SetNegotiatedVersion(&connection_);
  }
  connection_.SetDataProducer(&producer_);
  connection_.SetSessionNotifier(this);
  if (connection_.session_decides_what_to_write()) {
    notifier_ = QuicMakeUnique<test::SimpleSessionNotifier>(&connection_);
  }

  // Configure the connection as if it received a handshake.  This is important
  // primarily because
  //  - this enables pacing, and
  //  - this sets the non-handshake timeouts.
  QuicString error;
  CryptoHandshakeMessage peer_hello;
  peer_hello.SetValue(kICSL,
                      static_cast<uint32_t>(kMaximumIdleTimeoutSecs - 1));
  peer_hello.SetValue(kMIDS,
                      static_cast<uint32_t>(kDefaultMaxStreamsPerConnection));
  QuicConfig config;
  QuicErrorCode error_code = config.ProcessPeerHello(
      peer_hello, perspective == Perspective::IS_CLIENT ? SERVER : CLIENT,
      &error);
  DCHECK_EQ(error_code, QUIC_NO_ERROR) << "Configuration failed: " << error;
  connection_.SetFromConfig(config);
}

QuicEndpoint::~QuicEndpoint() {
  if (trace_visitor_ != nullptr) {
    const char* perspective_prefix =
        connection_.perspective() == Perspective::IS_CLIENT ? "C" : "S";

    QuicString identifier =
        QuicStrCat(perspective_prefix, connection_.connection_id());
    QuicRecordTestOutput(identifier,
                         trace_visitor_->trace()->SerializeAsString());
  }
}

QuicByteCount QuicEndpoint::bytes_received() const {
  QuicByteCount total = 0;
  for (auto& interval : offsets_received_) {
    total += interval.max() - interval.min();
  }
  return total;
}

QuicByteCount QuicEndpoint::bytes_to_transfer() const {
  if (notifier_ != nullptr) {
    return notifier_->StreamBytesToSend();
  }
  return bytes_to_transfer_;
}

QuicByteCount QuicEndpoint::bytes_transferred() const {
  if (notifier_ != nullptr) {
    return notifier_->StreamBytesSent();
  }
  return bytes_transferred_;
}

void QuicEndpoint::AddBytesToTransfer(QuicByteCount bytes) {
  if (notifier_ != nullptr) {
    if (notifier_->HasBufferedStreamData()) {
      Schedule(clock_->Now());
    }
    notifier_->WriteOrBufferData(kDataStream, bytes, NO_FIN);
    return;
  }

  if (bytes_to_transfer_ > 0) {
    Schedule(clock_->Now());
  }

  bytes_to_transfer_ += bytes;
  WriteStreamData();
}

void QuicEndpoint::DropNextIncomingPacket() {
  drop_next_packet_ = true;
}

void QuicEndpoint::RecordTrace() {
  trace_visitor_ = QuicMakeUnique<QuicTraceVisitor>(&connection_);
  connection_.set_debug_visitor(trace_visitor_.get());
}

void QuicEndpoint::AcceptPacket(std::unique_ptr<Packet> packet) {
  if (packet->destination != name_) {
    return;
  }
  if (drop_next_packet_) {
    drop_next_packet_ = false;
    return;
  }

  QuicReceivedPacket received_packet(packet->contents.data(),
                                     packet->contents.size(), clock_->Now());
  connection_.ProcessUdpPacket(connection_.self_address(),
                               connection_.peer_address(), received_packet);
}

UnconstrainedPortInterface* QuicEndpoint::GetRxPort() {
  return this;
}

void QuicEndpoint::SetTxPort(ConstrainedPortInterface* port) {
  // Any egress done by the endpoint is actually handled by a queue on an NIC.
  nic_tx_queue_.set_tx_port(port);
}

void QuicEndpoint::OnPacketDequeued() {
  if (writer_.IsWriteBlocked() &&
      (nic_tx_queue_.capacity() - nic_tx_queue_.bytes_queued()) >=
          kMaxPacketSize) {
    writer_.SetWritable();
    connection_.OnCanWrite();
  }
}

void QuicEndpoint::OnStreamFrame(const QuicStreamFrame& frame) {
  // Verify that the data received always matches the expected.
  DCHECK(frame.stream_id == kDataStream);
  for (size_t i = 0; i < frame.data_length; i++) {
    if (frame.data_buffer[i] != kStreamDataContents) {
      wrong_data_received_ = true;
    }
  }
  offsets_received_.Add(frame.offset, frame.offset + frame.data_length);
  // Sanity check against very pathological connections.
  DCHECK_LE(offsets_received_.Size(), 1000u);
}
void QuicEndpoint::OnCanWrite() {
  if (notifier_ != nullptr) {
    notifier_->OnCanWrite();
    return;
  }
  WriteStreamData();
}
bool QuicEndpoint::WillingAndAbleToWrite() const {
  if (notifier_ != nullptr) {
    return notifier_->WillingToWrite();
  }
  return bytes_to_transfer_ != 0;
}
bool QuicEndpoint::HasPendingHandshake() const {
  return false;
}
bool QuicEndpoint::HasOpenDynamicStreams() const {
  return true;
}

bool QuicEndpoint::AllowSelfAddressChange() const {
  return false;
}

bool QuicEndpoint::OnFrameAcked(const QuicFrame& frame,
                                QuicTime::Delta ack_delay_time) {
  if (notifier_ != nullptr) {
    return notifier_->OnFrameAcked(frame, ack_delay_time);
  }
  return false;
}

void QuicEndpoint::OnFrameLost(const QuicFrame& frame) {
  DCHECK(notifier_);
  notifier_->OnFrameLost(frame);
}

void QuicEndpoint::RetransmitFrames(const QuicFrames& frames,
                                    TransmissionType type) {
  DCHECK(notifier_);
  notifier_->RetransmitFrames(frames, type);
}

bool QuicEndpoint::IsFrameOutstanding(const QuicFrame& frame) const {
  DCHECK(notifier_);
  return notifier_->IsFrameOutstanding(frame);
}

bool QuicEndpoint::HasUnackedCryptoData() const {
  return false;
}

QuicEndpoint::Writer::Writer(QuicEndpoint* endpoint)
    : endpoint_(endpoint), is_blocked_(false) {}

QuicEndpoint::Writer::~Writer() {}

WriteResult QuicEndpoint::Writer::WritePacket(
    const char* buffer,
    size_t buf_len,
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address,
    PerPacketOptions* options) {
  DCHECK(!IsWriteBlocked());
  DCHECK(options == nullptr);
  DCHECK(buf_len <= kMaxPacketSize);

  // Instead of losing a packet, become write-blocked when the egress queue is
  // full.
  if (endpoint_->nic_tx_queue_.packets_queued() > kTxQueueSize) {
    is_blocked_ = true;
    endpoint_->write_blocked_count_++;
    return WriteResult(WRITE_STATUS_BLOCKED, 0);
  }

  auto packet = QuicMakeUnique<Packet>();
  packet->source = endpoint_->name();
  packet->destination = endpoint_->peer_name_;
  packet->tx_timestamp = endpoint_->clock_->Now();

  packet->contents = QuicString(buffer, buf_len);
  packet->size = buf_len;

  endpoint_->nic_tx_queue_.AcceptPacket(std::move(packet));

  return WriteResult(WRITE_STATUS_OK, buf_len);
}

bool QuicEndpoint::Writer::IsWriteBlockedDataBuffered() const {
  return false;
}

bool QuicEndpoint::Writer::IsWriteBlocked() const {
  return is_blocked_;
}

void QuicEndpoint::Writer::SetWritable() {
  is_blocked_ = false;
}

QuicByteCount QuicEndpoint::Writer::GetMaxPacketSize(
    const QuicSocketAddress& /*peer_address*/) const {
  return kMaxPacketSize;
}

bool QuicEndpoint::Writer::SupportsReleaseTime() const {
  return false;
}

bool QuicEndpoint::Writer::IsBatchMode() const {
  return false;
}

char* QuicEndpoint::Writer::GetNextWriteLocation(
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address) {
  return nullptr;
}

WriteResult QuicEndpoint::Writer::Flush() {
  return WriteResult(WRITE_STATUS_OK, 0);
}

WriteStreamDataResult QuicEndpoint::DataProducer::WriteStreamData(
    QuicStreamId id,
    QuicStreamOffset offset,
    QuicByteCount data_length,
    QuicDataWriter* writer) {
  writer->WriteRepeatedByte(kStreamDataContents, data_length);
  return WRITE_SUCCESS;
}

void QuicEndpoint::WriteStreamData() {
  // Instantiate a flusher which would normally be here due to QuicSession.
  QuicConnection::ScopedPacketFlusher flusher(
      &connection_, QuicConnection::SEND_ACK_IF_QUEUED);

  while (bytes_to_transfer_ > 0) {
    // Transfer data in chunks of size at most |kWriteChunkSize|.
    const size_t transmission_size =
        std::min(kWriteChunkSize, bytes_to_transfer_);

    QuicConsumedData consumed_data = connection_.SendStreamData(
        kDataStream, transmission_size, bytes_transferred_, NO_FIN);

    DCHECK(consumed_data.bytes_consumed <= transmission_size);
    bytes_transferred_ += consumed_data.bytes_consumed;
    bytes_to_transfer_ -= consumed_data.bytes_consumed;
    if (consumed_data.bytes_consumed != transmission_size) {
      return;
    }
  }
}

QuicEndpointMultiplexer::QuicEndpointMultiplexer(
    QuicString name,
    std::initializer_list<QuicEndpoint*> endpoints)
    : Endpoint((*endpoints.begin())->simulator(), name) {
  for (QuicEndpoint* endpoint : endpoints) {
    mapping_.insert(std::make_pair(endpoint->name(), endpoint));
  }
}

QuicEndpointMultiplexer::~QuicEndpointMultiplexer() {}

void QuicEndpointMultiplexer::AcceptPacket(std::unique_ptr<Packet> packet) {
  auto key_value_pair_it = mapping_.find(packet->destination);
  if (key_value_pair_it == mapping_.end()) {
    return;
  }

  key_value_pair_it->second->GetRxPort()->AcceptPacket(std::move(packet));
}
UnconstrainedPortInterface* QuicEndpointMultiplexer::GetRxPort() {
  return this;
}
void QuicEndpointMultiplexer::SetTxPort(ConstrainedPortInterface* port) {
  for (auto& key_value_pair : mapping_) {
    key_value_pair.second->SetTxPort(port);
  }
}

}  // namespace simulator
}  // namespace quic
