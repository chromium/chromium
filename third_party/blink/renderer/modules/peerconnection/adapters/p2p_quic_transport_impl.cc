// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"

#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/core/tls_client_handshaker.h"
#include "net/third_party/quiche/src/quic/core/tls_server_handshaker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice_storage.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"

namespace blink {

namespace {

static const char kClosingDetails[] = "Application closed connection.";

// QUIC's default is 100. Setting this value to 10000 allows room for QUIC to
// not refuse new incoming streams in the case that an application wants to send
// a small chunk of data per stream (and immediately close) unreliably.
uint32_t kMaxIncomingDynamicStreams = 10000;

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
    packet.packet_number =
        connection_->packet_creator().packet_number().ToUint64();
    packet.buffer = buffer;
    packet.buf_len = buf_len;
    int bytes_written = packet_transport_->WritePacket(packet);
    if (bytes_written <= 0) {
      writable_ = false;
      return quic::WriteResult(quic::WRITE_STATUS_BLOCKED, EWOULDBLOCK);
    }
    return quic::WriteResult(quic::WRITE_STATUS_OK, bytes_written);
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

  quic::QuicPacketBuffer GetNextWriteLocation(
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return {nullptr, nullptr};
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
    quic::Perspective perspective,
    quic::QuicConnectionHelperInterface* helper,
    quic::QuicPacketWriter* packet_writer,
    quic::QuicAlarmFactory* alarm_factory) {
  quic::QuicIpAddress ip;
  ip.FromString("0.0.0.0");
  quic::QuicSocketAddress dummy_address(ip, 0 /* Port */);
  quic::QuicConnectionId dummy_connection_id;
  char connection_id_bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  dummy_connection_id =
      quic::QuicConnectionId(connection_id_bytes, sizeof(connection_id_bytes));
  return std::make_unique<quic::QuicConnection>(
      dummy_connection_id, quic::QuicSocketAddress(), dummy_address, helper,
      alarm_factory, packet_writer, /* owns_writer */ true, perspective,
      quic::ParsedQuicVersionVector{quic::CurrentSupportedVersions()[0]});
}

// A dummy helper for a server crypto stream that accepts all client hellos
// and generates a random connection ID.
class DummyCryptoServerStreamHelper
    : public quic::QuicCryptoServerStreamBase::Helper {
 public:
  explicit DummyCryptoServerStreamHelper(quic::QuicRandom* random) {}

  ~DummyCryptoServerStreamHelper() override {}

  bool CanAcceptClientHello(const quic::CryptoHandshakeMessage& message,
                            const quic::QuicSocketAddress& client_address,
                            const quic::QuicSocketAddress& peer_address,
                            const quic::QuicSocketAddress& self_address,
                            std::string* error_details) const override {
    return true;
  }
};
}  // namespace

std::unique_ptr<P2PQuicTransportImpl> P2PQuicTransportImpl::Create(
    quic::QuicClock* clock,
    quic::QuicAlarmFactory* alarm_factory,
    quic::QuicRandom* quic_random,
    P2PQuicTransport::Delegate* delegate,
    P2PQuicPacketTransport* packet_transport,
    const P2PQuicTransportConfig& config,
    std::unique_ptr<P2PQuicCryptoConfigFactory> crypto_config_factory,
    std::unique_ptr<P2PQuicCryptoStreamFactory> crypto_stream_factory) {
  DCHECK(delegate);
  DCHECK(packet_transport);
  DCHECK(crypto_config_factory);
  DCHECK(crypto_stream_factory);

  // The P2PQuicSession owns these chromium specific objects required
  // by the QuicConnection. These outlive the QuicConnection itself.
  std::unique_ptr<net::QuicChromiumConnectionHelper> helper =
      std::make_unique<net::QuicChromiumConnectionHelper>(clock, quic_random);

  P2PQuicPacketWriter* packet_writer =
      new P2PQuicPacketWriter(packet_transport);
  std::unique_ptr<quic::QuicConnection> quic_connection = CreateQuicConnection(
      config.perspective, helper.get(), packet_writer, alarm_factory);
  // It's okay for the quic::QuicConnection to have a P2PQuicPacketWriter before
  // the P2PQuicPacketWriter is initialized, because the P2QuicPacketWriter
  // won't be writable until this occurs.
  packet_writer->InitializeWithQuicConnection(quic_connection.get());

  // QUIC configurations for the session are specified here.
  // TODO(shampson): Consider setting larger initial flow control window sizes
  // so that the default limit doesn't cause initial undersending.
  quic::QuicConfig quic_config;
  quic_config.SetMaxBidirectionalStreamsToSend(kMaxIncomingDynamicStreams);
  quic_config.SetMaxUnidirectionalStreamsToSend(kMaxIncomingDynamicStreams);
  // The handshake network timeouts are configured to large values to prevent
  // the QUIC connection from being closed on a slow connection. This can occur
  // if signaling is slow and one side begins the handshake early.
  // See ICE related bug: bugs.webrtc.org/9869.
  //
  // This timeout is from time of creation of the quic::QuicConnection object to
  // the completion of the handshake. It must be larger than the idle time.
  quic_config.set_max_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromSeconds(50));
  // This is the timeout for idle time in the handshake. This value allows
  // time for slow signaling to complete.
  quic_config.set_max_idle_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromSeconds(30));
  return std::make_unique<P2PQuicTransportImpl>(
      delegate, packet_transport, std::move(config), std::move(helper),
      std::move(quic_connection), quic_config, std::move(crypto_config_factory),
      std::move(crypto_stream_factory), clock);
}

