// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_candidate_pair.h"
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate_pair.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_parameters.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {

class ExceptionState;
class RTCIceCandidate;
class RTCIceGatherOptions;
class IceTransportAdapterCrossThreadFactory;
class RTCQuicTransport;

enum class RTCIceTransportState {
  kNew,
  kChecking,
  kConnected,
  kCompleted,
  kDisconnected,
  kFailed,
  kClosed
};

// Blink bindings for the RTCIceTransport JavaScript object.
//
// This class uses the IceTransportProxy to run and interact with the WebRTC
// ICE implementation running on the WebRTC worker thread managed by //content
// (called network_thread here).
//
// This object inherits from ActiveScriptWrappable since it must be kept alive
// while the ICE implementation is active, regardless of the number of
// JavaScript references held to it.
class MODULES_EXPORT RTCIceTransport final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<RTCIceTransport>,
      public ContextLifecycleObserver,
      public IceTransportProxy::Delegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCIceTransport);

 public:
  static RTCIceTransport* Create(ExecutionContext* context);
  static RTCIceTransport* Create(
      ExecutionContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
      scoped_refptr<base::SingleThreadTaskRunner> host_thread,
      std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory);

  ~RTCIceTransport() override;

  // Returns true if start() has been called.
  bool IsStarted() const { return role_ != cricket::ICEROLE_UNKNOWN; }

  // Returns the role specified in start().
  cricket::IceRole GetRole() const { return role_; }

  // Returns true if the RTCIceTransport is in a terminal state.
  bool IsClosed() const { return state_ == RTCIceTransportState::kClosed; }

  // An RTCQuicTransport can be connected to this RTCIceTransport. Only one can
  // be connected at a time. The consumer will be automatically disconnected
  // if stop() is called on this object. Otherwise, the RTCQuicTransport is
  // responsible for disconnecting itself when it is done.
  // ConnectConsumer returns an IceTransportProxy that can be used to connect
  // a QuicTransportProxy. It may be called repeatedly with the same
  // RTCQuicTransport.
  bool HasConsumer() const;
  IceTransportProxy* ConnectConsumer(RTCQuicTransport* consumer);
  void DisconnectConsumer(RTCQuicTransport* consumer);

  // rtc_ice_transport.idl
  String role() const;
  String state() const;
  String gatheringState() const;
  const HeapVector<Member<RTCIceCandidate>>& getLocalCandidates() const;
  const HeapVector<Member<RTCIceCandidate>>& getRemoteCandidates() const;
  void getSelectedCandidatePair(
      base::Optional<RTCIceCandidatePair>& result) const;
  void getLocalParameters(base::Optional<RTCIceParameters>& result) const;
  void getRemoteParameters(base::Optional<RTCIceParameters>& result) const;
  void gather(const RTCIceGatherOptions& options,
              ExceptionState& exception_state);
  void start(const RTCIceParameters& remote_parameters,
             const String& role,
             ExceptionState& exception_state);
  void stop();
  void addRemoteCandidate(RTCIceCandidate* remote_candidate,
                          ExceptionState& exception_state);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(gatheringstatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectedcandidatepairchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);

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
  explicit RTCIceTransport(
      ExecutionContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
      scoped_refptr<base::SingleThreadTaskRunner> host_thread,
      std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory);

  // IceTransportProxy::Delegate overrides.
  void OnGatheringStateChanged(cricket::IceGatheringState new_state) override;
  void OnCandidateGathered(const cricket::Candidate& candidate) override;
  void OnStateChanged(cricket::IceTransportState new_state) override;
  void OnSelectedCandidatePairChanged(
      const std::pair<cricket::Candidate, cricket::Candidate>&
          selected_candidate_pair) override;

  // Fills in |local_parameters_| with a random usernameFragment and a random
  // password.
  void GenerateLocalParameters();

  bool RaiseExceptionIfClosed(ExceptionState& exception_state) const;

  cricket::IceRole role_ = cricket::ICEROLE_UNKNOWN;
  RTCIceTransportState state_ = RTCIceTransportState::kNew;
  cricket::IceGatheringState gathering_state_ = cricket::kIceGatheringNew;

  HeapVector<Member<RTCIceCandidate>> local_candidates_;
  HeapVector<Member<RTCIceCandidate>> remote_candidates_;

  RTCIceParameters local_parameters_;
  base::Optional<RTCIceParameters> remote_parameters_;

  base::Optional<RTCIceCandidatePair> selected_candidate_pair_;

  Member<RTCQuicTransport> consumer_;

  // Handle to the WebRTC ICE transport. Created when this binding is
  // constructed and deleted once network traffic should be stopped.
  std::unique_ptr<IceTransportProxy> proxy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_H_
