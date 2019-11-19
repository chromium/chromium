// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport_stats.h"

namespace blink {
namespace {
// QUIC requires 128 bits of entropy for the pre shared key.
const size_t kPreSharedKeyLength = 128 / 8;

// This class wraps a P2PQuicTransportFactoryImpl but does not construct it
// until CreateQuicTransport is called for the first time. This ensures that it
// is executed on the WebRTC worker thread.
class DefaultP2PQuicTransportFactory : public P2PQuicTransportFactory {
 public:
  explicit DefaultP2PQuicTransportFactory(
      scoped_refptr<base::SingleThreadTaskRunner> host_thread)
      : host_thread_(std::move(host_thread)) {
    DCHECK(host_thread_);
  }

  // P2PQuicTransportFactory overrides.
  std::unique_ptr<P2PQuicTransport> CreateQuicTransport(
      P2PQuicTransport::Delegate* delegate,
      P2PQuicPacketTransport* packet_transport,
      const P2PQuicTransportConfig& config) override {
    DCHECK(host_thread_->RunsTasksInCurrentSequence());
    return GetFactory()->CreateQuicTransport(delegate, packet_transport,
                                             config);
  }

 private:
  P2PQuicTransportFactory* GetFactory() {
    DCHECK(host_thread_->RunsTasksInCurrentSequence());
    if (!factory_impl_) {
      quic::QuicClock* clock = quic::QuicChromiumClock::GetInstance();
      auto alarm_factory = std::make_unique<net::QuicChromiumAlarmFactory>(
          host_thread_.get(), clock);
      factory_impl_ = std::make_unique<P2PQuicTransportFactoryImpl>(
          clock, std::move(alarm_factory));
    }
    return factory_impl_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> host_thread_;
  std::unique_ptr<P2PQuicTransportFactory> factory_impl_;
};

void RejectPromise(ScriptPromiseResolver* promise_resolver,
                   const char* method_name) {
  ScriptState::Scope scope(promise_resolver->GetScriptState());
  ExceptionState exception_state(
      promise_resolver->GetScriptState()->GetIsolate(),
      ExceptionState::kExecutionContext, "RTCQuicTransport", method_name);
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The RTCQuicTransport is closed.");
  promise_resolver->Reject(exception_state);
}

}  // namespace

RTCQuicTransport* RTCQuicTransport::Create(ExecutionContext* context,
                                           RTCIceTransport* transport,
                                           ExceptionState& exception_state) {
  return Create(context, transport, {}, exception_state);
}

RTCQuicTransport* RTCQuicTransport::Create(
    ExecutionContext* context,
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state) {
  return Create(context, transport, certificates, exception_state,
                std::make_unique<DefaultP2PQuicTransportFactory>(
                    PeerConnectionDependencyFactory::GetInstance()
                        ->GetWebRtcWorkerTaskRunner()));
}

RTCQuicTransport* RTCQuicTransport::Create(
    ExecutionContext* context,
    RTCIceTransport* transport,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state,
    std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory) {
  DCHECK(context);
  DCHECK(transport);
  DCHECK(p2p_quic_transport_factory);
  if (transport->IsClosed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot construct an RTCQuicTransport "
                                      "with a closed RTCIceTransport.");
    return nullptr;
  }
  if (transport->HasConsumer()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot construct an RTCQuicTransport "
                                      "with an RTCIceTransport that already "
                                      "has a connected RTCQuicTransport.");
    return nullptr;
  }
  if (transport->IsFromPeerConnection()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot construct an RTCQuicTransport "
        "with an RTCIceTransport that came from an "
        "RTCPeerConnection.");
    return nullptr;
  }
  for (const auto& certificate : certificates) {
    if (certificate->expires() <
        ConvertSecondsToDOMTimeStamp(base::Time::Now().ToDoubleT())) {
      exception_state.ThrowTypeError(
          "Cannot construct an RTCQuicTransport with an expired "
          "certificate.");
      return nullptr;
    }
  }
  uint8_t generated_key[kPreSharedKeyLength];
  quic::QuicRandom::GetInstance()->RandBytes(generated_key,
                                             base::size(generated_key));
  return MakeGarbageCollected<RTCQuicTransport>(
      context, transport,
      DOMArrayBuffer::Create(generated_key, base::size(generated_key)),
      certificates, exception_state, std::move(p2p_quic_transport_factory));
}

