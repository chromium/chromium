// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_sctp_transport.h"

#include <limits>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_sctp_transport_state.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/sctp_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"

namespace blink {

namespace {
V8RTCSctpTransportState::Enum TransportStateToEnum(
    webrtc::SctpTransportState state) {
  switch (state) {
    case webrtc::SctpTransportState::kConnecting:
      return V8RTCSctpTransportState::Enum::kConnecting;
    case webrtc::SctpTransportState::kConnected:
      return V8RTCSctpTransportState::Enum::kConnected;
    case webrtc::SctpTransportState::kClosed:
      return V8RTCSctpTransportState::Enum::kClosed;
    case webrtc::SctpTransportState::kNew:
    case webrtc::SctpTransportState::kNumValues:
      // These shouldn't occur.
      break;
  }
  NOTREACHED();
}

std::unique_ptr<SctpTransportProxy> CreateProxy(
    ExecutionContext* context,
    webrtc::SctpTransportInterface* native_transport,
    SctpTransportProxy::Delegate* delegate,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> worker_thread) {
  DCHECK(main_thread);
  DCHECK(worker_thread);
  LocalFrame* frame = To<LocalDOMWindow>(context)->GetFrame();
  DCHECK(frame);
  return SctpTransportProxy::Create(
      *frame, main_thread, worker_thread,
      rtc::scoped_refptr<webrtc::SctpTransportInterface>(native_transport),
      delegate);
}

}  // namespace

RTCSctpTransport::RTCSctpTransport(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::SctpTransportInterface> native_transport)
    : RTCSctpTransport(context,
                       native_transport,
                       context->GetTaskRunner(TaskType::kNetworking),
                       PeerConnectionDependencyFactory::From(*context)
                           .GetWebRtcNetworkTaskRunner()) {}

RTCSctpTransport::RTCSctpTransport(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::SctpTransportInterface> native_transport,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<base::SingleThreadTaskRunner> worker_thread)
    : ExecutionContextClient(context),
      current_state_(webrtc::SctpTransportState::kNew),
      native_transport_(native_transport),
      proxy_(CreateProxy(context,
                         native_transport.get(),
                         this,
                         main_thread,
                         worker_thread)) {}

RTCSctpTransport::~RTCSctpTransport() {}

V8RTCSctpTransportState RTCSctpTransport::state() const {
  if (closed_from_owner_) {
    return V8RTCSctpTransportState(V8RTCSctpTransportState::Enum::kClosed);
  }
  return V8RTCSctpTransportState(TransportStateToEnum(current_state_.state()));
}

double RTCSctpTransport::maxMessageSize() const {
  if (current_state_.MaxMessageSize()) {
    return *current_state_.MaxMessageSize();
  }
  // Spec says:
  // If local size is unlimited and remote side is unknown, return infinity.
  // http://w3c.github.io/webrtc-pc/#dfn-update-the-data-max-message-size
  return std::numeric_limits<double>::infinity();
}

std::optional<int16_t> RTCSctpTransport::maxChannels() const {
  if (!current_state_.MaxChannels())
    return std::nullopt;
  return current_state_.MaxChannels().value();
}

RTCDtlsTransport* RTCSctpTransport::transport() const {
  return dtls_transport_.Get();
}

rtc::scoped_refptr<webrtc::SctpTransportInterface>
RTCSctpTransport::native_transport() {
  return native_transport_;
}

void RTCSctpTransport::ChangeState(webrtc::SctpTransportInformation info) {
  DCHECK(current_state_.state() != webrtc::SctpTransportState::kClosed);
  current_state_ = info;
}

void RTCSctpTransport::SetTransport(RTCDtlsTransport* transport) {
  dtls_transport_ = transport;
}

// Implementation of SctpTransportProxy::Delegate
void RTCSctpTransport::OnStartCompleted(webrtc::SctpTransportInformation info) {
  current_state_ = info;
  start_completed_ = true;
}

void RTCSctpTransport::OnStateChange(webrtc::SctpTransportInformation info) {
  // We depend on closed only happening once for safe garbage collection.
  DCHECK(current_state_.state() != webrtc::SctpTransportState::kClosed);
  current_state_ = info;
  // When Close() has been called, we do not report the state change from the
  // lower layer, but we keep the SctpTransport object alive until the
  // lower layer has sent notice that the closing has been completed.
  if (!closed_from_owner_) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

void RTCSctpTransport::Close() {
  closed_from_owner_ = true;
  if (current_state_.state() != webrtc::SctpTransportState::kClosed) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

const AtomicString& RTCSctpTransport::InterfaceName() const {
  return event_target_names::kRTCSctpTransport;
}

ExecutionContext* RTCSctpTransport::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void RTCSctpTransport::Trace(Visitor* visitor) const {
  visitor->Trace(dtls_transport_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  SctpTransportProxy::Delegate::Trace(visitor);
}

}  // namespace blink
