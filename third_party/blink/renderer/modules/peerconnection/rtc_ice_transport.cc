// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_gather_options.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_parameters.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_server.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_event_init.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/jsepicecandidate.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/p2p/base/portallocator.h"
#include "third_party/webrtc/p2p/base/transportdescription.h"
#include "third_party/webrtc/pc/iceserverparsing.h"
#include "third_party/webrtc/pc/webrtcsdp.h"

namespace blink {
namespace {

const char* kIceRoleControllingStr = "controlling";
const char* kIceRoleControlledStr = "controlled";

base::Optional<cricket::Candidate> ConvertToCricketIceCandidate(
    const RTCIceCandidate& candidate) {
  webrtc::JsepIceCandidate jsep_candidate("", 0);
  webrtc::SdpParseError error;
  if (!webrtc::SdpDeserializeCandidate(WebString(candidate.candidate()).Utf8(),
                                       &jsep_candidate, &error)) {
    LOG(WARNING) << "Failed to deserialize candidate: " << error.description;
    return base::nullopt;
  }
  return jsep_candidate.candidate();
}

RTCIceCandidate* ConvertToRtcIceCandidate(const cricket::Candidate& candidate) {
  return RTCIceCandidate::Create(WebRTCICECandidate::Create(
      WebString::FromUTF8(webrtc::SdpSerializeCandidate(candidate)), "", 0));
}

class DefaultIceTransportAdapterCrossThreadFactory
    : public IceTransportAdapterCrossThreadFactory {
 public:
  void InitializeOnMainThread() override {
    DCHECK(!port_allocator_);
    DCHECK(!worker_thread_rtc_thread_);
    port_allocator_ = Platform::Current()->CreateWebRtcPortAllocator(
        WebLocalFrame::FrameForCurrentContext());
    worker_thread_rtc_thread_ =
        Platform::Current()->GetWebRtcWorkerThreadRtcThread();
  }

  std::unique_ptr<IceTransportAdapter> ConstructOnWorkerThread(
      IceTransportAdapter::Delegate* delegate) override {
    DCHECK(port_allocator_);
    DCHECK(worker_thread_rtc_thread_);
    return std::make_unique<IceTransportAdapterImpl>(
        delegate, std::move(port_allocator_), worker_thread_rtc_thread_);
  }

 private:
  std::unique_ptr<cricket::PortAllocator> port_allocator_;
  rtc::Thread* worker_thread_rtc_thread_ = nullptr;
};

}  // namespace

RTCIceTransport* RTCIceTransport::Create(ExecutionContext* context) {
  LocalFrame* frame = To<Document>(context)->GetFrame();
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      frame->GetTaskRunner(TaskType::kNetworking);
  scoped_refptr<base::SingleThreadTaskRunner> host_thread =
      Platform::Current()->GetWebRtcWorkerThread();
  return new RTCIceTransport(
      context, std::move(proxy_thread), std::move(host_thread),
      std::make_unique<DefaultIceTransportAdapterCrossThreadFactory>());
}

RTCIceTransport* RTCIceTransport::Create(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory) {
  return new RTCIceTransport(context, std::move(proxy_thread),
                             std::move(host_thread),
                             std::move(adapter_factory));
}

RTCIceTransport::RTCIceTransport(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory)
    : ContextLifecycleObserver(context) {
  DCHECK(context);
  DCHECK(proxy_thread);
  DCHECK(host_thread);
  DCHECK(adapter_factory);
  DCHECK(proxy_thread->BelongsToCurrentThread());

  LocalFrame* frame = To<Document>(context)->GetFrame();
  DCHECK(frame);
  proxy_.reset(new IceTransportProxy(
      frame->GetFrameScheduler(), std::move(proxy_thread),
      std::move(host_thread), this, std::move(adapter_factory)));

  GenerateLocalParameters();
}

RTCIceTransport::~RTCIceTransport() {
  DCHECK(!proxy_);
}

bool RTCIceTransport::HasConsumer() const {
  return consumer_;
}

IceTransportProxy* RTCIceTransport::ConnectConsumer(
    RTCQuicTransport* consumer) {
  DCHECK(consumer);
  DCHECK(proxy_);
  if (!consumer_) {
    consumer_ = consumer;
  } else {
    DCHECK_EQ(consumer_, consumer);
  }
  return proxy_.get();
}

void RTCIceTransport::DisconnectConsumer(RTCQuicTransport* consumer) {
  DCHECK(consumer);
  DCHECK(proxy_);
  DCHECK_EQ(consumer, consumer_);
  consumer_ = nullptr;
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
  NOTREACHED();
  return String();
}

String RTCIceTransport::state() const {
  switch (state_) {
    case RTCIceTransportState::kNew:
      return "new";
    case RTCIceTransportState::kChecking:
      return "checking";
    case RTCIceTransportState::kConnected:
      return "connected";
    case RTCIceTransportState::kCompleted:
      return "completed";
    case RTCIceTransportState::kDisconnected:
      return "disconnected";
    case RTCIceTransportState::kFailed:
      return "failed";
    case RTCIceTransportState::kClosed:
      return "closed";
  }
  NOTREACHED();
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
      NOTREACHED();
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

void RTCIceTransport::getSelectedCandidatePair(
    base::Optional<RTCIceCandidatePair>& result) const {
  result = selected_candidate_pair_;
}

void RTCIceTransport::getLocalParameters(
    base::Optional<RTCIceParameters>& result) const {
  result = local_parameters_;
}

void RTCIceTransport::getRemoteParameters(
    base::Optional<RTCIceParameters>& result) const {
  result = remote_parameters_;
}

static webrtc::PeerConnectionInterface::IceServer ConvertIceServer(
    const RTCIceServer& ice_server) {
  webrtc::PeerConnectionInterface::IceServer converted_ice_server;
  // Prefer standardized 'urls' field over deprecated 'url' field.
  Vector<String> url_strings;
  if (ice_server.hasURLs()) {
    if (ice_server.urls().IsString()) {
      url_strings.push_back(ice_server.urls().GetAsString());
    } else if (ice_server.urls().IsStringSequence()) {
      url_strings = ice_server.urls().GetAsStringSequence();
    }
  } else if (ice_server.hasURL()) {
    url_strings.push_back(ice_server.url());
  }
  for (const String& url_string : url_strings) {
    converted_ice_server.urls.push_back(WebString(url_string).Utf8());
  }
  converted_ice_server.username = WebString(ice_server.username()).Utf8();
  converted_ice_server.password = WebString(ice_server.credential()).Utf8();
  return converted_ice_server;
}

static cricket::IceParameters ConvertIceParameters(
    const RTCIceParameters& ice_parameters) {
  cricket::IceParameters converted_ice_parameters;
  converted_ice_parameters.ufrag =
      WebString(ice_parameters.usernameFragment()).Utf8();
  converted_ice_parameters.pwd = WebString(ice_parameters.password()).Utf8();
  return converted_ice_parameters;
}

static std::vector<webrtc::PeerConnectionInterface::IceServer>
ConvertIceServers(const HeapVector<RTCIceServer>& ice_servers) {
  std::vector<webrtc::PeerConnectionInterface::IceServer> converted_ice_servers;
  for (const RTCIceServer& ice_server : ice_servers) {
    converted_ice_servers.push_back(ConvertIceServer(ice_server));
  }
  return converted_ice_servers;
}

static IceTransportPolicy IceTransportPolicyFromString(const String& str) {
  if (str == "relay") {
    return IceTransportPolicy::kRelay;
  }
  if (str == "all") {
    return IceTransportPolicy::kAll;
  }
  NOTREACHED();
  return IceTransportPolicy::kAll;
}

void RTCIceTransport::gather(const RTCIceGatherOptions& options,
                             ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  // TODO(github.com/w3c/webrtc-ice/issues/7): Possibly support calling gather()
  // more than once.
  if (gathering_state_ != cricket::kIceGatheringNew) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Can only call gather() once.");
    return;
  }
  std::vector<webrtc::PeerConnectionInterface::IceServer> ice_servers;
  if (options.hasIceServers()) {
    ice_servers = ConvertIceServers(options.iceServers());
  }
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  webrtc::RTCErrorType error_type =
      webrtc::ParseIceServers(ice_servers, &stun_servers, &turn_servers);
  if (error_type != webrtc::RTCErrorType::NONE) {
    ThrowExceptionFromRTCError(
        webrtc::RTCError(error_type, "Invalid ICE server URL(s)."),
        exception_state);
    return;
  }
  gathering_state_ = cricket::kIceGatheringGathering;
  proxy_->StartGathering(ConvertIceParameters(local_parameters_), stun_servers,
                         turn_servers,
                         IceTransportPolicyFromString(options.gatherPolicy()));
}

static cricket::IceRole IceRoleFromString(const String& role_string) {
  if (role_string == kIceRoleControllingStr) {
    return cricket::ICEROLE_CONTROLLING;
  }
  if (role_string == kIceRoleControlledStr) {
    return cricket::ICEROLE_CONTROLLED;
  }
  NOTREACHED();
  return cricket::ICEROLE_UNKNOWN;
}

static bool RTCIceParametersAreEqual(const RTCIceParameters& a,
                                     const RTCIceParameters& b) {
  return a.usernameFragment() == b.usernameFragment() &&
         a.password() == b.password();
}

void RTCIceTransport::start(const RTCIceParameters& remote_parameters,
                            const String& role_string,
                            ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (!remote_parameters.hasUsernameFragment() ||
      !remote_parameters.hasPassword()) {
    exception_state.ThrowTypeError(
        "remoteParameters must have usernameFragment and password fields set.");
    return;
  }
  cricket::IceRole role = IceRoleFromString(role_string);
  if (role_ != cricket::ICEROLE_UNKNOWN && role != role_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot change role once start() has been called.");
    return;
  }
  if (remote_parameters_ &&
      RTCIceParametersAreEqual(*remote_parameters_, remote_parameters)) {
    // No change to remote parameters: do nothing.
    return;
  }
  if (!remote_parameters_) {
    // Calling start() for the first time.
    role_ = role;
    if (remote_candidates_.size() > 0) {
      state_ = RTCIceTransportState::kChecking;
    }
    std::vector<cricket::Candidate> initial_remote_candidates;
    for (RTCIceCandidate* remote_candidate : remote_candidates_) {
      // This conversion is safe since we throw an exception in
      // addRemoteCandidate on malformed ICE candidates.
      initial_remote_candidates.push_back(
          *ConvertToCricketIceCandidate(*remote_candidate));
    }
    proxy_->Start(ConvertIceParameters(remote_parameters), role,
                  initial_remote_candidates);
    if (consumer_) {
      consumer_->OnTransportStarted();
    }
  } else {
    remote_candidates_.clear();
    state_ = RTCIceTransportState::kNew;
    proxy_->HandleRemoteRestart(ConvertIceParameters(remote_parameters));
  }
  remote_parameters_ = remote_parameters;
}

void RTCIceTransport::stop() {
  if (IsClosed()) {
    return;
  }
  if (HasConsumer()) {
    consumer_->stop();
  }
  // Stopping the consumer should cause it to disconnect.
  DCHECK(!HasConsumer());
  state_ = RTCIceTransportState::kClosed;
  selected_candidate_pair_ = base::nullopt;
  proxy_.reset();
}

void RTCIceTransport::addRemoteCandidate(RTCIceCandidate* remote_candidate,
                                         ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  base::Optional<cricket::Candidate> converted_remote_candidate =
      ConvertToCricketIceCandidate(*remote_candidate);
  if (!converted_remote_candidate) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Invalid ICE candidate.");
    return;
  }
  remote_candidates_.push_back(remote_candidate);
  if (remote_parameters_) {
    proxy_->AddRemoteCandidate(*converted_remote_candidate);
    state_ = RTCIceTransportState::kChecking;
  }
}

