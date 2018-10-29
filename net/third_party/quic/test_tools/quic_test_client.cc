// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/quic_test_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "net/third_party/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_epoll_connection_helper.h"
#include "net/third_party/quic/core/quic_packet_writer_wrapper.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_stack_trace.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_client_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/tools/quic_url.h"

namespace quic {
namespace test {
namespace {

// RecordingProofVerifier accepts any certificate chain and records the common
// name of the leaf and then delegates the actual verfication to an actual
// verifier. If no optional verifier is provided, then VerifyProof will return
// success.
class RecordingProofVerifier : public ProofVerifier {
 public:
  explicit RecordingProofVerifier(std::unique_ptr<ProofVerifier> verifier)
      : verifier_(std::move(verifier)) {}

  // ProofVerifier interface.
  QuicAsyncStatus VerifyProof(
      const QuicString& hostname,
      const uint16_t port,
      const QuicString& server_config,
      QuicTransportVersion transport_version,
      QuicStringPiece chlo_hash,
      const std::vector<QuicString>& certs,
      const QuicString& cert_sct,
      const QuicString& signature,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    common_name_.clear();
    if (certs.empty()) {
      return QUIC_FAILURE;
    }

    // Convert certs to X509Certificate.
    std::vector<QuicStringPiece> cert_pieces(certs.size());
    for (unsigned i = 0; i < certs.size(); i++) {
      cert_pieces[i] = QuicStringPiece(certs[i]);
    }
    // TODO(rtenneti): Fix after adding support for real certs. Currently,
    // cert_pieces are "leaf" and "intermediate" and CreateFromDERCertChain
    // fails to return cert from these cert_pieces.
    //    bssl::UniquePtr<X509> cert(d2i_X509(nullptr, &data, certs[0].size()));
    //    if (!cert.get()) {
    //      return QUIC_FAILURE;
    //    }
    //
    //    common_name_ = cert->subject().GetDisplayName();
    cert_sct_ = cert_sct;

    if (!verifier_) {
      return QUIC_SUCCESS;
    }

    return verifier_->VerifyProof(hostname, port, server_config,
                                  transport_version, chlo_hash, certs, cert_sct,
                                  signature, context, error_details, details,
                                  std::move(callback));
  }

  QuicAsyncStatus VerifyCertChain(
      const QuicString& hostname,
      const std::vector<QuicString>& certs,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return QUIC_SUCCESS;
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    return verifier_->CreateDefaultContext();
  }

  const QuicString& common_name() const { return common_name_; }

  const QuicString& cert_sct() const { return cert_sct_; }

