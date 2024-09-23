// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtls_transport.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/dtls_transport_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_util.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/webrtc/api/dtls_transport_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace blink {

namespace {
String TransportStateToString(webrtc::DtlsTransportState state) {
  switch (state) {
    case webrtc::DtlsTransportState::kNew:
      return String("new");
    case webrtc::DtlsTransportState::kConnecting:
      return String("connecting");
    case webrtc::DtlsTransportState::kConnected:
      return String("connected");
    case webrtc::DtlsTransportState::kClosed:
      return String("closed");
    case webrtc::DtlsTransportState::kFailed:
      return String("failed");
    default:
      NOTREACHED_IN_MIGRATION();
      return String("failed");
  }
}

std::unique_ptr<DtlsTransportProxy> CreateProxy(
    ExecutionContext* context,
    webrtc::DtlsTransportInterface* native_transport,
    DtlsTransportProxy::Delegate* delegate) {
  LocalFrame* frame = To<LocalDOMWindow>(context)->GetFrame();
  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread =
      frame->GetTaskRunner(TaskType::kNetworking);
  scoped_refptr<base::SingleThreadTaskRunner> host_thread =
      PeerConnectionDependencyFactory::From(*context)
          .GetWebRtcNetworkTaskRunner();
  return DtlsTransportProxy::Create(*frame, proxy_thread, host_thread,
                                    native_transport, delegate);
}

}  // namespace

RTCDtlsTransport::RTCDtlsTransport(
    ExecutionContext* context,
    rtc::scoped_refptr<webrtc::DtlsTransportInterface> native_transport,
    RTCIceTransport* ice_transport)
    : ExecutionContextClient(context),
      current_state_(webrtc::DtlsTransportState::kNew),
      native_transport_(native_transport),
      proxy_(CreateProxy(context, native_transport.get(), this)),
      ice_transport_(ice_transport) {}

RTCDtlsTransport::~RTCDtlsTransport() {}

String RTCDtlsTransport::state() const {
  if (closed_from_owner_) {
    return TransportStateToString(webrtc::DtlsTransportState::kClosed);
  }
  return TransportStateToString(current_state_.state());
}

const HeapVector<Member<DOMArrayBuffer>>&
RTCDtlsTransport::getRemoteCertificates() const {
  return remote_certificates_;
}

RTCIceTransport* RTCDtlsTransport::iceTransport() const {
  return ice_transport_.Get();
}

webrtc::DtlsTransportInterface* RTCDtlsTransport::native_transport() {
  return native_transport_.get();
}

void RTCDtlsTransport::ChangeState(webrtc::DtlsTransportInformation info) {
  DCHECK(info.state() == webrtc::DtlsTransportState::kClosed ||
         current_state_.state() != webrtc::DtlsTransportState::kClosed);
  current_state_ = info;
}

void RTCDtlsTransport::Close() {
  closed_from_owner_ = true;
  if (current_state_.state() != webrtc::DtlsTransportState::kClosed) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
  ice_transport_->Stop();
}

// Implementation of DtlsTransportProxy::Delegate
void RTCDtlsTransport::OnStartCompleted(webrtc::DtlsTransportInformation info) {
  current_state_ = info;
}

void RTCDtlsTransport::OnStateChange(webrtc::DtlsTransportInformation info) {
  // We depend on closed only happening once for safe garbage collection.
  DCHECK(current_state_.state() != webrtc::DtlsTransportState::kClosed);
  current_state_ = info;

  // DTLS 1.0 is deprecated, emit a console warning.
  if (current_state_.state() == webrtc::DtlsTransportState::kConnected) {
    if (current_state_.tls_version()) {
      if (*current_state_.tls_version() == DTLS1_VERSION ||
          *current_state_.tls_version() == SSL3_VERSION ||
          *current_state_.tls_version() == TLS1_VERSION ||
          *current_state_.tls_version() == TLS1_1_VERSION) {
        Deprecation::CountDeprecation(GetExecutionContext(),
                                      WebFeature::kObsoleteWebrtcTlsVersion);
      }
    }
  }

  // If the certificates have changed, copy them as DOMArrayBuffers.
  // This makes sure that getRemoteCertificates() == getRemoteCertificates()
  if (current_state_.remote_ssl_certificates()) {
    const rtc::SSLCertChain* certs = current_state_.remote_ssl_certificates();
    if (certs->GetSize() != remote_certificates_.size()) {
      remote_certificates_.clear();
      for (size_t i = 0; i < certs->GetSize(); i++) {
        auto& cert = certs->Get(i);
        rtc::Buffer der_cert;
        cert.ToDER(&der_cert);
        DOMArrayBuffer* dab_cert = DOMArrayBuffer::Create(der_cert);
        remote_certificates_.push_back(dab_cert);
      }
    } else {
      // Replace certificates that have changed, if any
      for (WTF::wtf_size_t i = 0; i < certs->GetSize(); i++) {
        auto& cert = certs->Get(i);
        rtc::Buffer der_cert;
        cert.ToDER(&der_cert);
        DOMArrayBuffer* dab_cert = DOMArrayBuffer::Create(der_cert);
        // Don't replace the certificate if it's unchanged.
        // Should have been "if (*dab_cert != *remote_certificates_[i])"
        if (dab_cert->ByteLength() != remote_certificates_[i]->ByteLength() ||
            memcmp(dab_cert->Data(), remote_certificates_[i]->Data(),
                   dab_cert->ByteLength()) != 0) {
          remote_certificates_[i] = dab_cert;
        }
      }
    }
  } else {
    remote_certificates_.clear();
  }
  if (!closed_from_owner_) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

const AtomicString& RTCDtlsTransport::InterfaceName() const {
  return event_target_names::kRTCDtlsTransport;
}

ExecutionContext* RTCDtlsTransport::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void RTCDtlsTransport::Trace(Visitor* visitor) const {
  visitor->Trace(remote_certificates_);
  visitor->Trace(ice_transport_);
  DtlsTransportProxy::Delegate::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
