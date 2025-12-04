// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport.h"

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_config.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/array_buffer_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/datagram_connection_factory.h"
#include "third_party/webrtc/pc/ice_server_parsing.h"
#include "third_party/webrtc/rtc_base/rtc_certificate_generator.h"

namespace blink {
namespace {
using PacketSendParameters = webrtc::DatagramConnection::PacketSendParameters;

// Maximum known size (SHA-512). See
// third_party/webrtc/rtc_base/message_digest.h
const size_t kMaxDigestSize = 64;

class DatagramConnectionObserver : public webrtc::DatagramConnection::Observer {
 public:
  DatagramConnectionObserver(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      CrossThreadWeakHandle<RtcTransport> transport)
      : main_task_runner_(main_task_runner), transport_(transport) {}
  ~DatagramConnectionObserver() override = default;

  void OnCandidateGathered(const webrtc::Candidate& candidate) override {
    webrtc::Candidate candidate_copy = candidate;
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RtcTransport::OnCandidateGatheredOnMainThread,
                            MakeUnwrappingCrossThreadWeakHandle(transport_),
                            candidate_copy));
  }

  void OnPacketReceived(webrtc::ArrayView<const uint8_t> data,
                        PacketMetadata metadata) override {
    Vector<uint8_t> data_vec(data);
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RtcTransport::OnPacketReceivedOnMainThread,
                            MakeUnwrappingCrossThreadWeakHandle(transport_),
                            std::move(data_vec), metadata.receive_time));
  }

  // TODO(crbug.com/443019066): Hook up this with JS events once the API design
  // includes them.
  void OnSendError() override {}
  void OnConnectionError() override {}
  void OnWritableChange() override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RtcTransport::OnWritableChangeOnMainThread,
                            MakeUnwrappingCrossThreadWeakHandle(transport_)));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  CrossThreadWeakHandle<RtcTransport> transport_;
};

V8RTCIceCandidateType IceCandidateTypeFrom(webrtc::IceCandidateType type) {
  switch (type) {
    case webrtc::IceCandidateType::kHost:
      return V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kHost);
    case webrtc::IceCandidateType::kSrflx:
      return V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kSrflx);
    case webrtc::IceCandidateType::kPrflx:
      return V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kPrflx);
    case webrtc::IceCandidateType::kRelay:
      return V8RTCIceCandidateType(V8RTCIceCandidateType::Enum::kRelay);
  }
}

webrtc::IceCandidateType WebrtcIceCandidateTypeFromIdl(
    V8RTCIceCandidateType type) {
  switch (type.AsEnum()) {
    case V8RTCIceCandidateType::Enum::kHost:
      return webrtc::IceCandidateType::kHost;
    case V8RTCIceCandidateType::Enum::kSrflx:
      return webrtc::IceCandidateType::kSrflx;
    case V8RTCIceCandidateType::Enum::kPrflx:
      return webrtc::IceCandidateType::kPrflx;
    case V8RTCIceCandidateType::Enum::kRelay:
      return webrtc::IceCandidateType::kRelay;
  }
}

class AsyncDatagramConnectionImpl : public AsyncDatagramConnection {
 public:
  AsyncDatagramConnectionImpl(
      webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection,
      scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner)
      : datagram_connection_(datagram_connection),
        js_thread_task_runner_(js_thread_task_runner) {}

  ~AsyncDatagramConnectionImpl() override = default;