 private:
  std::unique_ptr<ProofVerifier> verifier_;
  QuicString common_name_;
  QuicString cert_sct_;
};
}  // namespace

class MockableQuicClientEpollNetworkHelper
    : public QuicClientEpollNetworkHelper {
 public:
  using QuicClientEpollNetworkHelper::QuicClientEpollNetworkHelper;
  ~MockableQuicClientEpollNetworkHelper() override = default;

  void ProcessPacket(const QuicSocketAddress& self_address,
                     const QuicSocketAddress& peer_address,
                     const QuicReceivedPacket& packet) override {
    QuicClientEpollNetworkHelper::ProcessPacket(self_address, peer_address,
                                                packet);
    if (track_last_incoming_packet_) {
      last_incoming_packet_ = packet.Clone();
    }
  }

  QuicPacketWriter* CreateQuicPacketWriter() override {
    QuicPacketWriter* writer =
        QuicClientEpollNetworkHelper::CreateQuicPacketWriter();
    if (!test_writer_) {
      return writer;
    }
    test_writer_->set_writer(writer);
    return test_writer_;
  }

  const QuicReceivedPacket* last_incoming_packet() {
    return last_incoming_packet_.get();
  }

  void set_track_last_incoming_packet(bool track) {
    track_last_incoming_packet_ = track;
  }

  void UseWriter(QuicPacketWriterWrapper* writer) {
    CHECK(test_writer_ == nullptr);
    test_writer_ = writer;
  }

  void set_peer_address(const QuicSocketAddress& address) {
    CHECK(test_writer_ != nullptr);
    test_writer_->set_peer_address(address);
  }

 private:
  QuicPacketWriterWrapper* test_writer_ = nullptr;
  // The last incoming packet, iff |track_last_incoming_packet_| is true.
  std::unique_ptr<QuicReceivedPacket> last_incoming_packet_;
  // If true, copy each packet from ProcessPacket into |last_incoming_packet_|
  bool track_last_incoming_packet_ = false;
};

MockableQuicClient::MockableQuicClient(
    QuicSocketAddress server_address,
    const QuicServerId& server_id,
    const ParsedQuicVersionVector& supported_versions,
    net::EpollServer* epoll_server)
    : MockableQuicClient(server_address,
                         server_id,
                         QuicConfig(),
                         supported_versions,
                         epoll_server) {}

MockableQuicClient::MockableQuicClient(
    QuicSocketAddress server_address,
    const QuicServerId& server_id,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    net::EpollServer* epoll_server)
    : MockableQuicClient(server_address,
                         server_id,
                         config,
                         supported_versions,
                         epoll_server,
                         nullptr) {}

MockableQuicClient::MockableQuicClient(
    QuicSocketAddress server_address,
    const QuicServerId& server_id,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    net::EpollServer* epoll_server,
    std::unique_ptr<ProofVerifier> proof_verifier)
    : QuicClient(
          server_address,
          server_id,
          supported_versions,
          config,
          epoll_server,
          QuicMakeUnique<MockableQuicClientEpollNetworkHelper>(epoll_server,
                                                               this),
          QuicWrapUnique(
              new RecordingProofVerifier(std::move(proof_verifier)))),
      override_connection_id_(0) {}

MockableQuicClient::~MockableQuicClient() {
  if (connected()) {
    Disconnect();
  }
}

MockableQuicClientEpollNetworkHelper*
MockableQuicClient::mockable_network_helper() {
  return static_cast<MockableQuicClientEpollNetworkHelper*>(
      epoll_network_helper());
}

const MockableQuicClientEpollNetworkHelper*
MockableQuicClient::mockable_network_helper() const {
  return static_cast<const MockableQuicClientEpollNetworkHelper*>(
      epoll_network_helper());
}

QuicConnectionId MockableQuicClient::GenerateNewConnectionId() {
  return override_connection_id_ ? override_connection_id_
                                 : QuicClient::GenerateNewConnectionId();
}

void MockableQuicClient::UseConnectionId(QuicConnectionId connection_id) {
  override_connection_id_ = connection_id;
}

void MockableQuicClient::UseWriter(QuicPacketWriterWrapper* writer) {
  mockable_network_helper()->UseWriter(writer);
}

void MockableQuicClient::set_peer_address(const QuicSocketAddress& address) {
  mockable_network_helper()->set_peer_address(address);
}

const QuicReceivedPacket* MockableQuicClient::last_incoming_packet() {
  return mockable_network_helper()->last_incoming_packet();
}

void MockableQuicClient::set_track_last_incoming_packet(bool track) {
  mockable_network_helper()->set_track_last_incoming_packet(track);
}

QuicTestClient::QuicTestClient(
    QuicSocketAddress server_address,
    const QuicString& server_hostname,
    const ParsedQuicVersionVector& supported_versions)
    : QuicTestClient(server_address,
                     server_hostname,
                     QuicConfig(),
                     supported_versions) {}

QuicTestClient::QuicTestClient(
    QuicSocketAddress server_address,
    const QuicString& server_hostname,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions)
    : client_(new MockableQuicClient(
          server_address,
          QuicServerId(server_hostname, server_address.port(), false),
          config,
          supported_versions,
          &epoll_server_)) {
  Initialize();
}

QuicTestClient::QuicTestClient(
    QuicSocketAddress server_address,
    const QuicString& server_hostname,
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    std::unique_ptr<ProofVerifier> proof_verifier)
    : client_(new MockableQuicClient(
          server_address,
          QuicServerId(server_hostname, server_address.port(), false),
          config,
          supported_versions,
          &epoll_server_,
          std::move(proof_verifier))) {
  Initialize();
}

QuicTestClient::QuicTestClient() = default;

QuicTestClient::~QuicTestClient() {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    stream.second->set_visitor(nullptr);
  }
}

void QuicTestClient::Initialize() {
  priority_ = 3;
  connect_attempted_ = false;
  auto_reconnect_ = false;
  buffer_body_ = true;
  num_requests_ = 0;
  num_responses_ = 0;
  ClearPerConnectionState();
  // As chrome will generally do this, we want it to be the default when it's
  // not overridden.
  if (!client_->config()->HasSetBytesForConnectionIdToSend()) {
    client_->config()->SetBytesForConnectionIdToSend(0);
  }
}

void QuicTestClient::SetUserAgentID(const QuicString& user_agent_id) {
  client_->SetUserAgentID(user_agent_id);
}

ssize_t QuicTestClient::SendRequest(const QuicString& uri) {
  spdy::SpdyHeaderBlock headers;
  if (!PopulateHeaderBlockFromUrl(uri, &headers)) {
    return 0;
  }
  return SendMessage(headers, "");
}

ssize_t QuicTestClient::SendRequestAndRstTogether(const QuicString& uri) {
  spdy::SpdyHeaderBlock headers;
  if (!PopulateHeaderBlockFromUrl(uri, &headers)) {
    return 0;
  }

  QuicSpdyClientSession* session = client()->client_session();
  QuicConnection::ScopedPacketFlusher flusher(
      session->connection(), QuicConnection::SEND_ACK_IF_PENDING);
  ssize_t ret = SendMessage(headers, "", /*fin=*/true, /*flush=*/false);

  QuicStreamId stream_id =
      QuicSpdySessionPeer::GetNthClientInitiatedStreamId(*session, 0);
  session->SendRstStream(stream_id, QUIC_STREAM_CANCELLED, 0);
  return ret;
}

void QuicTestClient::SendRequestsAndWaitForResponses(
    const std::vector<QuicString>& url_list) {
  for (const QuicString& url : url_list) {
    SendRequest(url);
  }
  while (client()->WaitForEvents()) {
  }
}

ssize_t QuicTestClient::GetOrCreateStreamAndSendRequest(
    const spdy::SpdyHeaderBlock* headers,
    QuicStringPiece body,
    bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (headers) {
    QuicClientPushPromiseIndex::TryHandle* handle;
    QuicAsyncStatus rv =
        client()->push_promise_index()->Try(*headers, this, &handle);
    if (rv == QUIC_SUCCESS)
      return 1;
    if (rv == QUIC_PENDING) {
      // May need to retry request if asynchronous rendezvous fails.
      std::unique_ptr<spdy::SpdyHeaderBlock> new_headers(
          new spdy::SpdyHeaderBlock(headers->Clone()));
      push_promise_data_to_resend_ = QuicMakeUnique<TestClientDataToResend>(
          std::move(new_headers), body, fin, this, std::move(ack_listener));
      return 1;
    }
  }

  // Maybe it's better just to overload this.  it's just that we need
  // for the GetOrCreateStream function to call something else...which
  // is icky and complicated, but maybe not worse than this.
  QuicSpdyClientStream* stream = GetOrCreateStream();
  if (stream == nullptr) {
    return 0;
  }
  QuicStreamPeer::set_ack_listener(stream, ack_listener);

  ssize_t ret = 0;
  if (headers != nullptr) {
    spdy::SpdyHeaderBlock spdy_headers(headers->Clone());
    if (spdy_headers[":authority"].as_string().empty()) {
      spdy_headers[":authority"] = client_->server_id().host();
    }
    ret = stream->SendRequest(std::move(spdy_headers), body, fin);
    ++num_requests_;
  } else {
    stream->WriteOrBufferBody(QuicString(body), fin, ack_listener);
    ret = body.length();
  }
  if (GetQuicReloadableFlag(enable_quic_stateless_reject_support)) {
    std::unique_ptr<spdy::SpdyHeaderBlock> new_headers;
    if (headers) {
      new_headers = QuicMakeUnique<spdy::SpdyHeaderBlock>(headers->Clone());
    }
    std::unique_ptr<QuicSpdyClientBase::QuicDataToResend> data_to_resend(
        new TestClientDataToResend(std::move(new_headers), body, fin, this,
                                   ack_listener));
    client()->MaybeAddQuicDataToResend(std::move(data_to_resend));
  }
  return ret;
}

ssize_t QuicTestClient::SendMessage(const spdy::SpdyHeaderBlock& headers,
                                    QuicStringPiece body) {
  return SendMessage(headers, body, /*fin=*/true);
}

ssize_t QuicTestClient::SendMessage(const spdy::SpdyHeaderBlock& headers,
                                    QuicStringPiece body,
                                    bool fin) {
  return SendMessage(headers, body, fin, /*flush=*/true);
}

ssize_t QuicTestClient::SendMessage(const spdy::SpdyHeaderBlock& headers,
                                    QuicStringPiece body,
                                    bool fin,
                                    bool flush) {
  // Always force creation of a stream for SendMessage.
  latest_created_stream_ = nullptr;

  ssize_t ret = GetOrCreateStreamAndSendRequest(&headers, body, fin, nullptr);

  if (flush) {
    WaitForWriteToFlush();
  }
  return ret;
}

ssize_t QuicTestClient::SendData(const QuicString& data, bool last_data) {
  return SendData(data, last_data, nullptr);
}

ssize_t QuicTestClient::SendData(
    const QuicString& data,
    bool last_data,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  return GetOrCreateStreamAndSendRequest(nullptr, QuicStringPiece(data),
                                         last_data, std::move(ack_listener));
}

bool QuicTestClient::response_complete() const {
  return response_complete_;
}

int64_t QuicTestClient::response_body_size() const {
  return response_body_size_;
}

bool QuicTestClient::buffer_body() const {
  return buffer_body_;
}

void QuicTestClient::set_buffer_body(bool buffer_body) {
  buffer_body_ = buffer_body;
}

const QuicString& QuicTestClient::response_body() const {
  return response_;
}

QuicString QuicTestClient::SendCustomSynchronousRequest(
    const spdy::SpdyHeaderBlock& headers,
    const QuicString& body) {
  // Clear connection state here and only track this synchronous request.
  ClearPerConnectionState();
  if (SendMessage(headers, body) == 0) {
    QUIC_DLOG(ERROR) << "Failed the request for: " << headers.DebugString();
    // Set the response_ explicitly.  Otherwise response_ will contain the
    // response from the previously successful request.
    response_ = "";
  } else {
    WaitForResponse();
  }
  return response_;
}

QuicString QuicTestClient::SendSynchronousRequest(const QuicString& uri) {
  spdy::SpdyHeaderBlock headers;
  if (!PopulateHeaderBlockFromUrl(uri, &headers)) {
    return "";
  }
  return SendCustomSynchronousRequest(headers, "");
}

void QuicTestClient::SendConnectivityProbing() {
  QuicConnection* connection = client()->client_session()->connection();
  connection->SendConnectivityProbingPacket(connection->writer(),
                                            connection->peer_address());
}

void QuicTestClient::SetLatestCreatedStream(QuicSpdyClientStream* stream) {
  latest_created_stream_ = stream;
  if (latest_created_stream_ != nullptr) {
    open_streams_[stream->id()] = stream;
    stream->set_visitor(this);
  }
}

QuicSpdyClientStream* QuicTestClient::GetOrCreateStream() {
  if (!connect_attempted_ || auto_reconnect_) {
    if (!connected()) {
      Connect();
    }
    if (!connected()) {
      return nullptr;
    }
  }
  if (open_streams_.empty()) {
    ClearPerConnectionState();
  }
  if (!latest_created_stream_) {
    SetLatestCreatedStream(client_->CreateClientStream());
    if (latest_created_stream_) {
      latest_created_stream_->SetPriority(priority_);
    }
  }

  return latest_created_stream_;
}

QuicErrorCode QuicTestClient::connection_error() {
  return client()->connection_error();
}

MockableQuicClient* QuicTestClient::client() {
  return client_.get();
}

const QuicString& QuicTestClient::cert_common_name() const {
  return reinterpret_cast<RecordingProofVerifier*>(client_->proof_verifier())
      ->common_name();
}

const QuicString& QuicTestClient::cert_sct() const {
  return reinterpret_cast<RecordingProofVerifier*>(client_->proof_verifier())
      ->cert_sct();
}

QuicTagValueMap QuicTestClient::GetServerConfig() const {
  QuicCryptoClientConfig* config = client_->crypto_config();
  QuicCryptoClientConfig::CachedState* state =
      config->LookupOrCreate(client_->server_id());
  const CryptoHandshakeMessage* handshake_msg = state->GetServerConfig();
  if (handshake_msg != nullptr) {
    return handshake_msg->tag_value_map();
  } else {
    return QuicTagValueMap();
  }
}

bool QuicTestClient::connected() const {
  return client_->connected();
}

void QuicTestClient::Connect() {
  DCHECK(!connected());
  if (!connect_attempted_) {
    client_->Initialize();
  }

  // If we've been asked to override SNI, set it now
  if (override_sni_set_) {
    client_->set_server_id(
        QuicServerId(override_sni_, address().port(), false));
  }

  client_->Connect();
  connect_attempted_ = true;
}

void QuicTestClient::ResetConnection() {
  Disconnect();
  Connect();
}

void QuicTestClient::Disconnect() {
  ClearPerConnectionState();
  client_->Disconnect();
  connect_attempted_ = false;
}

QuicSocketAddress QuicTestClient::local_address() const {
  return client_->network_helper()->GetLatestClientAddress();
}

void QuicTestClient::ClearPerRequestState() {
  stream_error_ = QUIC_STREAM_NO_ERROR;
  response_ = "";
  response_complete_ = false;
  response_headers_complete_ = false;
  preliminary_headers_.clear();
  response_headers_.clear();
  response_trailers_.clear();
  bytes_read_ = 0;
  bytes_written_ = 0;
  response_body_size_ = 0;
}

bool QuicTestClient::HaveActiveStream() {
  return push_promise_data_to_resend_.get() || !open_streams_.empty();
}

bool QuicTestClient::WaitUntil(int timeout_ms, std::function<bool()> trigger) {
  int64_t timeout_us = timeout_ms * base::Time::kMicrosecondsPerMillisecond;
  int64_t old_timeout_us = epoll_server()->timeout_in_us();
  if (timeout_us > 0) {
    epoll_server()->set_timeout_in_us(timeout_us);
  }
  const QuicClock* clock =
      QuicConnectionPeer::GetHelper(client()->session()->connection())
          ->GetClock();
  QuicTime end_waiting_time =
      clock->Now() + QuicTime::Delta::FromMicroseconds(timeout_us);
  while (HaveActiveStream() && !(trigger && trigger()) &&
         (timeout_us < 0 || clock->Now() < end_waiting_time)) {
    client_->WaitForEvents();
  }
  ReadNextResponse();
  if (timeout_us > 0) {
    epoll_server()->set_timeout_in_us(old_timeout_us);
  }
  if (trigger && !trigger()) {
    VLOG(1) << "Client WaitUntil returning with trigger returning false."
            << QuicStackTrace();
    return false;
  }
  return true;
}

ssize_t QuicTestClient::Send(const void* buffer, size_t size) {
  return SendData(QuicString(static_cast<const char*>(buffer), size), false);
}

bool QuicTestClient::response_headers_complete() const {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    if (stream.second->headers_decompressed()) {
      return true;
    }
  }
  return response_headers_complete_;
}