P2PQuicTransportImpl::P2PQuicTransportImpl(
    Delegate* delegate,
    P2PQuicPacketTransport* packet_transport,
    const P2PQuicTransportConfig& p2p_transport_config,
    std::unique_ptr<quic::QuicConnectionHelperInterface> helper,
    std::unique_ptr<quic::QuicConnection> connection,
    const quic::QuicConfig& quic_config,
    std::unique_ptr<P2PQuicCryptoConfigFactory> crypto_config_factory,
    std::unique_ptr<P2PQuicCryptoStreamFactory> crypto_stream_factory,
    quic::QuicClock* clock)
    : quic::QuicSession(connection.get(),
                        nullptr /* visitor */,
                        quic_config,
                        quic::CurrentSupportedVersions(),
                        /*expected_num_static_unidirectional_streams = */ 0),
      helper_(std::move(helper)),
      connection_(std::move(connection)),
      crypto_config_factory_(std::move(crypto_config_factory)),
      crypto_stream_factory_(std::move(crypto_stream_factory)),
      perspective_(p2p_transport_config.perspective),
      packet_transport_(packet_transport),
      delegate_(delegate),
      clock_(clock),
      stream_delegate_read_buffer_size_(
          p2p_transport_config.stream_delegate_read_buffer_size),
      stream_write_buffer_size_(p2p_transport_config.stream_write_buffer_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  DCHECK(crypto_config_factory_);
  DCHECK(crypto_stream_factory_);
  DCHECK(clock_);
  DCHECK(packet_transport_);
  DCHECK_GT(stream_delegate_read_buffer_size_, 0u);
  DCHECK_GT(stream_write_buffer_size_, 0u);
  if (!p2p_transport_config.certificates.IsEmpty()) {
    // TODO(https://crbug.com/874296): The web API accepts multiple
    // certificates, and we might want to pass these down to let QUIC decide on
    // what to use.
    certificate_ = p2p_transport_config.certificates[0];
  }
  switch (perspective_) {
    case quic::Perspective::IS_CLIENT: {
      crypto_client_config_ =
          crypto_config_factory_->CreateClientCryptoConfig();
      break;
    }
    case quic::Perspective::IS_SERVER: {
      crypto_server_config_ =
          crypto_config_factory_->CreateServerCryptoConfig();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

P2PQuicTransportImpl::~P2PQuicTransportImpl() {
  packet_transport_->SetReceiveDelegate(nullptr);
}

void P2PQuicTransportImpl::Stop() {
  // This shouldn't be called before Start().
  DCHECK(crypto_stream_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (IsClosed()) {
    return;
  }
  // The error code used for the connection closing is
  // quic::QUIC_CONNECTION_CANCELLED. This allows us to distinguish that the
  // application closed the connection, as opposed to it closing from a
  // failure/error.
  connection_->CloseConnection(
      quic::QuicErrorCode::QUIC_CONNECTION_CANCELLED, kClosingDetails,
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void P2PQuicTransportImpl::Start(StartConfig config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Either the remote fingerprints are being verified or a pre shared key is
  // set.
  DCHECK((certificate_ && !config.remote_fingerprints.IsEmpty()) ||
         !config.pre_shared_key.empty());
  DCHECK(!crypto_stream_);

  remote_fingerprints_ = std::move(config.remote_fingerprints);
  switch (perspective_) {
    case quic::Perspective::IS_CLIENT: {
      crypto_client_config_->set_pre_shared_key(config.pre_shared_key);
      break;
    }
    case quic::Perspective::IS_SERVER: {
      crypto_server_config_->set_pre_shared_key(config.pre_shared_key);
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  InitializeCryptoStream();

  if (perspective_ == quic::Perspective::IS_CLIENT) {
    quic::QuicCryptoClientStream* client_crypto_stream =
        static_cast<quic::QuicCryptoClientStream*>(crypto_stream_.get());
    client_crypto_stream->CryptoConnect();
  }
  // Now that crypto streams are setup we are ready to receive QUIC packets.
  packet_transport_->SetReceiveDelegate(this);
}

void P2PQuicTransportImpl::OnPacketDataReceived(const char* data,
                                                size_t data_len) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Received data from the |packet_transport_|. Create a QUIC packet and send
  // it to be processed by the QuicSession/Connection.
  quic::QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(connection()->self_address(), connection()->peer_address(),
                   packet);
}

quic::QuicCryptoStream* P2PQuicTransportImpl::GetMutableCryptoStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return crypto_stream_.get();
}

const quic::QuicCryptoStream* P2PQuicTransportImpl::GetCryptoStream() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return crypto_stream_.get();
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return CreateOutgoingBidirectionalStream();
}

P2PQuicTransportStats P2PQuicTransportImpl::GetStats() const {
  P2PQuicTransportStats stats(connection_->GetStats());
  stats.num_incoming_streams_created = num_incoming_streams_created_;
  stats.num_outgoing_streams_created = num_outgoing_streams_created_;
  stats.num_datagrams_lost = num_datagrams_lost_;
  return stats;
}

void P2PQuicTransportImpl::SendDatagram(Vector<uint8_t> datagram) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(CanSendDatagram());
  DCHECK_LT(datagram.size(),
            quic::QuicSession::GetCurrentLargestMessagePayload());

  if (!datagram_buffer_.empty()) {
    // We are currently write blocked, just add to the buffer.
    datagram_buffer_.push(std::move(datagram));
    return;
  }
  if (!TrySendDatagram(datagram)) {
    datagram_buffer_.push(std::move(datagram));
  }
}

bool P2PQuicTransportImpl::CanSendDatagram() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return IsEncryptionEstablished() &&
         (connection()->version().SupportsMessageFrames()) && !IsClosed();
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateOutgoingBidirectionalStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  num_outgoing_streams_created_++;
  P2PQuicStreamImpl* stream =
      CreateStreamInternal(GetNextOutgoingBidirectionalStreamId());
  ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream));
  return stream;
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateIncomingStream(
    quic::QuicStreamId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  num_incoming_streams_created_++;
  P2PQuicStreamImpl* stream = CreateStreamInternal(id);
  ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream));
  delegate_->OnStream(stream);
  return stream;
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateIncomingStream(
    quic::PendingStream* pending) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  num_incoming_streams_created_++;
  P2PQuicStreamImpl* stream = CreateStreamInternal(pending);
  ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream));
  delegate_->OnStream(stream);
  return stream;
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateStreamInternal(
    quic::QuicStreamId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(crypto_stream_);
  DCHECK(IsEncryptionEstablished());
  DCHECK(!IsClosed());
  return new P2PQuicStreamImpl(id, this, stream_delegate_read_buffer_size_,
                               stream_write_buffer_size_);
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateStreamInternal(
    quic::PendingStream* pending) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(crypto_stream_);
  DCHECK(IsEncryptionEstablished());
  DCHECK(!IsClosed());
  return new P2PQuicStreamImpl(pending, this, stream_delegate_read_buffer_size_,
                               stream_write_buffer_size_);
}

