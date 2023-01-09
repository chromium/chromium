// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_gather_options.h"
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
#include "third_party/webrtc/pc/ice_server_parsing.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

namespace blink {
namespace {

const char* kIceRoleControllingStr = "controlling";
const char* kIceRoleControlledStr = "controlled";

absl::optional<cricket::Candidate> ConvertToCricketIceCandidate(
    const RTCIceCandidate& candidate) {
  webrtc::JsepIceCandidate jsep_candidate("", 0);
  webrtc::SdpParseError error;
  if (!webrtc::SdpDeserializeCandidate(candidate.candidate().Utf8(),
                                       &jsep_candidate, &error)) {
    LOG(WARNING) << "Failed to deserialize candidate: " << error.description;
    return absl::nullopt;
  }
  return jsep_candidate.candidate();
}

RTCIceCandidate* ConvertToRtcIceCandidate(const cricket::Candidate& candidate) {
  return RTCIceCandidate::Create(MakeGarbageCollected<RTCIceCandidatePlatform>(
      String::FromUTF8(webrtc::SdpSerializeCandidate(candidate)), "", 0));
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

RTCIceTransport* RTCIceTransport::Create(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory) {
  return MakeGarbageCollected<RTCIceTransport>(context, std::move(proxy_thread),
                                               std::move(host_thread),
                                               std::move(adapter_factory));
}

RTCIceTransport::RTCIceTransport(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory,
    RTCPeerConnection* peer_connection)
    : ExecutionContextLifecycleObserver(context),
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

  GenerateLocalParameters();
}

RTCIceTransport::RTCIceTransport(
    ExecutionContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
    scoped_refptr<base::SingleThreadTaskRunner> host_thread,
    std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory)
    : RTCIceTransport(context,
                      std::move(proxy_thread),
                      std::move(host_thread),
                      std::move(adapter_factory),
                      nullptr) {}

RTCIceTransport::~RTCIceTransport() {
  DCHECK(!proxy_);
}

bool RTCIceTransport::IsFromPeerConnection() const {
  return peer_connection_;
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

RTCIceCandidatePair* RTCIceTransport::getSelectedCandidatePair() const {
  return selected_candidate_pair_;
}

RTCIceParameters* RTCIceTransport::getLocalParameters() const {
  return local_parameters_;
}

RTCIceParameters* RTCIceTransport::getRemoteParameters() const {
  return remote_parameters_;
}

static webrtc::PeerConnectionInterface::IceServer ConvertIceServer(
    const RTCIceServer* ice_server) {
  webrtc::PeerConnectionInterface::IceServer converted_ice_server;
  // Prefer standardized 'urls' field over deprecated 'url' field.
  Vector<String> url_strings;
  if (ice_server->hasUrls()) {
    switch (ice_server->urls()->GetContentType()) {
      case V8UnionStringOrStringSequence::ContentType::kString:
        url_strings.push_back(ice_server->urls()->GetAsString());
        break;
      case V8UnionStringOrStringSequence::ContentType::kStringSequence:
        url_strings = ice_server->urls()->GetAsStringSequence();
        break;
    }
  } else if (ice_server->hasUrl()) {
    url_strings.push_back(ice_server->url());
  }
  for (const String& url_string : url_strings) {
    converted_ice_server.urls.push_back(url_string.Utf8());
  }
  if (ice_server->hasUsername()) {
    converted_ice_server.username = ice_server->username().Utf8();
  }
  if (ice_server->hasCredential()) {
    converted_ice_server.password = ice_server->credential().Utf8();
  }
  return converted_ice_server;
}

static webrtc::RTCErrorOr<cricket::IceParameters> ConvertIceParameters(
    const RTCIceParameters* raw_ice_parameters) {
  std::string raw_ufrag = raw_ice_parameters->usernameFragment().Utf8();
  std::string raw_pwd = raw_ice_parameters->password().Utf8();
  return cricket::IceParameters::Parse(raw_ufrag, raw_pwd);
}

static WebVector<webrtc::PeerConnectionInterface::IceServer> ConvertIceServers(
    const HeapVector<Member<RTCIceServer>>& ice_servers) {
  WebVector<webrtc::PeerConnectionInterface::IceServer> converted_ice_servers;
  for (const RTCIceServer* ice_server : ice_servers) {
    converted_ice_servers.emplace_back(ConvertIceServer(ice_server));
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

void RTCIceTransport::gather(RTCIceGatherOptions* options,
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
  WebVector<webrtc::PeerConnectionInterface::IceServer> ice_servers;
  if (options->hasIceServers()) {
    ice_servers = ConvertIceServers(options->iceServers());
  }
  cricket::ServerAddresses stun_servers;
  std::vector<cricket::RelayServerConfig> turn_servers;
  webrtc::RTCError error = webrtc::ParseIceServersOrError(
      ice_servers.ReleaseVector(), &stun_servers, &turn_servers);
  if (!error.ok()) {
    ThrowExceptionFromRTCError(error, exception_state);
    return;
  }
  gathering_state_ = cricket::kIceGatheringGathering;
  proxy_->StartGathering(ConvertIceParameters(local_parameters_).value(),
                         stun_servers, turn_servers,
                         IceTransportPolicyFromString(options->gatherPolicy()));
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

static bool RTCIceParametersAreEqual(const RTCIceParameters* a,
                                     const RTCIceParameters* b) {
  return a->usernameFragment() == b->usernameFragment() &&
         a->password() == b->password();
}

void RTCIceTransport::start(RTCIceParameters* raw_remote_parameters,
                            const String& role_string,
                            ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  if (!raw_remote_parameters->hasUsernameFragment() ||
      !raw_remote_parameters->hasPassword()) {
    exception_state.ThrowTypeError(
        "remoteParameters must have usernameFragment and password fields set.");
    return;
  }
  webrtc::RTCErrorOr<cricket::IceParameters> maybe_remote_parameters =
      ConvertIceParameters(raw_remote_parameters);
  if (!maybe_remote_parameters.ok()) {
    ThrowExceptionFromRTCError(maybe_remote_parameters.error(),
                               exception_state);
    return;
  }
  cricket::IceParameters remote_parameters =
      maybe_remote_parameters.MoveValue();
  cricket::IceRole role = IceRoleFromString(role_string);
  if (role_ != cricket::ICEROLE_UNKNOWN && role != role_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot change role once start() has been called.");
    return;
  }
  if (remote_parameters_ &&
      RTCIceParametersAreEqual(remote_parameters_, raw_remote_parameters)) {
    // No change to remote parameters: do nothing.
    return;
  }
  if (!remote_parameters_) {
    // Calling start() for the first time.
    role_ = role;
    if (remote_candidates_.size() > 0) {
      state_ = webrtc::IceTransportState::kChecking;
    }
    Vector<cricket::Candidate> initial_remote_candidates;
    for (RTCIceCandidate* remote_candidate : remote_candidates_) {
      // This conversion is safe since we throw an exception in
      // addRemoteCandidate on malformed ICE candidates.
      initial_remote_candidates.push_back(
          *ConvertToCricketIceCandidate(*remote_candidate));
    }
    proxy_->Start(remote_parameters, role, initial_remote_candidates);
  } else {
    remote_candidates_.clear();
    state_ = webrtc::IceTransportState::kNew;
    proxy_->HandleRemoteRestart(remote_parameters);
  }

  remote_parameters_ = raw_remote_parameters;
}

void RTCIceTransport::stop() {
  Close(CloseReason::kStopped);
}

void RTCIceTransport::addRemoteCandidate(RTCIceCandidate* remote_candidate,
                                         ExceptionState& exception_state) {
  if (RaiseExceptionIfClosed(exception_state)) {
    return;
  }
  absl::optional<cricket::Candidate> converted_remote_candidate =
      ConvertToCricketIceCandidate(*remote_candidate);
  if (!converted_remote_candidate) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Invalid ICE candidate.");
    return;
  }
  remote_candidates_.push_back(remote_candidate);
  if (remote_parameters_) {
    proxy_->AddRemoteCandidate(*converted_remote_candidate);
    state_ = webrtc::IceTransportState::kChecking;
  }
}

void RTCIceTransport::GenerateLocalParameters() {
  local_parameters_ = RTCIceParameters::Create();
  local_parameters_->setUsernameFragment(
      String::FromUTF8(rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH)));
  local_parameters_->setPassword(
      String::FromUTF8(rtc::CreateRandomString(cricket::ICE_PWD_LENGTH)));
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
  RTCPeerConnectionIceEventInit* event_init =
      RTCPeerConnectionIceEventInit::Create();
  event_init->setCandidate(candidate);
  DispatchEvent(*RTCPeerConnectionIceEvent::Create(
      event_type_names::kIcecandidate, event_init));
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
    stop();
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
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void RTCIceTransport::Dispose() {
  Close(CloseReason::kDisposed);
}

}  // namespace blink