const spdy::SpdyHeaderBlock* QuicTestClient::response_headers() const {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    size_t bytes_read =
        stream.second->stream_bytes_read() + stream.second->header_bytes_read();
    if (bytes_read > 0) {
      response_headers_ = stream.second->response_headers().Clone();
      break;
    }
  }
  return &response_headers_;
}

const spdy::SpdyHeaderBlock* QuicTestClient::preliminary_headers() const {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    size_t bytes_read =
        stream.second->stream_bytes_read() + stream.second->header_bytes_read();
    if (bytes_read > 0) {
      preliminary_headers_ = stream.second->preliminary_headers().Clone();
      break;
    }
  }
  return &preliminary_headers_;
}

const spdy::SpdyHeaderBlock& QuicTestClient::response_trailers() const {
  return response_trailers_;
}

int64_t QuicTestClient::response_size() const {
  return bytes_read();
}

size_t QuicTestClient::bytes_read() const {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    size_t bytes_read =
        stream.second->stream_bytes_read() + stream.second->header_bytes_read();
    if (bytes_read > 0) {
      return bytes_read;
    }
  }
  return bytes_read_;
}

size_t QuicTestClient::bytes_written() const {
  for (std::pair<QuicStreamId, QuicSpdyClientStream*> stream : open_streams_) {
    size_t bytes_written = stream.second->stream_bytes_written() +
                           stream.second->header_bytes_written();
    if (bytes_written > 0) {
      return bytes_written;
    }
  }
  return bytes_written_;
}