bool P2PQuicTransportImpl::TrySendDatagram(Vector<uint8_t>& datagram) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(CanSendDatagram());
  DCHECK_LT(datagram.size(),
            quic::QuicSession::GetCurrentLargestMessagePayload());

  struct iovec iov = {datagram.data(), datagram.size()};
  quic::QuicMemSliceStorage storage(
      &iov, 1, connection()->helper()->GetStreamSendBufferAllocator(),
      datagram.size());
  quic::MessageResult result = QuicSession::SendMessage(storage.ToSpan());
  switch (result.status) {
    case quic::MESSAGE_STATUS_BLOCKED:
      return false;
    case quic::MESSAGE_STATUS_SUCCESS:
      delegate_->OnDatagramSent();
      return true;
    case quic::MESSAGE_STATUS_ENCRYPTION_NOT_ESTABLISHED:
    case quic::MESSAGE_STATUS_INTERNAL_ERROR:
    case quic::MESSAGE_STATUS_TOO_LARGE:
    case quic::MESSAGE_STATUS_UNSUPPORTED:
      break;
  }
  // Anything besides blocked/success should never happen.
  LOG(ERROR) << "Unexpected result with QuicSession::SendMessage: "
             << result.status;
  NOTREACHED();
  return false;
}

void P2PQuicTransportImpl::InitializeCryptoStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!crypto_stream_);
  // TODO(shampson): If the P2PQuicTransportImpl is subclassed into a client
  // and server class we can call this as a virtual function and not need this
  // switch statement.
  switch (perspective_) {
    case quic::Perspective::IS_CLIENT: {
      DCHECK(crypto_client_config_);
      crypto_stream_ = crypto_stream_factory_->CreateClientCryptoStream(
          /*QuicSession=*/this, crypto_client_config_.get(),
          /*ProofHandler=*/this);
      QuicSession::Initialize();
      break;
    }
    case quic::Perspective::IS_SERVER: {
      DCHECK(crypto_server_config_);
      // Provide server with serialized config string to prove ownership.
      quic::QuicCryptoServerConfig::ConfigOptions options;
      // The |message| is used to handle the return value of AddDefaultConfig
      // which is raw pointer of the CryptoHandshakeMessage.
      std::unique_ptr<quic::CryptoHandshakeMessage> message(
          crypto_server_config_->AddDefaultConfig(
              helper_->GetRandomGenerator(), helper_->GetClock(), options));
      compressed_certs_cache_.reset(new quic::QuicCompressedCertsCache(
          quic::QuicCompressedCertsCache::kQuicCompressedCertsCacheSize));
      server_stream_helper_ = std::make_unique<DummyCryptoServerStreamHelper>(
          helper_->GetRandomGenerator());

      crypto_stream_ = crypto_stream_factory_->CreateServerCryptoStream(
          crypto_server_config_.get(), compressed_certs_cache_.get(), this,
          server_stream_helper_.get());
      QuicSession::Initialize();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void P2PQuicTransportImpl::SetDefaultEncryptionLevel(
    quic::EncryptionLevel level) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QuicSession::SetDefaultEncryptionLevel(level);
  if (level == quic::ENCRYPTION_FORWARD_SECURE) {
    DCHECK(IsEncryptionEstablished());
    DCHECK(OneRttKeysAvailable());
    P2PQuicNegotiatedParams negotiated_params;
    // The guaranteed largest message payload will not change throughout the
    // connection.
    uint16_t max_datagram_length =
        quic::QuicSession::GetGuaranteedLargestMessagePayload();
    if (max_datagram_length > 0) {
      // Datagrams are supported in this case.
      negotiated_params.set_max_datagram_length(max_datagram_length);
    }
    delegate_->OnConnected(negotiated_params);
  }
}

void P2PQuicTransportImpl::OnTlsHandshakeComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QuicSession::OnTlsHandshakeComplete();
  DCHECK(IsEncryptionEstablished());
  DCHECK(OneRttKeysAvailable());
  P2PQuicNegotiatedParams negotiated_params;
  // The guaranteed largest message payload will not change throughout the
  // connection.
  uint16_t max_datagram_length =
      quic::QuicSession::GetGuaranteedLargestMessagePayload();
  if (max_datagram_length > 0) {
    // Datagrams are supported in this case.
    negotiated_params.set_max_datagram_length(max_datagram_length);
  }
  delegate_->OnConnected(negotiated_params);
}