RTCQuicTransport::RTCQuicTransport(
    ExecutionContext* context,
    RTCIceTransport* transport,
    DOMArrayBuffer* key,
    const HeapVector<Member<RTCCertificate>>& certificates,
    ExceptionState& exception_state,
    std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory)
    : ContextClient(context),
      transport_(transport),
      key_(key),
      certificates_(certificates),
      p2p_quic_transport_factory_(std::move(p2p_quic_transport_factory)) {
  DCHECK_GT(key_->ByteLengthAsSizeT(), 0u);
  transport->ConnectConsumer(this);
}

RTCQuicTransport::~RTCQuicTransport() {
  DCHECK(!proxy_);
}

RTCIceTransport* RTCQuicTransport::transport() const {
  return transport_;
}

String RTCQuicTransport::state() const {
  switch (state_) {
    case RTCQuicTransportState::kNew:
      return "new";
    case RTCQuicTransportState::kConnecting:
      return "connecting";
    case RTCQuicTransportState::kConnected:
      return "connected";
    case RTCQuicTransportState::kClosed:
      return "closed";
    case RTCQuicTransportState::kFailed:
      return "failed";
  }
  return String();
}

DOMArrayBuffer* RTCQuicTransport::getKey() const {
  return DOMArrayBuffer::Create(key_->Data(), key_->ByteLengthAsSizeT());
}

void RTCQuicTransport::connect(ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (RaiseExceptionIfStarted(exception_state)) {
    return;
  }
  start_reason_ = StartReason::kClientConnecting;
  std::string pre_shared_key(static_cast<const char*>(key_->Data()),
                             key_->ByteLengthAsSizeT());
  StartConnection(quic::Perspective::IS_CLIENT,
                  P2PQuicTransport::StartConfig(pre_shared_key));
}

void RTCQuicTransport::listen(const DOMArrayPiece& remote_key,
                              ExceptionState& exception_state) {
  if (remote_key.ByteLength() == 0u) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Cannot listen with an empty key.");
    return;
  }
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (RaiseExceptionIfStarted(exception_state)) {
    return;
  }
  start_reason_ = StartReason::kServerListening;
  std::string pre_shared_key(static_cast<const char*>(remote_key.Data()),
                             remote_key.ByteLength());
  StartConnection(quic::Perspective::IS_SERVER,
                  P2PQuicTransport::StartConfig(pre_shared_key));
}

RTCQuicParameters* RTCQuicTransport::getLocalParameters() const {
  RTCQuicParameters* result = RTCQuicParameters::Create();

  HeapVector<Member<RTCDtlsFingerprint>> fingerprints;
  for (const auto& certificate : certificates_) {
    // TODO(github.com/w3c/webrtc-quic/issues/33): The specification says that
    // getLocalParameters should return one fingerprint per certificate but is
    // not clear which one to pick if an RTCCertificate has multiple
    // fingerprints.
    for (const auto& certificate_fingerprint : certificate->getFingerprints()) {
      fingerprints.push_back(certificate_fingerprint);
    }
  }
  result->setFingerprints(fingerprints);
  return result;
}

RTCQuicParameters* RTCQuicTransport::getRemoteParameters() const {
  return remote_parameters_;
}

const HeapVector<Member<RTCCertificate>>& RTCQuicTransport::getCertificates()
    const {
  return certificates_;
}

const HeapVector<Member<DOMArrayBuffer>>&
RTCQuicTransport::getRemoteCertificates() const {
  return remote_certificates_;
}

static quic::Perspective QuicPerspectiveFromIceRole(cricket::IceRole ice_role) {
  switch (ice_role) {
    case cricket::ICEROLE_CONTROLLED:
      return quic::Perspective::IS_CLIENT;
    case cricket::ICEROLE_CONTROLLING:
      return quic::Perspective::IS_SERVER;
    default:
      NOTREACHED();
  }
  return quic::Perspective::IS_CLIENT;
}