void QuicTestClient::OnClose(QuicSpdyStream* stream) {
  if (stream == nullptr) {
    return;
  }
  // Always close the stream, regardless of whether it was the last stream
  // written.
  client()->OnClose(stream);
  ++num_responses_;
  if (!QuicContainsKey(open_streams_, stream->id())) {
    return;
  }
  if (latest_created_stream_ == stream) {
    latest_created_stream_ = nullptr;
  }
  QuicSpdyClientStream* client_stream =
      static_cast<QuicSpdyClientStream*>(stream);
  QuicStreamId id = client_stream->id();
  closed_stream_states_.insert(std::make_pair(
      id,
      PerStreamState(
          client_stream->stream_error(), true,
          client_stream->headers_decompressed(),
          client_stream->response_headers(),
          client_stream->preliminary_headers(),
          (buffer_body() ? client_stream->data() : ""),
          client_stream->received_trailers(),
          // Use NumBytesConsumed to avoid counting retransmitted stream frames.
          QuicStreamPeer::sequencer(client_stream)->NumBytesConsumed() +
              client_stream->header_bytes_read(),
          client_stream->stream_bytes_written() +
              client_stream->header_bytes_written(),
          client_stream->data().size())));
  open_streams_.erase(id);
}

bool QuicTestClient::CheckVary(const spdy::SpdyHeaderBlock& client_request,
                               const spdy::SpdyHeaderBlock& promise_request,
                               const spdy::SpdyHeaderBlock& promise_response) {
  return true;
}

