// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_dtls_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_send_packet_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_ice_candidate_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_received_packet.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/api/datagram_connection.h"

namespace blink {

// Async adapter of webrtc::DatagramConnection to allow mocking out threading in
// unittests.
class AsyncDatagramConnection {
 public:
  virtual ~AsyncDatagramConnection() = default;
  virtual void AddRemoteCandidate(const webrtc::Candidate& candidate) = 0;

  virtual void Writable(ScriptPromiseResolver<IDLBoolean>* resolver) = 0;

  virtual void SetRemoteDtlsParameters(
      String digestAlgorithm,
      Vector<uint8_t> fingerprint,
      webrtc::DatagramConnection::SSLRole ssl_role) = 0;

  virtual void SendPackets(
      std::unique_ptr<Vector<Vector<uint8_t>>> packet_payloads) = 0;

  virtual void Terminate() = 0;
};

class MODULES_EXPORT RtcTransport final
    : public EventTarget,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(RtcTransport, Dispose);

 public:
  using PassKey = base::PassKey<RtcTransport>;
  static RtcTransport* Create(ExecutionContext* context,
                              const RtcTransportConfig* config,
                              ExceptionState& exception_state);
  static RtcTransport* CreateForTests(
      ExecutionContext* context,
      const RtcTransportConfig* config,
      ExceptionState& exception_state,
      std::unique_ptr<AsyncDatagramConnection> async_datagram_connection =
          nullptr,
      webrtc::scoped_refptr<webrtc::DatagramConnection> datagram_connection =
          nullptr);

  RtcTransport(PassKey, ExecutionContext* context);
  ~RtcTransport() override;

  // ExecutionContextLifecycleObserver implementation
  void ContextDestroyed() final {}

  // EventTarget
  const AtomicString& InterfaceName() const override {
    return event_target_names::kRTCTransport;
  }

  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }

  // IDL implementation
  void addRemoteCandidate(RtcTransportICECandidateInit* init,
                          ExceptionState& exception_state);

  HeapVector<Member<RtcReceivedPacket>> getReceivedPackets();

  void sendPackets(HeapVector<Member<RtcSendPacketParameters>> packets);

  ScriptPromise<IDLBoolean> writable(ScriptState* script_state);

  void setRemoteDtlsParameters(RtcDtlsParameters* parameters);

  String fingerprintDigestAlgorithm() { return fingerprintDigestAlgorithm_; }

  DOMArrayBuffer* fingerprint() { return DOMArrayBuffer::Create(digest_); }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate, kIcecandidate)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(writablechange, kWritablechange)

  void OnPacketReceivedOnMainThread(Vector<uint8_t> data,
                                    webrtc::Timestamp receive_time);
  void OnCandidateGatheredOnMainThread(webrtc::Candidate candidate);
  void OnWritableChangeOnMainThread();

  // ScriptWrappable implementation
  void Trace(Visitor* visitor) const override;

 private:
  friend class RtcTransportTest;

  struct StunAndTurnServers {
    webrtc::ServerAddresses stun_servers;
    std::vector<webrtc::RelayServerConfig> turn_servers
        ALLOW_DISCOURAGED_TYPE("Interacting with webrtc library APIs");
  };
  static std::unique_ptr<StunAndTurnServers> ParseStunServers(
      const RtcTransportConfig* config,
      ExceptionState& exception_state);
  void ContinueInitialization(
      bool ice_controlling,
      std::unique_ptr<StunAndTurnServers> stun_and_turn_servers,
      webrtc::scoped_refptr<webrtc::DatagramConnection>
          injected_datagram_connection,
      webrtc::DatagramConnection::WireProtocol wire_protocol,
      RtcTransportDependencies* dependencies);
  void OnInitialized(std::unique_ptr<AsyncDatagramConnection>);

  // Pre-finalizer
  void Dispose();

  bool initialized_ = false;
  std::unique_ptr<AsyncDatagramConnection> async_datagram_connection_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapVector<Member<RtcReceivedPacket>> received_packets_;
  webrtc::scoped_refptr<webrtc::RTCCertificate> certificate_;
  String fingerprintDigestAlgorithm_;
  webrtc::Buffer digest_;

  // State related to calls which happen before initialization is complete.
  Vector<webrtc::Candidate> pending_remote_candidates_;
  HeapVector<Member<RtcSendPacketParameters>> pending_send_packets_calls_;
  Member<RtcDtlsParameters> pending_dtls_parameters_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_