static std::unique_ptr<rtc::SSLFingerprint> RTCDtlsFingerprintToSSLFingerprint(
    const RTCDtlsFingerprint* dtls_fingerprint) {
  std::string algorithm = dtls_fingerprint->algorithm().Utf8();
  std::string value = dtls_fingerprint->value().Utf8();
  std::unique_ptr<rtc::SSLFingerprint> rtc_fingerprint =
      rtc::SSLFingerprint::CreateUniqueFromRfc4572(algorithm, value);
  DCHECK(rtc_fingerprint);
  return rtc_fingerprint;
}

void RTCQuicTransport::start(RTCQuicParameters* remote_parameters,
                             ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (RaiseExceptionIfStarted(exception_state)) {
    return;
  }
  remote_parameters_ = remote_parameters;
  start_reason_ = StartReason::kP2PWithRemoteFingerprints;
  if (transport_->IsStarted()) {
    Vector<std::unique_ptr<rtc::SSLFingerprint>> rtc_fingerprints;
    for (const RTCDtlsFingerprint* fingerprint :
         remote_parameters_->fingerprints()) {
      rtc_fingerprints.push_back(
          RTCDtlsFingerprintToSSLFingerprint(fingerprint));
    };
    StartConnection(QuicPerspectiveFromIceRole(transport_->GetRole()),
                    P2PQuicTransport::StartConfig(std::move(rtc_fingerprints)));
  }
}

void RTCQuicTransport::StartConnection(
    quic::Perspective perspective,
    P2PQuicTransport::StartConfig start_config) {
  DCHECK_EQ(state_, RTCQuicTransportState::kNew);
  DCHECK_NE(start_reason_, StartReason::kNotStarted);

  state_ = RTCQuicTransportState::kConnecting;
  // We don't create the underlying transports until we are starting
  // to connect.
  Vector<rtc::scoped_refptr<rtc::RTCCertificate>> rtc_certificates;
  for (const auto& certificate : certificates_) {
    rtc_certificates.push_back(certificate->Certificate());
  }
  IceTransportProxy* transport_proxy = transport_->ConnectConsumer(this);
  P2PQuicTransportConfig quic_transport_config(
      perspective, rtc_certificates,
      /*stream_delegate_read_buffer_size_in=*/RTCQuicStream::kReadBufferSize,
      /*stream_write_buffer_size_in=*/RTCQuicStream::kWriteBufferSize);
  proxy_.reset(new QuicTransportProxy(this, transport_proxy,
                                      std::move(p2p_quic_transport_factory_),
                                      quic_transport_config));
  proxy_->Start(std::move(start_config));
}

void RTCQuicTransport::OnIceTransportStarted() {
  // If start() has already been called, we now start up the connection,
  // since start() determines its quic::Perspective based upon ICE.
  if (start_reason_ == StartReason::kP2PWithRemoteFingerprints) {
    DCHECK(remote_parameters_);
    Vector<std::unique_ptr<rtc::SSLFingerprint>> rtc_fingerprints;
    for (const RTCDtlsFingerprint* fingerprint :
         remote_parameters_->fingerprints()) {
      rtc_fingerprints.push_back(
          RTCDtlsFingerprintToSSLFingerprint(fingerprint));
    };
    StartConnection(QuicPerspectiveFromIceRole(transport_->GetRole()),
                    P2PQuicTransport::StartConfig(std::move(rtc_fingerprints)));
  }
}

void RTCQuicTransport::stop() {
  if (IsClosed()) {
    // The transport could have already been closed due to the context being
    // destroyed, the RTCIceTransport closing or a remote/local stop().
    return;
  }
  if (IsDisposed()) {
    // This occurs in the "failed" state.
    state_ = RTCQuicTransportState::kClosed;
    return;
  }
  Close(CloseReason::kLocalStopped);
}

RTCQuicStream* RTCQuicTransport::createStream(ExceptionState& exception_state) {
  // TODO(github.com/w3c/webrtc-quic/issues/50): Maybe support createStream in
  // the 'new' or 'connecting' states.
  if (state_ != RTCQuicTransportState::kConnected) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "RTCQuicTransport.createStream() is only "
                                      "valid in the 'connected' state.");
    return nullptr;
  }
  return AddStream(proxy_->CreateStream());
}

