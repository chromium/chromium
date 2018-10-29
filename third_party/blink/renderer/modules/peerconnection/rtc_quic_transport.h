// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_parameters.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class RTCCertificate;
class RTCIceTransport;
class RTCQuicStream;
class P2PQuicTransportFactory;

enum class RTCQuicTransportState {
  kNew,
  kConnecting,
  kConnected,
  kClosed,
  kFailed
};

class MODULES_EXPORT RTCQuicTransport final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<RTCQuicTransport>,
      public ContextLifecycleObserver,
      public QuicTransportProxy::Delegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCQuicTransport);

 public:
  static RTCQuicTransport* Create(
      ExecutionContext* context,
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state);
  static RTCQuicTransport* Create(
      ExecutionContext* context,
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state,
      std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory);

  ~RTCQuicTransport() override;

  RTCQuicStream* AddStream(QuicStreamProxy* stream_proxy);
  void RemoveStream(RTCQuicStream* stream);

  // https://w3c.github.io/webrtc-quic/#quic-transport*
  RTCIceTransport* transport() const;
  String state() const;
  void getLocalParameters(RTCQuicParameters& result) const;
  void getRemoteParameters(base::Optional<RTCQuicParameters>& result) const;
  const HeapVector<Member<RTCCertificate>>& getCertificates() const;
  const HeapVector<Member<DOMArrayBuffer>>& getRemoteCertificates() const;
  void start(const RTCQuicParameters& remote_parameters,
             ExceptionState& exception_state);
  void stop();
  RTCQuicStream* createStream(ExceptionState& exception_state);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(quicstream);

  // Called by the RTCIceTransport when its start() method is called.
  void OnTransportStarted();

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;

  // For garbage collection.
  void Trace(blink::Visitor* visitor) override;

 private:
  RTCQuicTransport(
      ExecutionContext* context,
      RTCIceTransport* transport,
      const HeapVector<Member<RTCCertificate>>& certificates,
      ExceptionState& exception_state,
      std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory);

  // QuicTransportProxy::Delegate overrides;
  void OnConnected() override;
  void OnConnectionFailed(const std::string& error_details,
                          bool from_remote) override;
  void OnRemoteStopped() override;
  void OnStream(QuicStreamProxy* stream_proxy) override;

  bool IsClosed() const { return state_ == RTCQuicTransportState::kClosed; }
  bool RaiseExceptionIfClosed(ExceptionState& exception_state) const;

  // Starts the underlying QUIC connection.
  void StartConnection();

  // Close all streams, delete the underlying QUIC transport, and transition to
  // the given state, closed or failed.
  void Close(RTCQuicTransportState new_state);

  Member<RTCIceTransport> transport_;
  RTCQuicTransportState state_ = RTCQuicTransportState::kNew;
  HeapVector<Member<RTCCertificate>> certificates_;
  HeapVector<Member<DOMArrayBuffer>> remote_certificates_;
  base::Optional<RTCQuicParameters> remote_parameters_;
  std::unique_ptr<P2PQuicTransportFactory> p2p_quic_transport_factory_;
  std::unique_ptr<QuicTransportProxy> proxy_;
  HeapHashSet<Member<RTCQuicStream>> streams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_TRANSPORT_H_
