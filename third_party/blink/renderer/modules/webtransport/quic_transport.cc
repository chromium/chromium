// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

KURL ReparseURLAsHTTPS(KURL url) {
  url.SetProtocol("https");
  return url;
}

}  // namespace

QuicTransport* QuicTransport::Create(ScriptState* script_state,
                                     const String& url,
                                     ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Create() url=" << url;
  auto* transport =
      MakeGarbageCollected<QuicTransport>(PassKey(), script_state, url);
  transport->Init(exception_state);
  return transport;
}

QuicTransport::QuicTransport(PassKey,
                             ScriptState* script_state,
                             const String& url)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      url_(NullURL(), url) {}

void QuicTransport::close(const WebTransportCloseInfo* close_info) {
  DVLOG(1) << "QuicTransport::close() this=" << this;
  // TODO(ricea): Send |close_info| to the network service.
  Dispose();
}

void QuicTransport::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::QuicTransport> quic_transport,
    mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>
        client_receiver) {
  DVLOG(1) << "QuicTransport::OnConnectionEstablished() this=" << this;
  handshake_client_receiver_.reset();

  // TODO(ricea): Report to devtools.

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);

  client_receiver_.Bind(std::move(client_receiver), task_runner);
  client_receiver_.set_disconnect_handler(
      WTF::Bind(&QuicTransport::OnConnectionError, WrapWeakPersistent(this)));

  DCHECK(!quic_transport_);
  quic_transport_.Bind(std::move(quic_transport), task_runner);
}

QuicTransport::~QuicTransport() = default;

void QuicTransport::OnHandshakeFailed() {
  DVLOG(1) << "QuicTransport::OnHandshakeFailed() this=" << this;
  handshake_client_receiver_.reset();
}

void QuicTransport::ContextDestroyed(ExecutionContext* execution_context) {
  DVLOG(1) << "QuicTransport::ContextDestroyed() this=" << this;
  Dispose();
}

bool QuicTransport::HasPendingActivity() const {
  DVLOG(1) << "QuicTransport::HasPendingActivity() this=" << this;
  return handshake_client_receiver_.is_bound() || client_receiver_.is_bound();
}

void QuicTransport::Trace(Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void QuicTransport::Init(ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Init() url=" << url_ << " this=" << this;
  if (!url_.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL '" + url_.ElidedString() + "' is invalid.");
    return;
  }

  if (!url_.ProtocolIs("quic-transport")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL's scheme must be 'quic-transport'. '" + url_.Protocol() +
            "' is not allowed.");
    return;
  }

  // TODO(ricea): Use the URL as-is once "quic-transport" it has been added to
  // the "special" schemes list.
  KURL url_as_https = ReparseURLAsHTTPS(url_);

  if (!url_as_https.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL '" + url_.ElidedString() + "' is invalid.");
    return;
  }

  if (url_as_https.HasFragmentIdentifier()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('#" +
            url_as_https.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in QuicTransport URLs.");
    return;
  }

  auto* execution_context = GetExecutionContext();

  if (!execution_context->GetContentSecurityPolicyForWorld()
           ->AllowConnectToSource(url_as_https)) {
    // TODO(ricea): This error should probably be asynchronous like it is for
    // WebSockets and fetch.
    exception_state.ThrowSecurityError(
        "Failed to connect to '" + url_.ElidedString() + "'",
        "Refused to connect to '" + url_.ElidedString() +
            "' because it violates the document's Content Security Policy");
    return;
  }

  // TODO(ricea): Register SchedulingPolicy so that we don't get throttled and
  // to disable bfcache. Must be done before shipping.

  // TODO(ricea): Check the SubresourceFilter and fail asynchronously if
  // disallowed. Must be done before shipping.

  mojo::Remote<mojom::blink::QuicTransportConnector> connector;
  auto* interface_provider = execution_context->GetInterfaceProvider();

  DCHECK(interface_provider);
  interface_provider->GetInterface(connector.BindNewPipeAndPassReceiver(
      execution_context->GetTaskRunner(TaskType::kNetworking)));

  connector->Connect(
      url_, handshake_client_receiver_.BindNewPipeAndPassRemote(
                execution_context->GetTaskRunner(TaskType::kNetworking)));

  handshake_client_receiver_.set_disconnect_handler(
      WTF::Bind(&QuicTransport::OnConnectionError, WrapWeakPersistent(this)));

  // TODO(ricea): Report something to devtools.
}

void QuicTransport::Dispose() {
  DVLOG(1) << "QuicTransport::Dispose() this=" << this;
  quic_transport_.reset();
  handshake_client_receiver_.reset();
  client_receiver_.reset();
}

void QuicTransport::OnConnectionError() {
  DVLOG(1) << "QuicTransport::OnConnectionError() this=" << this;
  Dispose();
}

}  // namespace blink