void QuicTestClient::OnRendezvousResult(QuicSpdyStream* stream) {
  std::unique_ptr<TestClientDataToResend> data_to_resend =
      std::move(push_promise_data_to_resend_);
  SetLatestCreatedStream(static_cast<QuicSpdyClientStream*>(stream));
  if (stream) {
    stream->OnDataAvailable();
  } else if (data_to_resend) {
    data_to_resend->Resend();
  }
}

void QuicTestClient::UseWriter(QuicPacketWriterWrapper* writer) {
  client_->UseWriter(writer);
}

void QuicTestClient::UseConnectionId(QuicConnectionId connection_id) {
  DCHECK(!connected());
  client_->UseConnectionId(connection_id);
}

bool QuicTestClient::MigrateSocket(const QuicIpAddress& new_host) {
  return client_->MigrateSocket(new_host);
}

bool QuicTestClient::MigrateSocketWithSpecifiedPort(
    const QuicIpAddress& new_host,
    int port) {
  client_->set_local_port(port);
  return client_->MigrateSocket(new_host);
}

QuicIpAddress QuicTestClient::bind_to_address() const {
  return client_->bind_to_address();
}

void QuicTestClient::set_bind_to_address(QuicIpAddress address) {
  client_->set_bind_to_address(address);
}

