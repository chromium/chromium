// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/quic/quic_test_packet_maker.h"

#include <list>
#include <utility>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "net/base/features.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quiche/common/quiche_buffer_allocator.h"
#include "net/third_party/quiche/src/quiche/common/simple_buffer_allocator.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/http_constants.h"
#include "net/third_party/quiche/src/quiche/quic/core/qpack/qpack_instruction_encoder.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_framer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_random.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"

namespace net::test {
namespace {

quic::QuicFrames CloneFrames(const quic::QuicFrames& frames) {
  quic::QuicFrames new_frames = frames;
  for (auto& frame : new_frames) {
    switch (frame.type) {
      // Frames smaller than a pointer are inlined, so don't need to be cloned.
      case quic::PADDING_FRAME:
      case quic::MTU_DISCOVERY_FRAME:
      case quic::PING_FRAME:
      case quic::MAX_STREAMS_FRAME:
      case quic::STOP_WAITING_FRAME:
      case quic::STREAMS_BLOCKED_FRAME:
      case quic::STREAM_FRAME:
      case quic::HANDSHAKE_DONE_FRAME:
      case quic::BLOCKED_FRAME:
      case quic::WINDOW_UPDATE_FRAME:
      case quic::STOP_SENDING_FRAME:
      case quic::PATH_CHALLENGE_FRAME:
      case quic::PATH_RESPONSE_FRAME:
        break;
      case quic::ACK_FRAME:
        frame.ack_frame = new quic::QuicAckFrame(*frame.ack_frame);
        break;
      case quic::RST_STREAM_FRAME:
        frame.rst_stream_frame =
            new quic::QuicRstStreamFrame(*frame.rst_stream_frame);
        break;
      case quic::CONNECTION_CLOSE_FRAME:
        frame.connection_close_frame =
            new quic::QuicConnectionCloseFrame(*frame.connection_close_frame);
        break;
      case quic::GOAWAY_FRAME:
        frame.goaway_frame = new quic::QuicGoAwayFrame(*frame.goaway_frame);
        break;
      case quic::NEW_CONNECTION_ID_FRAME:
        frame.new_connection_id_frame =
            new quic::QuicNewConnectionIdFrame(*frame.new_connection_id_frame);
        break;
      case quic::RETIRE_CONNECTION_ID_FRAME:
        frame.retire_connection_id_frame =
            new quic::QuicRetireConnectionIdFrame(
                *frame.retire_connection_id_frame);
        break;
      case quic::MESSAGE_FRAME:
        frame.message_frame = new quic::QuicMessageFrame(
            frame.message_frame->message_id,
            quiche::QuicheMemSlice(quiche::QuicheBuffer::Copy(
                quiche::SimpleBufferAllocator::Get(),
                frame.message_frame->message_data.data()->AsStringView())));
        break;
      case quic::CRYPTO_FRAME:
        frame.crypto_frame = new quic::QuicCryptoFrame(*frame.crypto_frame);
        break;
      case quic::NEW_TOKEN_FRAME:
        frame.new_token_frame =
            new quic::QuicNewTokenFrame(*frame.new_token_frame);
        break;
      case quic::ACK_FREQUENCY_FRAME:
        frame.ack_frequency_frame =
            new quic::QuicAckFrequencyFrame(*frame.ack_frequency_frame);
        break;
      case quic::RESET_STREAM_AT_FRAME:
        frame.reset_stream_at_frame =
            new quic::QuicResetStreamAtFrame(*frame.reset_stream_at_frame);
        break;

      case quic::NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot clone frame type: " << frame.type;
    }
  }
  return new_frames;
}

}  // namespace

QuicTestPacketMaker::ConnectionState::ConnectionState() = default;

QuicTestPacketMaker::ConnectionState::~ConnectionState() {
  for (auto& kv : saved_frames) {
    quic::DeleteFrames(&(kv.second));
  }
}

std::string_view QuicTestPacketMaker::ConnectionState::SaveStreamData(
    std::string_view data) {
  saved_stream_data.push_back(std::make_unique<std::string>(data));
  return *saved_stream_data.back();
}

void QuicTestPacketMaker::ConnectionState::Reset() {
  stream_offsets.clear();
}

QuicTestPacketMaker::QuicTestPacketMaker(quic::ParsedQuicVersion version,
                                         quic::QuicConnectionId connection_id,
                                         const quic::QuicClock* clock,
                                         const std::string& host,
                                         quic::Perspective perspective,
                                         bool client_priority_uses_incremental,
                                         bool use_priority_header)
    : version_(version),
      clock_(clock),
      host_(host),
      qpack_encoder_(&decoder_stream_error_delegate_,
                     quic::HuffmanEncoding::kEnabled,
                     quic::CookieCrumbling::kEnabled),
      perspective_(perspective),
      client_priority_uses_incremental_(client_priority_uses_incremental),
      use_priority_header_(use_priority_header) {
  DCHECK(version.HasIetfQuicFrames());
  DCHECK(!(perspective_ == quic::Perspective::IS_SERVER &&
           client_priority_uses_incremental_));

  set_connection_id(connection_id);
  qpack_encoder_.set_qpack_stream_sender_delegate(
      &encoder_stream_sender_delegate_);
}

QuicTestPacketMaker::~QuicTestPacketMaker() {
  CHECK(!builder_) << "QuicTestPacketMacker destroyed with unfinished packet "
                      "build operation";
}

void QuicTestPacketMaker::set_hostname(const std::string& host) {
  host_.assign(host);
}

QuicTestPacketBuilder& QuicTestPacketMaker::Packet(uint64_t packet_number) {
  CHECK(!builder_);
  builder_ = std::make_unique<QuicTestPacketBuilder>(packet_number, this,
                                                     &connection_state_);
  return *builder_.get();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeDummyCHLOPacket(uint64_t packet_number) {
  SetEncryptionLevel(quic::ENCRYPTION_INITIAL);

  auto& builder = Packet(packet_number);

  quic::CryptoHandshakeMessage message =
      MockCryptoClientStream::GetDummyCHLOMessage();
  const quic::QuicData& data = message.GetSerialized();

  builder.AddCryptoFrame(quic::ENCRYPTION_INITIAL, 0, data);

  builder.AddPaddingFrame();
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndRetransmissionPacket(
    uint64_t packet_number,
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    const std::vector<uint64_t>& original_packet_numbers) {
  DCHECK(connection_state_.save_packet_frames);
  auto& builder = Packet(packet_number);
  builder.AddAckFrame(first_received, largest_received, smallest_received);
  for (auto it : original_packet_numbers) {
    builder.AddPacketRetransmission(it);
  }
  return builder.Build();
}

quic::QuicFrames QuicTestPacketMaker::CloneSavedFrames(uint64_t packet_number) {
  DCHECK(connection_state_.save_packet_frames);
  return CloneFrames(
      connection_state_.saved_frames[quic::QuicPacketNumber(packet_number)]);
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeCombinedRetransmissionPacket(
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t new_packet_number) {
  DCHECK(connection_state_.save_packet_frames);
  auto& builder = Packet(new_packet_number);
  for (auto it : original_packet_numbers) {
    for (auto& frame : CloneFrames(
             connection_state_.saved_frames[quic::QuicPacketNumber(it)])) {
      if (frame.type != quic::PADDING_FRAME) {
        builder.AddFrameWithCoalescing(frame);
      }
    }
  }
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeAckAndDatagramPacket(uint64_t packet_number,
                                              uint64_t largest_received,
                                              uint64_t smallest_received,
                                              std::string_view data) {
  return Packet(packet_number)
      .AddAckFrame(/*first_received=*/1, largest_received, smallest_received)
      .AddMessageFrame(data)
      .Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndMultipleDataFramesPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    quiche::HttpHeaderBlock headers,
    size_t* spdy_headers_frame_length,
    const std::vector<std::string>& data_writes) {
  auto& builder = Packet(packet_number);
  builder.MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    builder.AddStreamFrame(2, false, priority_data);
  }

  AddPriorityHeader(spdy_priority, &headers);
  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  for (const auto& data_write : data_writes) {
    data += data_write;
  }
  builder.AddStreamFrame(stream_id, fin, data);
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    quiche::HttpHeaderBlock headers,
    size_t* spdy_headers_frame_length,
    bool should_include_priority_frame) {
  auto& builder = Packet(packet_number);
  builder.MaybeAddHttp3SettingsFrames();

  if (should_include_priority_frame) {
    std::string priority_data =
        GenerateHttp3PriorityData(spdy_priority, stream_id);
    if (!priority_data.empty()) {
      builder.AddStreamFrame(2, false, priority_data);
    }
  }

  AddPriorityHeader(spdy_priority, &headers);
  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  builder.AddStreamFrame(stream_id, fin, data);
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionAndRequestHeadersPacket(
    const std::vector<uint64_t>& original_packet_numbers,
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    quiche::HttpHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  DCHECK(connection_state_.save_packet_frames);
  auto& builder = Packet(packet_number);
  for (auto it : original_packet_numbers) {
    builder.AddPacketRetransmission(it);
  }

  builder.MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    builder.AddStreamFrame(2, false, priority_data);
  }

  AddPriorityHeader(spdy_priority, &headers);
  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  builder.AddStreamFrame(stream_id, fin, data);
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRequestHeadersAndRstPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin,
    spdy::SpdyPriority spdy_priority,
    quiche::HttpHeaderBlock headers,
    size_t* spdy_headers_frame_length,
    quic::QuicRstStreamErrorCode error_code) {
  auto& builder = Packet(packet_number);
  builder.MaybeAddHttp3SettingsFrames();

  std::string priority_data =
      GenerateHttp3PriorityData(spdy_priority, stream_id);
  if (!priority_data.empty()) {
    builder.AddStreamFrame(2, false, priority_data);
  }

  AddPriorityHeader(spdy_priority, &headers);
  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  builder.AddStreamFrame(stream_id, fin, data);
  builder.AddStopSendingFrame(stream_id, error_code);
  builder.AddRstStreamFrame(stream_id, error_code);
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeResponseHeadersPacket(
    uint64_t packet_number,
    quic::QuicStreamId stream_id,
    bool fin,
    quiche::HttpHeaderBlock headers,
    size_t* spdy_headers_frame_length) {
  std::string data = QpackEncodeHeaders(stream_id, std::move(headers),
                                        spdy_headers_frame_length);
  return Packet(packet_number).AddStreamFrame(stream_id, fin, data).Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeInitialSettingsPacket(uint64_t packet_number) {
  return Packet(packet_number).MaybeAddHttp3SettingsFrames().Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakePriorityPacket(uint64_t packet_number,
                                        quic::QuicStreamId id,
                                        spdy::SpdyPriority spdy_priority) {
  auto& builder = Packet(packet_number);
  std::string priority_data = GenerateHttp3PriorityData(spdy_priority, id);
  if (!priority_data.empty()) {
    builder.AddStreamFrame(2, false, priority_data);
  }
  return builder.Build();
}

std::unique_ptr<quic::QuicReceivedPacket>
QuicTestPacketMaker::MakeRetransmissionPacket(uint64_t original_packet_number,
                                              uint64_t new_packet_number) {
  DCHECK(connection_state_.save_packet_frames);
  auto& builder = Packet(new_packet_number);
  builder.AddPacketRetransmission(original_packet_number);
  return builder.Build();
}

std::unique_ptr<quic::QuicEncryptedPacket>
QuicTestPacketMaker::MakeStatelessResetPacket() {
  auto connection_id = quic::test::TestConnectionId();
  return quic::QuicFramer::BuildIetfStatelessResetPacket(
      connection_id, quic::QuicFramer::GetMinStatelessResetPacketLength() + 1,
      quic::QuicUtils::GenerateStatelessResetToken(connection_id));
}

void QuicTestPacketMaker::RemoveSavedStreamFrames(
    quic::QuicStreamId stream_id) {
  for (auto& kv : connection_state_.saved_frames) {
    auto* it = kv.second.begin();
    while (it != kv.second.end()) {
      if (it->type == quic::STREAM_FRAME &&
          it->stream_frame.stream_id == stream_id) {
        it = kv.second.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void QuicTestPacketMaker::SetEncryptionLevel(quic::EncryptionLevel level) {
  encryption_level_ = level;
  switch (level) {
    case quic::ENCRYPTION_INITIAL:
      long_header_type_ = quic::INITIAL;
      break;
    case quic::ENCRYPTION_ZERO_RTT:
      long_header_type_ = quic::ZERO_RTT_PROTECTED;
      break;
    case quic::ENCRYPTION_FORWARD_SECURE:
      long_header_type_ = quic::INVALID_PACKET_TYPE;
      break;
    default:
      LOG(DFATAL) << quic::EncryptionLevelToString(level);
      long_header_type_ = quic::INVALID_PACKET_TYPE;
  }
}

quiche::HttpHeaderBlock QuicTestPacketMaker::GetRequestHeaders(
    const std::string& method,
    const std::string& scheme,
    const std::string& path) const {
  quiche::HttpHeaderBlock headers;
  headers[":method"] = method;
  headers[":authority"] = host_;
  headers[":scheme"] = scheme;
  headers[":path"] = path;
  return headers;
}

quiche::HttpHeaderBlock QuicTestPacketMaker::ConnectRequestHeaders(
    const std::string& host_port) const {
  quiche::HttpHeaderBlock headers;
  headers[":method"] = "CONNECT";
  headers[":authority"] = host_port;
  return headers;
}

quiche::HttpHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status) const {
  quiche::HttpHeaderBlock headers;
  headers[":status"] = status;
  headers["content-type"] = "text/plain";
  return headers;
}

quiche::HttpHeaderBlock QuicTestPacketMaker::GetResponseHeaders(
    const std::string& status,
    const std::string& alt_svc) const {
  quiche::HttpHeaderBlock headers;
  headers[":status"] = status;
  headers["alt-svc"] = alt_svc;
  headers["content-type"] = "text/plain";
  return headers;
}

void QuicTestPacketMaker::Reset() {
  connection_state_.Reset();
}

std::string QuicTestPacketMaker::QpackEncodeHeaders(
    quic::QuicStreamId stream_id,
    quiche::HttpHeaderBlock headers,
    size_t* encoded_data_length) {
  std::string data;

  std::string encoded_headers =
      qpack_encoder_.EncodeHeaderList(stream_id, headers, nullptr);

  // Generate HEADERS frame header.
  const std::string headers_frame_header =
      quic::HttpEncoder::SerializeHeadersFrameHeader(encoded_headers.size());

  // Add the HEADERS frame header.
  data += headers_frame_header;
  // Add the HEADERS frame payload.
  data += encoded_headers;

  // Compute the total data length.
  if (encoded_data_length) {
    *encoded_data_length = data.length();
  }
  return data;
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketMaker::FinishPacket(
    quic::QuicPacketHeader header,
    quic::QuicFrames frames,
    std::unique_ptr<quic::QuicStreamFrameDataProducer> data_producer) {
  CHECK(builder_);
  builder_ = nullptr;

  quic::QuicFramer framer(quic::test::SupportedVersions(version_),
                          clock_->Now(), perspective_,
                          quic::kQuicDefaultConnectionIdLength);
  if (encryption_level_ == quic::ENCRYPTION_INITIAL) {
    framer.SetInitialObfuscators(perspective_ == quic::Perspective::IS_CLIENT
                                     ? header.destination_connection_id
                                     : header.source_connection_id);
  } else {
    framer.SetEncrypter(
        encryption_level_,
        std::make_unique<quic::test::TaggingEncrypter>(  // IN-TEST
            encryption_level_));
  }
  if (data_producer != nullptr) {
    framer.set_data_producer(data_producer.get());
  }
  size_t max_plaintext_size = framer.GetMaxPlaintextSize(max_plaintext_size_);
  size_t packet_size =
      quic::GetPacketHeaderSize(version_.transport_version, header);
  size_t frames_size = 0;
  for (size_t i = 0; i < frames.size(); ++i) {
    bool first_frame = i == 0;
    bool last_frame = i == frames.size() - 1;
    const size_t frame_size = framer.GetSerializedFrameLength(
        frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
        header.packet_number_length);
    packet_size += frame_size;
    frames_size += frame_size;
  }

  const size_t min_plaintext_packet_size =
      quic::QuicPacketCreator::MinPlaintextPacketSize(
          version_, header.packet_number_length);
  if (frames_size < min_plaintext_packet_size) {
    frames.insert(frames.begin(),
                  quic::QuicFrame(quic::QuicPaddingFrame(
                      min_plaintext_packet_size - frames_size)));
  }

  std::unique_ptr<quic::QuicPacket> packet(quic::test::BuildUnsizedDataPacket(
      &framer, header, frames, max_plaintext_size));
  char buffer[quic::kMaxOutgoingPacketSize];
  size_t encrypted_size =
      framer.EncryptPayload(encryption_level_, header.packet_number, *packet,
                            buffer, quic::kMaxOutgoingPacketSize);
  EXPECT_NE(0u, encrypted_size);
  quic::QuicReceivedPacket encrypted(buffer, encrypted_size, clock_->Now(),
                                     false, 0, true, nullptr, 0, false,
                                     ecn_codepoint_);
  if (connection_state_.save_packet_frames) {
    connection_state_.saved_frames[header.packet_number] = std::move(frames);
  } else {
    connection_state_.saved_stream_data.clear();
    DeleteFrames(&frames);
  }

  return encrypted.Clone();
}

quic::QuicStreamId QuicTestPacketMaker::GetFirstBidirectionalStreamId() const {
  return quic::QuicUtils::GetFirstBidirectionalStreamId(
      version_.transport_version, perspective_);
}

std::string QuicTestPacketMaker::GenerateHttp3PriorityData(
    spdy::SpdyPriority spdy_priority,
    quic::QuicStreamId stream_id) {
  std::string priority_data;
  quic::PriorityUpdateFrame priority_update;
  quic::HttpStreamPriority priority{
      spdy_priority, quic::HttpStreamPriority::kDefaultIncremental};
  if (client_priority_uses_incremental_) {
    priority.incremental = kDefaultPriorityIncremental;
  }

  if (priority.urgency != quic::HttpStreamPriority::kDefaultUrgency ||
      priority.incremental != quic::HttpStreamPriority::kDefaultIncremental) {
    priority_update.priority_field_value =
        quic::SerializePriorityFieldValue(priority);
  }

  // Only generate a frame if a non-empty string was generated.
  if (!priority_update.priority_field_value.empty()) {
    priority_update.prioritized_element_id = stream_id;
    priority_data =
        quic::HttpEncoder::SerializePriorityUpdateFrame(priority_update);
  }

  return priority_data;
}

void QuicTestPacketMaker::AddPriorityHeader(spdy::SpdyPriority spdy_priority,
                                            quiche::HttpHeaderBlock* headers) {
  if (use_priority_header_ &&
      base::FeatureList::IsEnabled(net::features::kPriorityHeader)) {
    quic::HttpStreamPriority priority{
        spdy_priority, quic::HttpStreamPriority::kDefaultIncremental};
    if (client_priority_uses_incremental_) {
      priority.incremental = kDefaultPriorityIncremental;
    }
    std::string serialized_priority =
        quic::SerializePriorityFieldValue(priority);
    if (!serialized_priority.empty()) {
      (*headers)[net::kHttp2PriorityHeader] = serialized_priority;
    }
  }
}

QuicTestPacketBuilder::QuicTestPacketBuilder(
    uint64_t packet_number,
    QuicTestPacketMaker* maker,
    QuicTestPacketMaker::ConnectionState* connection_state)
    : maker_(maker), connection_state_(connection_state) {
  CHECK(maker_);
  InitializeHeader(packet_number);
}

QuicTestPacketBuilder::~QuicTestPacketBuilder() {
  CHECK(!maker_) << "QuicTestPacketBuilder is missing a call to Build()";
  DeleteFrames(&frames_);
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddPaddingFrame(size_t length) {
  quic::QuicPaddingFrame padding_frame =
      (length > 0) ? quic::QuicPaddingFrame(length) : quic::QuicPaddingFrame();
  AddFrame(quic::QuicFrame(padding_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddPingFrame() {
  quic::QuicPingFrame ping_frame;
  AddFrame(quic::QuicFrame(ping_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddRetireConnectionIdFrame(
    uint64_t sequence_number) {
  auto* retire_cid_frame = new quic::QuicRetireConnectionIdFrame();
  retire_cid_frame->sequence_number = sequence_number;
  AddFrame(quic::QuicFrame(retire_cid_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddNewConnectionIdFrame(
    const quic::QuicConnectionId& cid,
    uint64_t sequence_number,
    uint64_t retire_prior_to) {
  auto* new_cid_frame = new quic::QuicNewConnectionIdFrame();
  new_cid_frame->connection_id = cid;
  new_cid_frame->sequence_number = sequence_number;
  new_cid_frame->retire_prior_to = retire_prior_to;
  new_cid_frame->stateless_reset_token =
      quic::QuicUtils::GenerateStatelessResetToken(cid);
  AddFrame(quic::QuicFrame(new_cid_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddMaxStreamsFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicMaxStreamsFrame max_streams_frame(control_frame_id, stream_count,
                                              unidirectional);
  AddFrame(quic::QuicFrame(max_streams_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddStreamsBlockedFrame(
    quic::QuicControlFrameId control_frame_id,
    quic::QuicStreamCount stream_count,
    bool unidirectional) {
  quic::QuicStreamsBlockedFrame streams_blocked_frame(
      control_frame_id, stream_count, unidirectional);
  AddFrame(quic::QuicFrame(streams_blocked_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddStreamFrame(
    quic::QuicStreamId stream_id,
    bool fin,
    std::string_view data) {
  quic::QuicStreamOffset offset = connection_state_->stream_offsets[stream_id];
  std::string_view saved_data = connection_state_->SaveStreamData(data);
  quic::QuicStreamFrame stream_frame(stream_id, fin, offset, saved_data);
  AddFrame(quic::QuicFrame(stream_frame));
  connection_state_->stream_offsets[stream_id] += data.length();
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddAckFrame(
    uint64_t first_received,
    uint64_t largest_received,
    uint64_t smallest_received,
    std::optional<quic::QuicEcnCounts> ecn) {
  auto* ack_frame = new quic::QuicAckFrame;
  ack_frame->largest_acked = quic::QuicPacketNumber(largest_received);
  ack_frame->ack_delay_time = quic::QuicTime::Delta::Zero();
  for (uint64_t i = smallest_received; i <= largest_received; ++i) {
    ack_frame->received_packet_times.emplace_back(quic::QuicPacketNumber(i),
                                                  maker_->clock()->Now());
  }
  if (largest_received > 0) {
    DCHECK_GE(largest_received, first_received);
    ack_frame->packets.AddRange(quic::QuicPacketNumber(first_received),
                                quic::QuicPacketNumber(largest_received + 1));
  }
  ack_frame->ecn_counters = ecn;
  AddFrame(quic::QuicFrame(ack_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddMessageFrame(
    std::string_view data) {
  auto* message_frame = new quic::QuicMessageFrame(
      /*message_id=*/0, quiche::QuicheMemSlice(quiche::QuicheBuffer::Copy(
                            quiche::SimpleBufferAllocator::Get(), data)));
  AddFrame(quic::QuicFrame(message_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddRstStreamFrame(
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  auto* rst_stream_frame = new quic::QuicRstStreamFrame(
      1, stream_id, error_code, connection_state_->stream_offsets[stream_id]);
  AddFrame(quic::QuicFrame(rst_stream_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddConnectionCloseFrame(
    quic::QuicErrorCode quic_error,
    const std::string& quic_error_details,
    uint64_t frame_type) {
  auto* close_frame = new quic::QuicConnectionCloseFrame(
      maker_->version().transport_version, quic_error, quic::NO_IETF_QUIC_ERROR,
      quic_error_details, frame_type);
  AddFrame(quic::QuicFrame(close_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddGoAwayFrame(
    quic::QuicErrorCode error_code,
    std::string reason_phrase) {
  auto* goaway_frame = new quic::QuicGoAwayFrame();
  goaway_frame->error_code = error_code;
  goaway_frame->last_good_stream_id = 0;
  goaway_frame->reason_phrase = reason_phrase;
  AddFrame(quic::QuicFrame(goaway_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddPathResponseFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto path_response_frame = quic::QuicPathResponseFrame(0, payload);
  AddFrame(quic::QuicFrame(path_response_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddPathChallengeFrame() {
  quic::test::MockRandom rand(0);
  quic::QuicPathFrameBuffer payload;
  rand.RandBytes(payload.data(), payload.size());
  auto path_challenge_frame = quic::QuicPathChallengeFrame(0, payload);
  AddFrame(quic::QuicFrame(path_challenge_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddStopSendingFrame(
    quic::QuicStreamId stream_id,
    quic::QuicRstStreamErrorCode error_code) {
  auto stop_sending_frame =
      quic::QuicStopSendingFrame(1, stream_id, error_code);
  AddFrame(quic::QuicFrame(stop_sending_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddCryptoFrame(
    quic::EncryptionLevel level,
    quic::QuicStreamOffset offset,
    const quic::QuicData& data) {
  if (!data_producer_) {
    data_producer_ =
        std::make_unique<quic::test::SimpleDataProducer>();  // IN-TEST
  }
  data_producer_->SaveCryptoData(level, offset, data.AsStringPiece());
  auto* crypto_frame = new quic::QuicCryptoFrame(level, offset, data.length());
  AddFrame(quic::QuicFrame(crypto_frame));
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddFrame(quic::QuicFrame frame) {
  CHECK(maker_);
  frames_.push_back(std::move(frame));
  DVLOG(1) << "Adding frame: " << frames_.back();
  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddFrameWithCoalescing(
    const quic::QuicFrame& frame) {
  if (frames_.empty()) {
    return AddFrame(frame);
  }
  if (frame.type != quic::STREAM_FRAME ||
      frames_.back().type != quic::STREAM_FRAME) {
    return AddFrame(frame);
  }

  // Make sure they are congruent data segments in the stream.
  const quic::QuicStreamFrame* new_frame = &frame.stream_frame;
  quic::QuicStreamFrame* previous_frame = &frames_.back().stream_frame;
  if (new_frame->stream_id != previous_frame->stream_id ||
      new_frame->offset !=
          previous_frame->offset + previous_frame->data_length) {
    return AddFrame(frame);
  }

  // Extend the data buffer to include the data from both frames (into a copy
  // buffer). This doesn't attempt to limit coalescing to a particular packet
  // size limit and may need to be updated if a test comes along that
  // retransmits enough stream data to span multiple packets.
  std::string data(previous_frame->data_buffer, previous_frame->data_length);
  data += std::string(new_frame->data_buffer, new_frame->data_length);
  connection_state_->saved_stream_data.push_back(
      std::make_unique<std::string>(data));
  std::string_view saved_data = *connection_state_->saved_stream_data.back();
  previous_frame->data_buffer = saved_data.data();
  previous_frame->data_length = saved_data.length();

  // Copy the fin state from the last frame.
  previous_frame->fin = new_frame->fin;

  return *this;
}

QuicTestPacketBuilder& QuicTestPacketBuilder::AddPacketRetransmission(
    uint64_t packet_number,
    base::RepeatingCallback<bool(const quic::QuicFrame&)> filter) {
  for (auto frame :
       connection_state_->saved_frames[quic::QuicPacketNumber(packet_number)]) {
    if (!filter || filter.Run(frame)) {
      AddFrameWithCoalescing(frame);
    }
  }
  return *this;
}

std::unique_ptr<quic::QuicReceivedPacket> QuicTestPacketBuilder::Build() {
  CHECK(maker_);
  QuicTestPacketMaker* maker = maker_;
  maker_ = nullptr;
  return maker->FinishPacket(std::move(header_), std::move(frames_),
                             std::move(data_producer_));
}

QuicTestPacketBuilder& QuicTestPacketBuilder::MaybeAddHttp3SettingsFrames() {
  quic::QuicStreamId stream_id =
      quic::QuicUtils::GetFirstUnidirectionalStreamId(
          maker_->version().transport_version, maker_->perspective());

  // If the control stream has already been written to, do nothing.
  if (connection_state_->stream_offsets[stream_id] != 0) {
    return *this;
  }

  // A stream frame containing stream type will be written on the control
  // stream first.
  std::string type(1, 0x00);
  std::string settings_data = GenerateHttp3SettingsData();
  std::string grease_data = GenerateHttp3GreaseData();

  // The type and the SETTINGS frame may be sent in multiple QUIC STREAM
  // frames.
  std::string data = type + settings_data + grease_data;

  AddStreamFrame(stream_id, false, data);
  return *this;
}

std::string QuicTestPacketBuilder::GenerateHttp3SettingsData() const {
  quic::SettingsFrame settings;
  settings.values[quic::SETTINGS_MAX_FIELD_SECTION_SIZE] =
      kQuicMaxHeaderListSize;
  settings.values[quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY] =
      quic::kDefaultQpackMaxDynamicTableCapacity;
  settings.values[quic::SETTINGS_QPACK_BLOCKED_STREAMS] =
      quic::kDefaultMaximumBlockedStreams;
  settings.values[quic::SETTINGS_H3_DATAGRAM] = 1;
  // Greased setting.
  settings.values[0x40] = 20;
  return quic::HttpEncoder::SerializeSettingsFrame(settings);
}

std::string QuicTestPacketBuilder::GenerateHttp3GreaseData() const {
  return quic::HttpEncoder::SerializeGreasingFrame();
}

void QuicTestPacketBuilder::InitializeHeader(uint64_t packet_number) {
  header_.destination_connection_id = DestinationConnectionId();
  header_.destination_connection_id_included = quic::CONNECTION_ID_PRESENT;
  header_.source_connection_id = SourceConnectionId();
  header_.source_connection_id_included = quic::CONNECTION_ID_PRESENT;
  header_.reset_flag = false;
  header_.version_flag = ShouldIncludeVersion();
  header_.form = header_.version_flag ? quic::IETF_QUIC_LONG_HEADER_PACKET
                                      : quic::IETF_QUIC_SHORT_HEADER_PACKET;
  header_.long_packet_type = maker_->long_header_type();
  header_.packet_number_length = quic::PACKET_1BYTE_PACKET_NUMBER;
  header_.packet_number = quic::QuicPacketNumber(packet_number);
  if (header_.version_flag) {
    if (maker_->long_header_type() == quic::INITIAL) {
      header_.retry_token_length_length =
          quiche::VARIABLE_LENGTH_INTEGER_LENGTH_1;
    }
    header_.length_length = quiche::VARIABLE_LENGTH_INTEGER_LENGTH_2;
  }
}

quic::QuicConnectionId QuicTestPacketBuilder::DestinationConnectionId() const {
  if (maker_->perspective() == quic::Perspective::IS_SERVER) {
    return quic::EmptyQuicConnectionId();
  }
  return maker_->connection_id();
}

quic::QuicConnectionId QuicTestPacketBuilder::SourceConnectionId() const {
  if (maker_->perspective() == quic::Perspective::IS_CLIENT) {
    return quic::EmptyQuicConnectionId();
  }
  return maker_->connection_id();
}

bool QuicTestPacketBuilder::ShouldIncludeVersion() const {
  return maker_->encryption_level() < quic::ENCRYPTION_FORWARD_SECURE;
}

}  // namespace net::test
