// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

#include <string>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_ice_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/ice_transport_factory.h"
#include "third_party/webrtc/api/ice_transport_interface.h"
#include "third_party/webrtc/api/jsep_ice_candidate.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/p2p/base/transport_description.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

namespace blink {
namespace {

const char* kIceRoleControllingStr = "controlling";
const char* kIceRoleControlledStr = "controlled";

RTCIceCandidate* ConvertToRtcIceCandidate(const cricket::Candidate& candidate) {
  std::string url = candidate.url();
  std::optional<String> optional_url;
  if (!url.empty()) {
    optional_url = String(url);
  }
  // The "" mid and sdpMLineIndex 0 are wrong, see https://crbug.com/1385446
  return RTCIceCandidate::Create(MakeGarbageCollected<RTCIceCandidatePlatform>(
      String::FromUTF8(webrtc::SdpSerializeCandidate(candidate)), "", 0,
      String(candidate.username()), optional_url));
}

class DtlsIceTransportAdapterCrossThreadFactory
    : public IceTransportAdapterCrossThreadFactory {
 public:
  explicit DtlsIceTransportAdapterCrossThreadFactory(
      rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport)
      : ice_transport_(ice_transport) {}
  void InitializeOnMainThread(LocalFrame& frame) override {
  }

  std::unique_ptr<IceTransportAdapter> ConstructOnWorkerThread(
      IceTransportAdapter::Delegate* delegate) override {
    DCHECK(ice_transport_);
    return std::make_unique<IceTransportAdapterImpl>(delegate,
                                                     std::move(ice_transport_));
  }

 private:
  rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport_;
};

}  // namespace

RTCIceTransport* RTCIceTransport::Create(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::IceTransportInterface> ice_transport,
    RTCPeerConnection* peer_connection) {
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      context->GetTaskRunner(TaskType::kNetworking);

  PeerConnectionDependencyFactory::From(*context).EnsureInitialized();
  scoped_refptr<base::SingleThreadTaskRunner> host_thread =
      PeerConnectionDependencyFactory::From(*context)
          .GetWebRtcNetworkTaskRunner();
  return MakeGarbageCollected<RTCIceTransport>(
      context, std::move(proxy_thread), std::move(host_thread),
      std::make_unique<DtlsIceTransportAdapterCrossThreadFactory>(
          std::move(ice_transport)),
      peer_connection);
}

RTCIceTransport::RTCIceTransport(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory,
    RTCPeerConnection* peer_connection)
    : ActiveScriptWrappable<RTCIceTransport>({}),
      ExecutionContextLifecycleObserver(context),
      peer_connection_(peer_connection) {
  DCHECK(context);
  DCHECK(proxy_thread);
  DCHECK(host_thread);
  DCHECK(adapter_factory);
  DCHECK(proxy_thread->BelongsToCurrentThread());

  LocalFrame* frame = To<LocalDOMWindow>(context)->GetFrame();
  DCHECK(frame);
  proxy_ = std::make_unique<IceTransportProxy>(*frame, std::move(proxy_thread),
                                               std::move(host_thread), this,
                                               std::move(adapter_factory));
}

RTCIceTransport::~RTCIceTransport() {
  DCHECK(!proxy_);
}

String RTCIceTransport::role() const {
  switch (role_) {
    case cricket::ICEROLE_CONTROLLING:
      return kIceRoleControllingStr;
    case cricket::ICEROLE_CONTROLLED:
      return kIceRoleControlledStr;
    case cricket::ICEROLE_UNKNOWN:
      return String();
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

String RTCIceTransport::state() const {
  switch (state_) {
    case webrtc::IceTransportState::kNew:
      return "new";
    case webrtc::IceTransportState::kChecking:
      return "checking";
    case webrtc::IceTransportState::kConnected:
      return "connected";
    case webrtc::IceTransportState::kCompleted:
      return "completed";
    case webrtc::IceTransportState::kDisconnected:
      return "disconnected";
    case webrtc::IceTransportState::kFailed:
      return "failed";
    case webrtc::IceTransportState::kClosed:
      return "closed";
  }
  NOTREACHED_IN_MIGRATION();
  return g_empty_string;
}

String RTCIceTransport::gatheringState() const {
  switch (gathering_state_) {
    case cricket::kIceGatheringNew:
      return "new";
    case cricket::kIceGatheringGathering:
      return "gathering";
    case cricket::kIceGatheringComplete:
      return "complete";
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_string;
  }
}

const HeapVector<Member<RTCIceCandidate>>& RTCIceTransport::getLocalCandidates()
    const {
  return local_candidates_;
}

const HeapVector<Member<RTCIceCandidate>>&
RTCIceTransport::getRemoteCandidates() const {
  return remote_candidates_;
}

RTCIceCandidatePair* RTCIceTransport::getSelectedCandidatePair() const {
  return selected_candidate_pair_.Get();
}

RTCIceParameters* RTCIceTransport::getLocalParameters() const {
  return local_parameters_.Get();
}

RTCIceParameters* RTCIceTransport::getRemoteParameters() const {
  return remote_parameters_.Get();
}

void RTCIceTransport::OnGatheringStateChanged(
    cricket::IceGatheringState new_state) {
  if (new_state == gathering_state_) {
    return;
  }
  if (new_state == cricket::kIceGatheringComplete) {
    // Generate a null ICE candidate to signal the end of candidates.
    DispatchEvent(*RTCPeerConnectionIceEvent::Create(nullptr));
  }
  gathering_state_ = new_state;
  DispatchEvent(*Event::Create(event_type_names::kGatheringstatechange));
}
void RTCIceTransport::OnCandidateGathered(
    const cricket::Candidate& parsed_candidate) {
  RTCIceCandidate* candidate = ConvertToRtcIceCandidate(parsed_candidate);
  local_candidates_.push_back(candidate);
}

void RTCIceTransport::OnStateChanged(webrtc::IceTransportState new_state) {
  // MONKEY PATCH:
  // Due to crbug.com/957487, the lower layers signal kFailed when they
  // should have been sending kDisconnected. Remap the state.
  if (new_state == webrtc::IceTransportState::kFailed) {
    LOG(INFO) << "crbug/957487: Remapping ICE state failed to disconnected";
    new_state = webrtc::IceTransportState::kDisconnected;
  }
  if (new_state == state_) {
    return;
  }
  state_ = new_state;
  if (state_ == webrtc::IceTransportState::kFailed) {
    selected_candidate_pair_ = nullptr;
  }
  // Make sure the peerconnection's state is updated before the event fires.
  if (peer_connection_) {
    peer_connection_->UpdateIceConnectionState();
  }
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
  if (state_ == webrtc::IceTransportState::kClosed ||
      state_ == webrtc::IceTransportState::kFailed) {
    Stop();
  }
}

void RTCIceTransport::OnSelectedCandidatePairChanged(
    const std::pair<cricket::Candidate, cricket::Candidate>&
        selected_candidate_pair) {
  RTCIceCandidate* local =
      ConvertToRtcIceCandidate(selected_candidate_pair.first);
  RTCIceCandidate* remote =
      ConvertToRtcIceCandidate(selected_candidate_pair.second);
  selected_candidate_pair_ = RTCIceCandidatePair::Create();
  selected_candidate_pair_->setLocal(local);
  selected_candidate_pair_->setRemote(remote);
  DispatchEvent(*Event::Create(event_type_names::kSelectedcandidatepairchange));
}

void RTCIceTransport::Close(CloseReason reason) {
  if (IsClosed()) {
    return;
  }
  state_ = webrtc::IceTransportState::kClosed;
  selected_candidate_pair_ = nullptr;
  proxy_.reset();
}

bool RTCIceTransport::RaiseExceptionIfClosed(
    ExceptionState& exception_state) const {
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The RTCIceTransport's state is 'closed'.");
    return true;
  }
  return false;
}

const AtomicString& RTCIceTransport::InterfaceName() const {
  return event_target_names::kRTCIceTransport;
}

ExecutionContext* RTCIceTransport::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void RTCIceTransport::ContextDestroyed() {
  Close(CloseReason::kContextDestroyed);
}

bool RTCIceTransport::HasPendingActivity() const {
  // Only allow the RTCIceTransport to be garbage collected if the ICE
  // implementation is not active.
  return !!proxy_;
}

void RTCIceTransport::Trace(Visitor* visitor) const {
  visitor->Trace(local_candidates_);
  visitor->Trace(remote_candidates_);
  visitor->Trace(local_parameters_);
  visitor->Trace(remote_parameters_);
  visitor->Trace(selected_candidate_pair_);
  visitor->Trace(peer_connection_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void RTCIceTransport::Dispose() {
  Close(CloseReason::kDisposed);
}

}  // namespace blink