  void AddRemoteCandidate(const webrtc::Candidate& candidate) override {
    PostCrossThreadTask(
        *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                   datagram_connection,
               const webrtc::Candidate& candidate) {
              // Set IceParameters to add passwords to any peer reflexive
              // candidates we have already received.
              datagram_connection->SetRemoteIceParameters(webrtc::IceParameters(
                  candidate.username(), candidate.password(),
                  /*ice_renomination=*/true));
              datagram_connection->AddRemoteCandidate(candidate);
            },
            datagram_connection_, candidate));
  }

  void Writable(ScriptPromiseResolver<IDLBoolean>* resolver) override {
    CHECK(js_thread_task_runner_->RunsTasksInCurrentSequence());
    PostCrossThreadTask(
        *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                   datagram_connection,
               CrossThreadHandle<ScriptPromiseResolver<IDLBoolean>> resolver,
               scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner) {
              bool writable = datagram_connection->Writable();

              PostCrossThreadTask(
                  *js_thread_task_runner, FROM_HERE,
                  CrossThreadBindOnce(
                      [](ScriptPromiseResolver<IDLBoolean>* resolver,
                         bool writable) { resolver->Resolve(writable); },
                      MakeUnwrappingCrossThreadHandle(resolver), writable));
            },
            datagram_connection_, MakeCrossThreadHandle(resolver),
            js_thread_task_runner_));
  }

  void SetRemoteDtlsParameters(
      String digestAlgorithm,
      Vector<uint8_t> fingerprint,
      webrtc::DatagramConnection::SSLRole ssl_role) override {
    PostCrossThreadTask(
        *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                   datagram_connection,
               std::string digestAlgorithm, Vector<uint8_t> fingerprint,
               webrtc::DatagramConnection::SSLRole ssl_role) {
              datagram_connection->SetRemoteDtlsParameters(
                  digestAlgorithm, fingerprint.data(), fingerprint.size(),
                  ssl_role);
            },
            datagram_connection_, digestAlgorithm.Utf8(),
            std::move(fingerprint), ssl_role));
  }

  void SendPackets(
      std::unique_ptr<Vector<Vector<uint8_t>>> packet_payloads) override {
    PostCrossThreadTask(
        *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                   datagram_connection,
               std::unique_ptr<Vector<Vector<uint8_t>>> packet_payloads) {
              std::vector<PacketSendParameters> send_params;
              send_params.reserve(packet_payloads->size());
              for (const Vector<uint8_t>& payload : *packet_payloads) {
                send_params.push_back(PacketSendParameters{.payload = payload});
              }
              datagram_connection->SendPackets(send_params);
            },
            datagram_connection_, std::move(packet_payloads)));
  }

  void Terminate() override {
    // Post the webrtc objects to the network thread for destruction there.
    PostCrossThreadTask(*RtcTransportDependencies::NetworkTaskRunner(),
                        FROM_HERE,
                        CrossThreadBindOnce(
                            [](webrtc::scoped_refptr<webrtc::DatagramConnection>
                                   datagram_connection) {
                              datagram_connection->Terminate([] {});
                            },
                            std::move(datagram_connection_)));
  }

  webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection_;
  scoped_refptr<base::SequencedTaskRunner> js_thread_task_runner_;
};

webrtc::DatagramConnection::WireProtocol ToWebrtcWireProtocol(
    V8RtcTransportWireProtocol wire_protocol) {
  switch (wire_protocol.AsEnum()) {
    case V8RtcTransportWireProtocol::Enum::kDtls:
      return webrtc::DatagramConnection::WireProtocol::kDtls;
    case V8RtcTransportWireProtocol::Enum::kDtlsSrtp:
      return webrtc::DatagramConnection::WireProtocol::kDtlsSrtp;
  }
}

}  // namespace

// static
RtcTransport* RtcTransport::Create(ExecutionContext* context,
                                   const RtcTransportConfig* config,
                                   ExceptionState& exception_state) {
  auto* transport = MakeGarbageCollected<RtcTransport>(PassKey(), context);

  std::unique_ptr<StunAndTurnServers> stun_and_turn_servers =
      ParseStunServers(config, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  if (!config->hasIceControlling()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing iceControlling");
    return nullptr;
  }
  if (!config->hasWireProtocol()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing wireProtocol");
    return nullptr;
  }

  RtcTransportDependencies::GetInitialized(
      *context,
      BindOnce(&RtcTransport::ContinueInitialization, WrapPersistent(transport),
               config->iceControlling(), std::move(stun_and_turn_servers),
               /*injected_datagram_connection=*/nullptr,
               ToWebrtcWireProtocol(config->wireProtocol())));
  return transport;
}

// static
RtcTransport* RtcTransport::CreateForTests(
    ExecutionContext* context,
    const RtcTransportConfig* config,
    ExceptionState& exception_state,
    std::unique_ptr<AsyncDatagramConnection> async_datagram_connection,
    webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection) {
  auto* transport = MakeGarbageCollected<RtcTransport>(PassKey(), context);

  std::unique_ptr<StunAndTurnServers> stun_and_turn_servers =
      ParseStunServers(config, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (async_datagram_connection) {
    transport->OnInitialized(std::move(async_datagram_connection));
  }
  if (datagram_connection) {
    RtcTransportDependencies::GetInitialized(
        *context,
        BindOnce(&RtcTransport::ContinueInitialization,
                 WrapPersistent(transport), config->iceControlling(),
                 std::move(stun_and_turn_servers), datagram_connection,
                 ToWebrtcWireProtocol(config->wireProtocol())));
  }
  return transport;
}

// static
std::unique_ptr<RtcTransport::StunAndTurnServers>
RtcTransport::ParseStunServers(const RtcTransportConfig* config,
                               ExceptionState& exception_state) {
  auto stun_and_turn_servers = std::make_unique<StunAndTurnServers>();
  if (!config->hasIceServers()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Missing iceServers");
    return stun_and_turn_servers;
  }

  webrtc::PeerConnectionInterface::IceServers ice_servers;
  for (const RTCIceServer* ice_server : config->iceServers()) {
    if (ice_server->hasUrl()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "'url' field not supported, use 'urls'");
      return stun_and_turn_servers;
    }
    if (!ice_server->hasUrls()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "Ice server must have 'urls'");
      return stun_and_turn_servers;
    }

    webrtc::PeerConnectionInterface::IceServer server;
    switch (ice_server->urls()->GetContentType()) {
      case V8UnionStringOrStringSequence::ContentType::kString:
        server.urls.push_back(ice_server->urls()->GetAsString().Utf8());
        break;
      case V8UnionStringOrStringSequence::ContentType::kStringSequence:
        for (const String& url : ice_server->urls()->GetAsStringSequence()) {
          server.urls.push_back(url.Utf8());
        }
    }
    if (ice_server->hasUsername()) {
      server.username = ice_server->username().Utf8();
    }
    if (ice_server->hasCredential()) {
      server.password = ice_server->credential().Utf8();
    }
    ice_servers.push_back(server);
  }
  webrtc::RTCError error = webrtc::ParseIceServersOrError(
      ice_servers, &stun_and_turn_servers->stun_servers,
      &stun_and_turn_servers->turn_servers);
  if (!error.ok()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Failed to parse ice servers");
  }

  return stun_and_turn_servers;
}