void RTCIceTransport::GenerateLocalParameters() {
  local_parameters_.setUsernameFragment(
      WebString::FromUTF8(rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH)));
  local_parameters_.setPassword(
      WebString::FromUTF8(rtc::CreateRandomString(cricket::ICE_PWD_LENGTH)));
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
  DispatchEvent(*Event::Create(EventTypeNames::gatheringstatechange));
}

void RTCIceTransport::OnCandidateGathered(
    const cricket::Candidate& parsed_candidate) {
  RTCIceCandidate* candidate = ConvertToRtcIceCandidate(parsed_candidate);
  local_candidates_.push_back(candidate);
  RTCPeerConnectionIceEventInit event_init;
  event_init.setCandidate(candidate);
  DispatchEvent(*RTCPeerConnectionIceEvent::Create(EventTypeNames::icecandidate,
                                                   event_init));
}

static RTCIceTransportState ConvertIceTransportState(
    cricket::IceTransportState state) {
  switch (state) {
    case cricket::IceTransportState::STATE_INIT:
      return RTCIceTransportState::kNew;
    case cricket::IceTransportState::STATE_CONNECTING:
      return RTCIceTransportState::kChecking;
    case cricket::IceTransportState::STATE_COMPLETED:
      return RTCIceTransportState::kConnected;
    case cricket::IceTransportState::STATE_FAILED:
      return RTCIceTransportState::kFailed;
    default:
      NOTREACHED();
      return RTCIceTransportState::kClosed;
  }
}