const QuicSocketAddress& QuicTestClient::address() const {
  return client_->server_address();
}

void QuicTestClient::WaitForWriteToFlush() {
  while (connected() && client()->session()->HasDataToWrite()) {
    client_->WaitForEvents();
  }
}

QuicTestClient::TestClientDataToResend::TestClientDataToResend(
    std::unique_ptr<spdy::SpdyHeaderBlock> headers,
    QuicStringPiece body,
    bool fin,
    QuicTestClient* test_client,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener)
    : QuicClient::QuicDataToResend(std::move(headers), body, fin),
      test_client_(test_client),
      ack_listener_(std::move(ack_listener)) {}

QuicTestClient::TestClientDataToResend::~TestClientDataToResend() = default;

void QuicTestClient::TestClientDataToResend::Resend() {
  test_client_->GetOrCreateStreamAndSendRequest(headers_.get(), body_, fin_,
                                                ack_listener_);
  headers_.reset();
}

QuicTestClient::PerStreamState::PerStreamState(const PerStreamState& other)
    : stream_error(other.stream_error),
      response_complete(other.response_complete),
      response_headers_complete(other.response_headers_complete),
      response_headers(other.response_headers.Clone()),
      preliminary_headers(other.preliminary_headers.Clone()),
      response(other.response),
      response_trailers(other.response_trailers.Clone()),
      bytes_read(other.bytes_read),
      bytes_written(other.bytes_written),
      response_body_size(other.response_body_size) {}