RtcTransport::RtcTransport(PassKey, ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      task_runner_(context->GetTaskRunner(TaskType::kNetworking)),
      digest_(0, kMaxDigestSize) {
  // Should this be done async? cf
  // RTCCertificateGenerator::GenerateCertificateAsync.
  certificate_ = webrtc::RTCCertificateGenerator::GenerateCertificate(
      webrtc::KeyParams::ECDSA(), /*expires_ms=*/std::nullopt);

  std::string digestAlgorithm;
  certificate_->GetSSLCertificate().GetSignatureDigestAlgorithm(
      &digestAlgorithm);
  fingerprintDigestAlgorithm_ = String(digestAlgorithm);
  certificate_->GetSSLCertificate().ComputeDigest(digestAlgorithm, digest_);
}

void RtcTransport::ContinueInitialization(
    bool ice_controlling,
    std::unique_ptr<StunAndTurnServers> stun_and_turn_servers,
    webrtc::scoped_refptr<webrtc::DatagramConnection>
        injected_datagram_connection,
    webrtc::DatagramConnection::WireProtocol wire_protocol,
    RtcTransportDependencies* dependencies) {
  std::unique_ptr<P2PPortAllocator> port_allocator =
      dependencies->CreatePortAllocator();
  port_allocator->SetConfiguration(stun_and_turn_servers->stun_servers,
                                   stun_and_turn_servers->turn_servers, 0,
                                   webrtc::PortPrunePolicy::NO_PRUNE);

  auto observer = std::make_unique<DatagramConnectionObserver>(
      task_runner_, MakeCrossThreadWeakHandle(this));
  PostCrossThreadTask(
      *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadHandle<RtcTransport> transport,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             std::unique_ptr<P2PPortAllocator> port_allocator,
             bool ice_controlling, const webrtc::Environment& env,
             webrtc::scoped_refptr<webrtc::RTCCertificate> certificate,
             std::unique_ptr<webrtc::DatagramConnection::Observer> observer,
             webrtc::scoped_refptr<webrtc::DatagramConnection>
                 injected_datagram_connection,
             webrtc::DatagramConnection::WireProtocol wire_protocol) {
            port_allocator->Initialize();

            webrtc::scoped_refptr<webrtc::DatagramConnection>
                datagram_connection =
                    injected_datagram_connection
                        ? injected_datagram_connection
                        : webrtc::CreateDatagramConnection(
                              env, std::move(port_allocator),
                              /*name=*/"RtcTransport", ice_controlling,
                              std::move(certificate), std::move(observer),
                              wire_protocol);

            auto async_datagram_connection =
                std::make_unique<AsyncDatagramConnectionImpl>(
                    std::move(datagram_connection), task_runner);
            PostCrossThreadTask(
                *task_runner, FROM_HERE,
                CrossThreadBindOnce(&RtcTransport::OnInitialized,
                                    MakeUnwrappingCrossThreadHandle(transport),
                                    std::move(async_datagram_connection)));
          },
          MakeCrossThreadHandle(this), task_runner_, std::move(port_allocator),
          ice_controlling, dependencies->Environment(), certificate_,
          std::move(observer), std::move(injected_datagram_connection),
          wire_protocol));
}