ScriptPromise RTCQuicTransport::readyToSendDatagram(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (ready_to_send_datagram_promise_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Pending readyToSendDatagram promise exists");
    return ScriptPromise();
  }
  if (RaiseExceptionIfNotConnected(exception_state)) {
    return ScriptPromise();
  }

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = promise_resolver->Promise();
  if (CanWriteDatagram()) {
    promise_resolver->Resolve();
    return promise;
  }
  ready_to_send_datagram_promise_ = promise_resolver;
  return promise;
}

void RTCQuicTransport::sendDatagram(const DOMArrayPiece& data,
                                    ExceptionState& exception_state) {
  if (RaiseExceptionIfNotConnected(exception_state)) {
    return;
  }
  if (!CanWriteDatagram()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot send datagram because not readyToSend()");
    return;
  }
  if (data.ByteLength() > max_datagram_length_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "data of size " + String::Number(data.ByteLength()) +
            " is too large to fit into a datagram of max size: " +
            String::Number(max_datagram_length_.value_or(0)));
    return;
  }

  Vector<uint8_t> datagram(data.ByteLength());
  memcpy(datagram.data(), data.Data(), data.ByteLength());
  proxy_->SendDatagram(std::move(datagram));
  num_buffered_sent_datagrams_++;
}

ScriptPromise RTCQuicTransport::receiveDatagrams(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (receive_datagrams_promise_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Pending receiveDatagrams promise exists");
    return ScriptPromise();
  }

  if (RaiseExceptionIfNotConnected(exception_state)) {
    return ScriptPromise();
  }

  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = promise_resolver->Promise();
  if (received_datagrams_.IsEmpty()) {
    receive_datagrams_promise_ = promise_resolver;
    return promise;
  }
  HeapVector<Member<DOMArrayBuffer>> resolved_datagrams;
  resolved_datagrams.swap(received_datagrams_);
  promise_resolver->Resolve(resolved_datagrams);
  return promise;
}

ScriptPromise RTCQuicTransport::getStats(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  // TODO(https://crbug.com/874296): If a shutdown procedure is implemented, we
  // can cache the stats before the underlying transport is torn down. This
  // would allow getting stats after your transport has closed.
  if (state_ != RTCQuicTransportState::kConnected &&
      state_ != RTCQuicTransportState::kConnecting) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport's state is not 'connecting' or 'connected'.");
    return ScriptPromise();
  }
  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  uint32_t request_id = ++get_stats_id_counter_;
  stats_promise_map_.Set(request_id, promise_resolver);
  proxy_->GetStats(request_id);
  return promise_resolver->Promise();
}

RTCQuicStream* RTCQuicTransport::AddStream(QuicStreamProxy* stream_proxy) {
  auto* stream = MakeGarbageCollected<RTCQuicStream>(GetExecutionContext(),
                                                     this, stream_proxy);
  stream_proxy->set_delegate(stream);
  streams_.insert(stream);
  return stream;
}

void RTCQuicTransport::RemoveStream(RTCQuicStream* stream) {
  DCHECK(stream);
  auto it = streams_.find(stream);
  DCHECK(it != streams_.end());
  streams_.erase(it);
}