void RTCIceTransport::OnStateChanged(cricket::IceTransportState new_state) {
  RTCIceTransportState local_new_state = ConvertIceTransportState(new_state);
  if (local_new_state == state_) {
    return;
  }
  state_ = local_new_state;
  if (state_ == RTCIceTransportState::kFailed) {
    selected_candidate_pair_ = base::nullopt;
  }
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

void RTCIceTransport::OnSelectedCandidatePairChanged(
    const std::pair<cricket::Candidate, cricket::Candidate>&
        selected_candidate_pair) {
  RTCIceCandidate* local =
      ConvertToRtcIceCandidate(selected_candidate_pair.first);
  RTCIceCandidate* remote =
      ConvertToRtcIceCandidate(selected_candidate_pair.second);
  selected_candidate_pair_ = RTCIceCandidatePair();
  selected_candidate_pair_->setLocal(local);
  selected_candidate_pair_->setRemote(remote);
  DispatchEvent(*Event::Create(EventTypeNames::selectedcandidatepairchange));
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
  return EventTargetNames::RTCIceTransport;
}

ExecutionContext* RTCIceTransport::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void RTCIceTransport::ContextDestroyed(ExecutionContext*) {
  stop();
}

bool RTCIceTransport::HasPendingActivity() const {
  // Only allow the RTCIceTransport to be garbage collected if the ICE
  // implementation is not active.
  return static_cast<bool>(proxy_);
}

void RTCIceTransport::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_candidates_);
  visitor->Trace(remote_candidates_);
  visitor->Trace(selected_candidate_pair_);
  visitor->Trace(consumer_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