void RtcTransport::OnInitialized(
    std::unique_ptr<AsyncDatagramConnection> async_datagram_connection) {
  DCHECK(!initialized_);
  initialized_ = true;
  async_datagram_connection_ = std::move(async_datagram_connection);

  if (pending_dtls_parameters_) {
    // Apply pending DTLS parameters
    setRemoteDtlsParameters(pending_dtls_parameters_.Release());
  }

  for (const auto& candidate : pending_remote_candidates_) {
    async_datagram_connection_->AddRemoteCandidate(candidate);
  }
  pending_remote_candidates_.clear();

  if (!pending_send_packets_calls_.empty()) {
    sendPackets(pending_send_packets_calls_);
    pending_send_packets_calls_.clear();
  }
}

RtcTransport::~RtcTransport() = default;

void RtcTransport::OnCandidateGatheredOnMainThread(
    webrtc::Candidate candidate) {
  DispatchEvent(*MakeGarbageCollected<RtcTransportIceEvent>(
      MakeGarbageCollected<RtcTransportIceCandidate>(
          String::FromUTF8(candidate.username()),
          String::FromUTF8(candidate.password()),
          String::FromUTF8(candidate.address().ipaddr().ToString()),
          candidate.address().port(), IceCandidateTypeFrom(candidate.type()))));
}

void RtcTransport::OnPacketReceivedOnMainThread(
    Vector<uint8_t> data,
    webrtc::Timestamp receive_time) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    // Context has gone away - eg page has been closed. Just bail.
    return;
  }
  received_packets_.push_back(MakeGarbageCollected<RtcReceivedPacket>(
      std::move(data), RTCTimeStampFromTimeTicks(
                           context, ConvertToBaseTimeTicks(receive_time))));
}

void RtcTransport::addRemoteCandidate(RtcTransportICECandidateInit* init,
                                      ExceptionState& exception_state) {
  webrtc::Candidate candidate;
  // Only UDP candidates supported.
  candidate.set_protocol("udp");

  webrtc::SocketAddress address =
      webrtc::SocketAddress(init->address().Utf8(), init->port());
  if (address.ipaddr().IsNil()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Invalid address");
    return;
  }

  candidate.set_address(address);
  candidate.set_username(init->usernameFragment().Utf8());
  candidate.set_password(init->password().Utf8());
  candidate.set_type(WebrtcIceCandidateTypeFromIdl(init->type()));

  if (!initialized_) {
    pending_remote_candidates_.push_back(candidate);
    return;
  }

  async_datagram_connection_->AddRemoteCandidate(candidate);
}

HeapVector<Member<RtcReceivedPacket>> RtcTransport::getReceivedPackets() {
  HeapVector<Member<RtcReceivedPacket>> packets;
  std::swap(packets, received_packets_);
  return packets;
}

void RtcTransport::sendPackets(
    HeapVector<Member<RtcSendPacketParameters>> packets) {
  if (!initialized_) {
    pending_send_packets_calls_.AppendVector(packets);
    return;
  }
  auto packet_payloads = std::make_unique<Vector<Vector<uint8_t>>>();
  packet_payloads->reserve(packets.size());
  for (const auto& packet : packets) {
    // Copy from the data buffer source into a new Vector.
    packet_payloads->emplace_back(
        RtcTransportBufferSourceAsByteSpan(*packet->data()));
  }

  async_datagram_connection_->SendPackets(std::move(packet_payloads));
}

void RtcTransport::setRemoteDtlsParameters(RtcDtlsParameters* parameters) {
  if (!initialized_) {
    // Store parameters for later application.
    pending_dtls_parameters_ = parameters;
    return;
  }

  async_datagram_connection_->SetRemoteDtlsParameters(
      parameters->fingerprintDigestAlgorithm(),
      Vector<uint8_t>(parameters->fingerprint()->ByteSpan()),
      parameters->sslRole() == V8RtcTransportSslRole::Enum::kServer
          ? webrtc::DatagramConnection::SSLRole::kServer
          : webrtc::DatagramConnection::SSLRole::kClient);
}

ScriptPromise<IDLBoolean> RtcTransport::writable(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);

  if (initialized_) {
    async_datagram_connection_->Writable(resolver);
  } else {
    resolver->Resolve(false);
  }

  return resolver->Promise();
}

void RtcTransport::OnWritableChangeOnMainThread() {
  DispatchEvent(*Event::Create(event_type_names::kWritablechange));
}

void RtcTransport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(received_packets_);
  visitor->Trace(pending_send_packets_calls_);
  visitor->Trace(pending_dtls_parameters_);
}

void RtcTransport::Dispose() {
  if (initialized_) {
    async_datagram_connection_->Terminate();
  }
}

}  // namespace blink