void RTCQuicTransport::OnConnected(P2PQuicNegotiatedParams negotiated_params) {
  // Datagrams should always be supported between RTCQuicTransport endpoints.
  DCHECK(negotiated_params.datagrams_supported());
  max_datagram_length_ = negotiated_params.max_datagram_length();
  state_ = RTCQuicTransportState::kConnected;
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

void RTCQuicTransport::OnConnectionFailed(const std::string& error_details,
                                          bool from_remote) {
  Close(CloseReason::kFailed);
}

void RTCQuicTransport::OnRemoteStopped() {
  Close(CloseReason::kRemoteStopped);
}

void RTCQuicTransport::OnStream(QuicStreamProxy* stream_proxy) {
  RTCQuicStream* stream = AddStream(stream_proxy);
  DispatchEvent(*MakeGarbageCollected<RTCQuicStreamEvent>(stream));
}

static RTCQuicTransportStats* CreateRTCQuicTransportStats(
    const P2PQuicTransportStats& p2p_stats) {
  RTCQuicTransportStats* rtc_stats = RTCQuicTransportStats::Create();
  rtc_stats->setTimestamp(
      ConvertTimeTicksToDOMHighResTimeStamp(p2p_stats.timestamp));
  rtc_stats->setBytesSent(p2p_stats.bytes_sent);
  rtc_stats->setPacketsSent(p2p_stats.packets_sent);
  rtc_stats->setStreamBytesSent(p2p_stats.stream_bytes_sent);
  rtc_stats->setStreamBytesReceived(p2p_stats.stream_bytes_received);
  rtc_stats->setNumOutgoingStreamsCreated(
      p2p_stats.num_outgoing_streams_created);
  rtc_stats->setNumIncomingStreamsCreated(
      p2p_stats.num_incoming_streams_created);
  rtc_stats->setBytesReceived(p2p_stats.bytes_received);
  rtc_stats->setPacketsReceived(p2p_stats.packets_received);
  rtc_stats->setPacketsProcessed(p2p_stats.packets_processed);
  rtc_stats->setBytesRetransmitted(p2p_stats.bytes_retransmitted);
  rtc_stats->setPacketsRetransmitted(p2p_stats.packets_retransmitted);
  rtc_stats->setPacketsLost(p2p_stats.packets_lost);
  rtc_stats->setPacketsDropped(p2p_stats.packets_dropped);
  rtc_stats->setCryptoRetransmitCount(p2p_stats.crypto_retransmit_count);
  rtc_stats->setMinRttUs(p2p_stats.min_rtt_us);
  rtc_stats->setSmoothedRttUs(p2p_stats.srtt_us);
  rtc_stats->setMaxPacketSize(p2p_stats.max_packet_size);
  rtc_stats->setMaxReceivedPacketSize(p2p_stats.max_received_packet_size);
  rtc_stats->setEstimatedBandwidthBps(p2p_stats.estimated_bandwidth_bps);
  rtc_stats->setPacketsReordered(p2p_stats.packets_reordered);
  rtc_stats->setBlockedFramesReceived(p2p_stats.blocked_frames_received);
  rtc_stats->setBlockedFramesSent(p2p_stats.blocked_frames_sent);
  rtc_stats->setConnectivityProbingPacketsReceived(
      p2p_stats.connectivity_probing_packets_received);
  rtc_stats->setNumDatagramsLost(p2p_stats.num_datagrams_lost);
  return rtc_stats;
}

void RTCQuicTransport::OnStats(uint32_t request_id,
                               const P2PQuicTransportStats& stats) {
  auto it = stats_promise_map_.find(request_id);
  DCHECK(it != stats_promise_map_.end());
  RTCQuicTransportStats* rtc_stats = CreateRTCQuicTransportStats(stats);
  rtc_stats->setNumReceivedDatagramsDropped(num_dropped_received_datagrams_);
  it->value->Resolve(rtc_stats);
  stats_promise_map_.erase(it);
}

void RTCQuicTransport::OnDatagramSent() {
  num_buffered_sent_datagrams_--;
  // There may be a pending readyToSend promise that can now be resolved.
  if (ready_to_send_datagram_promise_) {
    ready_to_send_datagram_promise_->Resolve();
    ready_to_send_datagram_promise_.Clear();
  }
  DCHECK_GE(num_buffered_sent_datagrams_, 0);
}

void RTCQuicTransport::OnDatagramReceived(Vector<uint8_t> datagram) {
  DOMArrayBuffer* copied_datagram = DOMArrayBuffer::Create(
      static_cast<void*>(datagram.data()), datagram.size());
  if (receive_datagrams_promise_) {
    // We have an pending promise to resolve with received datagrams.
    HeapVector<Member<DOMArrayBuffer>> received_datagrams;
    received_datagrams.push_back(copied_datagram);
    receive_datagrams_promise_->Resolve(received_datagrams);
    receive_datagrams_promise_.Clear();
    return;
  }
  if (received_datagrams_.size() == kMaxBufferedRecvDatagrams) {
    num_dropped_received_datagrams_++;
    return;
  }
  received_datagrams_.push_back(copied_datagram);
}

void RTCQuicTransport::OnIceTransportClosed(
    RTCIceTransport::CloseReason reason) {
  if (reason == RTCIceTransport::CloseReason::kContextDestroyed) {
    Close(CloseReason::kContextDestroyed);
  } else {
    Close(CloseReason::kIceTransportClosed);
  }
}

void RTCQuicTransport::Close(CloseReason reason) {
  DCHECK(!IsDisposed());

  // Disconnect from the RTCIceTransport, allowing a new RTCQuicTransport to
  // connect to it.
  transport_->DisconnectConsumer(this);

  // Notify the active streams that the transport is closing.
  for (RTCQuicStream* stream : streams_) {
    stream->OnQuicTransportClosed(reason);
  }
  streams_.clear();

  // Tear down the QuicTransportProxy and change the state.
  switch (reason) {
    case CloseReason::kLocalStopped:
    case CloseReason::kIceTransportClosed:
    case CloseReason::kContextDestroyed:
      // The QuicTransportProxy may be active so gracefully Stop() before
      // destroying it.
      if (proxy_) {
        proxy_->Stop();
        proxy_.reset();
      }
      state_ = RTCQuicTransportState::kClosed;
      break;
    case CloseReason::kRemoteStopped:
    case CloseReason::kFailed:
      // The QuicTransportProxy has already been closed by the event, so just
      // go ahead and delete it.
      proxy_.reset();
      state_ =
          (reason == CloseReason::kFailed ? RTCQuicTransportState::kFailed
                                          : RTCQuicTransportState::kClosed);
      DispatchEvent(*Event::Create(event_type_names::kStatechange));
      break;
  }
  received_datagrams_.clear();

  if (reason != CloseReason::kContextDestroyed) {
    // Cannot reject/resolve promises when ExecutionContext is being destroyed.
    RejectPendingPromises();
  }

  DCHECK(!proxy_);
  DCHECK(IsDisposed());
}

bool RTCQuicTransport::RaiseExceptionIfClosed(
    ExceptionState& exception_state) const {
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport's state is 'closed'.");
    return true;
  }
  return false;
}