void P2PQuicTransportImpl::OnCanWrite() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  while (!datagram_buffer_.empty()) {
    if (TrySendDatagram(datagram_buffer_.front())) {
      datagram_buffer_.pop();
    } else {
      // Keep the message in the buffer to be written when we can write again.
      return;
    }
  }

  // We have successfully sent all buffered datagrams.
  QuicSession::OnCanWrite();
}

void P2PQuicTransportImpl::OnMessageReceived(
    quiche::QuicheStringPiece message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // This will never overflow because of the datagram size limit.
  Vector<uint8_t> datagram(static_cast<wtf_size_t>(message.size()));
  memcpy(datagram.data(), message.data(), message.size());
  delegate_->OnDatagramReceived(std::move(datagram));
}

void P2PQuicTransportImpl::OnMessageLost(quic::QuicMessageId message_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  num_datagrams_lost_++;
}

void P2PQuicTransportImpl::OnConnectionClosed(
    const quic::QuicConnectionCloseFrame& frame,
    quic::ConnectionCloseSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const quic::QuicErrorCode error = frame.quic_error_code;
  const std::string& error_details = frame.error_details;
  quic::QuicSession::OnConnectionClosed(frame, source);
  if (error != quic::QuicErrorCode::QUIC_CONNECTION_CANCELLED) {
    delegate_->OnConnectionFailed(
        error_details, source == quic::ConnectionCloseSource::FROM_PEER);
  } else if (source == quic::ConnectionCloseSource::FROM_PEER) {
    // This connection was closed by the application of the remote side.
    delegate_->OnRemoteStopped();
  }
}

bool P2PQuicTransportImpl::ShouldKeepConnectionAlive() const {
  return GetNumActiveStreams() > 0;
}

bool P2PQuicTransportImpl::IsClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !connection_->connected();
}

}  // namespace blink