QuicTestClient::PerStreamState::PerStreamState(
    QuicRstStreamErrorCode stream_error,
    bool response_complete,
    bool response_headers_complete,
    const spdy::SpdyHeaderBlock& response_headers,
    const spdy::SpdyHeaderBlock& preliminary_headers,
    const QuicString& response,
    const spdy::SpdyHeaderBlock& response_trailers,
    uint64_t bytes_read,
    uint64_t bytes_written,
    int64_t response_body_size)
    : stream_error(stream_error),
      response_complete(response_complete),
      response_headers_complete(response_headers_complete),
      response_headers(response_headers.Clone()),
      preliminary_headers(preliminary_headers.Clone()),
      response(response),
      response_trailers(response_trailers.Clone()),
      bytes_read(bytes_read),
      bytes_written(bytes_written),
      response_body_size(response_body_size) {}

QuicTestClient::PerStreamState::~PerStreamState() = default;

bool QuicTestClient::PopulateHeaderBlockFromUrl(
    const QuicString& uri,
    spdy::SpdyHeaderBlock* headers) {
  QuicString url;
  if (QuicTextUtils::StartsWith(uri, "https://") ||
      QuicTextUtils::StartsWith(uri, "http://")) {
    url = uri;
  } else if (uri[0] == '/') {
    url = "https://" + client_->server_id().host() + uri;
  } else {
    url = "https://" + uri;
  }
  return SpdyUtils::PopulateHeaderBlockFromUrl(url, headers);
}

void QuicTestClient::ReadNextResponse() {
  if (closed_stream_states_.empty()) {
    return;
  }

  PerStreamState state(closed_stream_states_.front().second);

  stream_error_ = state.stream_error;
  response_ = state.response;
  response_complete_ = state.response_complete;
  response_headers_complete_ = state.response_headers_complete;
  preliminary_headers_ = state.preliminary_headers.Clone();
  response_headers_ = state.response_headers.Clone();
  response_trailers_ = state.response_trailers.Clone();
  bytes_read_ = state.bytes_read;
  bytes_written_ = state.bytes_written;
  response_body_size_ = state.response_body_size;

  closed_stream_states_.pop_front();
}

void QuicTestClient::ClearPerConnectionState() {
  ClearPerRequestState();
  open_streams_.clear();
  closed_stream_states_.clear();
  latest_created_stream_ = nullptr;
}

void QuicTestClient::WaitForDelayedAcks() {
  // kWaitDuration is a period of time that is long enough for all delayed
  // acks to be sent and received on the other end.
  const QuicTime::Delta kWaitDuration =
      4 * QuicTime::Delta::FromMilliseconds(kDefaultDelayedAckTimeMs);

  const QuicClock* clock = client()->client_session()->connection()->clock();

  QuicTime wait_until = clock->ApproximateNow() + kWaitDuration;
  while (clock->ApproximateNow() < wait_until) {
    // This waits for up to 50 ms.
    client()->WaitForEvents();
  }
}

}  // namespace test
}  // namespace quic