bool RTCQuicTransport::RaiseExceptionIfNotConnected(
    ExceptionState& exception_state) const {
  if (state_ != RTCQuicTransportState::kConnected) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "RTCQuicTransport is not in the 'connected' state.");
    return true;
  }
  return false;
}

bool RTCQuicTransport::RaiseExceptionIfStarted(
    ExceptionState& exception_state) const {
  if (start_reason_ == StartReason::kServerListening) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport has already called listen().");
    return true;
  }
  if (start_reason_ == StartReason::kClientConnecting) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport has already called connect().");
    return true;
  }
  if (start_reason_ == StartReason::kP2PWithRemoteFingerprints) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCQuicTransport has already called start().");
    return true;
  }
  return false;
}

void RTCQuicTransport::RejectPendingPromises() {
  for (ScriptPromiseResolver* promise_resolver : stats_promise_map_.Values()) {
    RejectPromise(promise_resolver, "getStats");
  }
  stats_promise_map_.clear();
  if (ready_to_send_datagram_promise_) {
    RejectPromise(ready_to_send_datagram_promise_, "readyToSendDatagram");
    ready_to_send_datagram_promise_.Clear();
  }
  if (receive_datagrams_promise_) {
    RejectPromise(receive_datagrams_promise_, "receiveDatagrams");
    receive_datagrams_promise_.Clear();
  }
}

const AtomicString& RTCQuicTransport::InterfaceName() const {
  return event_target_names::kRTCQuicTransport;
}

ExecutionContext* RTCQuicTransport::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCQuicTransport::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  visitor->Trace(certificates_);
  visitor->Trace(remote_certificates_);
  visitor->Trace(remote_parameters_);
  visitor->Trace(streams_);
  visitor->Trace(key_);
  visitor->Trace(stats_promise_map_);
  visitor->Trace(receive_datagrams_promise_);
  visitor->Trace(ready_to_send_datagram_promise_);
  visitor->Trace(received_datagrams_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
