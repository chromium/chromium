// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include <stdint.h>

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_quic_transport_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_dtls_fingerprint.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"
#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/send_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Creates a mojo DataPipe with the options we use for our stream data pipes. On
// success, returns true. On failure, throws an exception and returns false.
bool CreateStreamDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                          mojo::ScopedDataPipeConsumerHandle* consumer,
                          ExceptionState& exception_state) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  // TODO(ricea): Find an appropriate value for capacity_num_bytes.
  options.capacity_num_bytes = 0;

  MojoResult result = mojo::CreateDataPipe(&options, producer, consumer);
  if (result != MOJO_RESULT_OK) {
    // Probably out of resources.
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "Insufficient resources.");
    return false;
  }

  return true;
}

}  // namespace

// Sends a datagram on write().
class QuicTransport::DatagramUnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit DatagramUnderlyingSink(QuicTransport* quic_transport)
      : quic_transport_(quic_transport) {}

  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController*,
                      ExceptionState&) override {
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override {
    auto v8chunk = chunk.V8Value();
    if (v8chunk->IsArrayBuffer()) {
      DOMArrayBuffer* data =
          V8ArrayBuffer::ToImpl(v8chunk.As<v8::ArrayBuffer>());
      return SendDatagram(
          {static_cast<const uint8_t*>(data->Data()), data->ByteLength()});
    }

    auto* isolate = script_state->GetIsolate();
    if (v8chunk->IsArrayBufferView()) {
      NotShared<DOMArrayBufferView> data =
          ToNotShared<NotShared<DOMArrayBufferView>>(isolate, v8chunk,
                                                     exception_state);
      if (exception_state.HadException()) {
        return ScriptPromise();
      }

      return SendDatagram(
          {static_cast<const uint8_t*>(data.View()->buffer()->Data()) +
               data.View()->byteOffset(),
           data.View()->byteLength()});
    }

    exception_state.ThrowTypeError(
        "Datagram is not an ArrayBuffer or ArrayBufferView type.");
    return ScriptPromise();
  }

  ScriptPromise close(ScriptState* script_state, ExceptionState&) override {
    quic_transport_ = nullptr;
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState&) override {
    quic_transport_ = nullptr;
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(quic_transport_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  ScriptPromise SendDatagram(base::span<const uint8_t> data) {
    if (!quic_transport_->quic_transport_.is_bound()) {
      // Silently drop the datagram if we are not connected.
      // TODO(ricea): Change the behaviour if the standard changes. See
      // https://github.com/WICG/web-transport/issues/93.
      return ScriptPromise::CastUndefined(quic_transport_->script_state_);
    }

    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        quic_transport_->script_state_);
    quic_transport_->quic_transport_->SendDatagram(
        data, WTF::Bind(&DatagramSent, WrapPersistent(resolver)));
    return resolver->Promise();
  }

  // |sent| indicates whether the datagram was sent or dropped. Currently we
  // |don't do anything with this information.
  static void DatagramSent(ScriptPromiseResolver* resolver, bool sent) {
    resolver->Resolve();
  }

  Member<QuicTransport> quic_transport_;
};

// Captures a pointer to the ReadableStreamDefaultControllerWithScriptScope in
// the Start() method, and then does nothing else. Queuing of received datagrams
// is done inside the implementation of QuicTransport.
class QuicTransport::DatagramUnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  DatagramUnderlyingSource(ScriptState* script_state,
                           QuicTransport* quic_transport)
      : UnderlyingSourceBase(script_state), quic_transport_(quic_transport) {}

  ScriptPromise Start(ScriptState* script_state) override {
    quic_transport_->received_datagrams_controller_ = Controller();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise pull(ScriptState* script_state) override {
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    // Stop Enqueue() from being called again.

    quic_transport_->received_datagrams_controller_->NoteHasBeenCanceled();
    quic_transport_->received_datagrams_controller_ = nullptr;
    quic_transport_ = nullptr;
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(quic_transport_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  Member<QuicTransport> quic_transport_;
};

class QuicTransport::StreamVendingUnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  class StreamVendor : public GarbageCollected<StreamVendor> {
   public:
    using EnqueueCallback = base::OnceCallback<void(ScriptWrappable*)>;
    virtual void RequestStream(EnqueueCallback) = 0;
    virtual void Trace(Visitor*) const {}
  };

  template <class VendorType>
  static StreamVendingUnderlyingSource* CreateWithVendor(
      ScriptState* script_state,
      QuicTransport* quic_transport) {
    auto* vendor =
        MakeGarbageCollected<VendorType>(script_state, quic_transport);
    return MakeGarbageCollected<StreamVendingUnderlyingSource>(script_state,
                                                               vendor);
  }

  StreamVendingUnderlyingSource(ScriptState* script_state, StreamVendor* vendor)
      : UnderlyingSourceBase(script_state),
        script_state_(script_state),
        vendor_(vendor) {}

  ScriptPromise pull(ScriptState* script_state) override {
    if (!is_opened_) {
      is_pull_waiting_ = true;
      return ScriptPromise::CastUndefined(script_state);
    }

    vendor_->RequestStream(WTF::Bind(&StreamVendingUnderlyingSource::Enqueue,
                                     WrapWeakPersistent(this)));

    return ScriptPromise::CastUndefined(script_state);
  }

  // Used by QuicTransport to error the stream.
  void Error(v8::Local<v8::Value> reason) { Controller()->Error(reason); }

  // Used by QuicTransport to close the stream.
  void Close() { Controller()->Close(); }

  // Used by QuicTransport to notify that the QuicTransport interface is
  // available.
  void NotifyOpened() {
    is_opened_ = true;

    if (is_pull_waiting_) {
      ScriptState::Scope scope(script_state_);
      pull(script_state_);
      is_pull_waiting_ = false;
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(vendor_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  void Enqueue(ScriptWrappable* stream) { Controller()->Enqueue(stream); }

  const Member<ScriptState> script_state_;
  const Member<StreamVendor> vendor_;
  bool is_opened_ = false;
  bool is_pull_waiting_ = false;
};

class QuicTransport::ReceiveStreamVendor final
    : public QuicTransport::StreamVendingUnderlyingSource::StreamVendor {
 public:
  ReceiveStreamVendor(ScriptState* script_state, QuicTransport* quic_transport)
      : script_state_(script_state), quic_transport_(quic_transport) {}

  void RequestStream(EnqueueCallback enqueue) override {
    quic_transport_->quic_transport_->AcceptUnidirectionalStream(
        WTF::Bind(&ReceiveStreamVendor::OnAcceptUnidirectionalStreamResponse,
                  WrapWeakPersistent(this), std::move(enqueue)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(quic_transport_);
    StreamVendor::Trace(visitor);
  }

 private:
  void OnAcceptUnidirectionalStreamResponse(
      EnqueueCallback enqueue,
      uint32_t stream_id,
      mojo::ScopedDataPipeConsumerHandle readable) {
    ScriptState::Scope scope(script_state_);
    auto* receive_stream = MakeGarbageCollected<ReceiveStream>(
        script_state_, quic_transport_, stream_id, std::move(readable));
    receive_stream->Init();
    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    quic_transport_->stream_map_.insert(stream_id, receive_stream);

    std::move(enqueue).Run(receive_stream);
  }

  const Member<ScriptState> script_state_;
  const Member<QuicTransport> quic_transport_;
};

class QuicTransport::BidirectionalStreamVendor final
    : public QuicTransport::StreamVendingUnderlyingSource::StreamVendor {
 public:
  BidirectionalStreamVendor(ScriptState* script_state,
                            QuicTransport* quic_transport)
      : script_state_(script_state), quic_transport_(quic_transport) {}

  void RequestStream(EnqueueCallback enqueue) override {
    quic_transport_->quic_transport_->AcceptBidirectionalStream(WTF::Bind(
        &BidirectionalStreamVendor::OnAcceptBidirectionalStreamResponse,
        WrapWeakPersistent(this), std::move(enqueue)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(quic_transport_);
    StreamVendor::Trace(visitor);
  }

 private:
  void OnAcceptBidirectionalStreamResponse(
      EnqueueCallback enqueue,
      uint32_t stream_id,
      mojo::ScopedDataPipeConsumerHandle incoming_consumer,
      mojo::ScopedDataPipeProducerHandle outgoing_producer) {
    ScriptState::Scope scope(script_state_);
    auto* bidirectional_stream = MakeGarbageCollected<BidirectionalStream>(
        script_state_, quic_transport_, stream_id, std::move(outgoing_producer),
        std::move(incoming_consumer));
    bidirectional_stream->Init();
    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    quic_transport_->stream_map_.insert(stream_id, bidirectional_stream);

    std::move(enqueue).Run(bidirectional_stream);
  }

  const Member<ScriptState> script_state_;
  const Member<QuicTransport> quic_transport_;
};

QuicTransport* QuicTransport::Create(ScriptState* script_state,
                                     const String& url,
                                     QuicTransportOptions* options,
                                     ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Create() url=" << url;
  DCHECK(options);
  ExecutionContext::From(script_state)->CountUse(WebFeature::kQuicTransport);
  auto* transport =
      MakeGarbageCollected<QuicTransport>(PassKey(), script_state, url);
  transport->Init(url, *options, exception_state);
  return transport;
}

QuicTransport::QuicTransport(PassKey,
                             ScriptState* script_state,
                             const String& url)
    : QuicTransport(script_state, url, ExecutionContext::From(script_state)) {}

QuicTransport::QuicTransport(ScriptState* script_state,
                             const String& url,
                             ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      script_state_(script_state),
      url_(NullURL(), url),
      quic_transport_(context),
      handshake_client_receiver_(this, context),
      client_receiver_(this, context) {}

ScriptPromise QuicTransport::createSendStream(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::createSendStream() this=" << this;

  GetExecutionContext()->CountUse(WebFeature::kQuicTransportStreamApis);
  if (!quic_transport_.is_bound()) {
    // TODO(ricea): Should we wait if we're still connecting?
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return ScriptPromise();
  }

  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;

  if (!CreateStreamDataPipe(&data_pipe_producer, &data_pipe_consumer,
                            exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  create_stream_resolvers_.insert(resolver);
  quic_transport_->CreateStream(
      std::move(data_pipe_consumer), mojo::ScopedDataPipeProducerHandle(),
      WTF::Bind(&QuicTransport::OnCreateSendStreamResponse,
                WrapWeakPersistent(this), WrapWeakPersistent(resolver),
                std::move(data_pipe_producer)));

  return resolver->Promise();
}

ReadableStream* QuicTransport::receiveStreams() {
  GetExecutionContext()->CountUse(WebFeature::kQuicTransportStreamApis);
  return received_streams_;
}

ScriptPromise QuicTransport::createBidirectionalStream(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::createBidirectionalStream() this=" << this;

  GetExecutionContext()->CountUse(WebFeature::kQuicTransportStreamApis);
  if (!quic_transport_.is_bound()) {
    // TODO(ricea): We should wait if we are still connecting.
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return ScriptPromise();
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  // TODO(ricea): Find an appropriate value for capacity_num_bytes.
  options.capacity_num_bytes = 0;

  mojo::ScopedDataPipeProducerHandle outgoing_producer;
  mojo::ScopedDataPipeConsumerHandle outgoing_consumer;
  if (!CreateStreamDataPipe(&outgoing_producer, &outgoing_consumer,
                            exception_state)) {
    return ScriptPromise();
  }

  mojo::ScopedDataPipeProducerHandle incoming_producer;
  mojo::ScopedDataPipeConsumerHandle incoming_consumer;
  if (!CreateStreamDataPipe(&incoming_producer, &incoming_consumer,
                            exception_state)) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  create_stream_resolvers_.insert(resolver);
  quic_transport_->CreateStream(
      std::move(outgoing_consumer), std::move(incoming_producer),
      WTF::Bind(&QuicTransport::OnCreateBidirectionalStreamResponse,
                WrapWeakPersistent(this), WrapWeakPersistent(resolver),
                std::move(outgoing_producer), std::move(incoming_consumer)));

  return resolver->Promise();
}

ReadableStream* QuicTransport::receiveBidirectionalStreams() {
  GetExecutionContext()->CountUse(WebFeature::kQuicTransportStreamApis);
  return received_bidirectional_streams_;
}

WritableStream* QuicTransport::sendDatagrams() {
  GetExecutionContext()->CountUse(WebFeature::kQuicTransportDatagramApis);
  return outgoing_datagrams_;
}

ReadableStream* QuicTransport::receiveDatagrams() {
  GetExecutionContext()->CountUse(WebFeature::kQuicTransportDatagramApis);
  return received_datagrams_;
}

void QuicTransport::close(const WebTransportCloseInfo* close_info) {
  DVLOG(1) << "QuicTransport::close() this=" << this;
  // TODO(ricea): Send |close_info| to the network service.

  if (cleanly_closed_) {
    // close() has already been called. Ignore it.
    return;
  }
  cleanly_closed_ = true;

  if (received_datagrams_controller_) {
    received_datagrams_controller_->Close();
    received_datagrams_controller_ = nullptr;
  }

  received_streams_underlying_source_->Close();
  received_bidirectional_streams_underlying_source_->Close();

  // If we don't manage to close the writable stream here, then it will
  // error when a write() is attempted.
  if (!WritableStream::IsLocked(outgoing_datagrams_) &&
      !WritableStream::CloseQueuedOrInFlight(outgoing_datagrams_)) {
    auto promise = WritableStream::Close(script_state_, outgoing_datagrams_);
    promise->MarkAsHandled();
  }
  closed_resolver_->Resolve(close_info);

  v8::Local<v8::Value> reason = V8ThrowException::CreateTypeError(
      script_state_->GetIsolate(), "Connection closed.");
  ready_resolver_->Reject(reason);
  RejectPendingStreamResolvers();
  ResetAll();
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

  DCHECK(!quic_transport_.is_bound());
  quic_transport_.Bind(std::move(quic_transport), task_runner);

  received_streams_underlying_source_->NotifyOpened();
  received_bidirectional_streams_underlying_source_->NotifyOpened();

  ready_resolver_->Resolve();
}

QuicTransport::~QuicTransport() = default;

void QuicTransport::OnHandshakeFailed(
    network::mojom::blink::QuicTransportErrorPtr error) {
  // |error| should be null from security/privacy reasons.
  DCHECK(!error);
  DVLOG(1) << "QuicTransport::OnHandshakeFailed() this=" << this;
  ScriptState::Scope scope(script_state_);
  {
    v8::Local<v8::Value> reason = V8ThrowException::CreateTypeError(
        script_state_->GetIsolate(), "Connection lost.");
    ready_resolver_->Reject(reason);
    closed_resolver_->Reject(reason);
  }
  ResetAll();
}

void QuicTransport::OnDatagramReceived(base::span<const uint8_t> data) {
  ReadableStreamDefaultControllerWithScriptScope* controller =
      received_datagrams_controller_;

  // Discard datagrams if the readable has been cancelled.
  if (!controller)
    return;

  // The spec says we should discard older datagrams first, but that's not what
  // ReadableStream does, so instead we might need to maintain a separate queue
  // with the desired semantics. But for now we'll just use a small queue in
  // ReadableStream.
  // TODO(ricea): Figure out how to get nice semantics here.

  if (controller->DesiredSize() > 0) {
    auto* array = DOMUint8Array::Create(
        data.data(), base::checked_cast<wtf_size_t>(data.size()));
    controller->Enqueue(array);
  }
}

void QuicTransport::OnIncomingStreamClosed(uint32_t stream_id,
                                           bool fin_received) {
  DVLOG(1) << "QuicTransport::OnIncomingStreamClosed(" << stream_id << ", "
           << fin_received << ") this=" << this;
  WebTransportStream* stream = stream_map_.at(stream_id);
  // |stream| can be unset because of races between different ways of closing
  // |bidirectional streams.
  if (stream) {
    stream->OnIncomingStreamClosed(fin_received);
  }
}

void QuicTransport::ContextDestroyed() {
  DVLOG(1) << "QuicTransport::ContextDestroyed() this=" << this;
  // Child streams must be reset first to ensure that garbage collection
  // ordering is safe. ContextDestroyed() is required not to execute JavaScript,
  // so this loop will not be re-entered.
  for (WebTransportStream* stream : stream_map_.Values()) {
    stream->ContextDestroyed();
  }
  Dispose();
}

bool QuicTransport::HasPendingActivity() const {
  DVLOG(1) << "QuicTransport::HasPendingActivity() this=" << this;
  return handshake_client_receiver_.is_bound() || client_receiver_.is_bound();
}

void QuicTransport::SendFin(uint32_t stream_id) {
  quic_transport_->SendFin(stream_id);
}

void QuicTransport::AbortStream(uint32_t stream_id) {
  quic_transport_->AbortStream(stream_id, /*code=*/0);
}

void QuicTransport::ForgetStream(uint32_t stream_id) {
  stream_map_.erase(stream_id);
}

void QuicTransport::Trace(Visitor* visitor) const {
  visitor->Trace(received_datagrams_);
  visitor->Trace(received_datagrams_controller_);
  visitor->Trace(outgoing_datagrams_);
  visitor->Trace(script_state_);
  visitor->Trace(create_stream_resolvers_);
  visitor->Trace(quic_transport_);
  visitor->Trace(handshake_client_receiver_);
  visitor->Trace(client_receiver_);
  visitor->Trace(ready_resolver_);
  visitor->Trace(ready_);
  visitor->Trace(closed_resolver_);
  visitor->Trace(closed_);
  visitor->Trace(stream_map_);
  visitor->Trace(received_streams_);
  visitor->Trace(received_streams_underlying_source_);
  visitor->Trace(received_bidirectional_streams_);
  visitor->Trace(received_bidirectional_streams_underlying_source_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void QuicTransport::Init(const String& url,
                         const QuicTransportOptions& options,
                         ExceptionState& exception_state) {
  DVLOG(1) << "QuicTransport::Init() url=" << url << " this=" << this;
  if (!url_.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The URL '" + url + "' is invalid.");
    return;
  }

  if (!url_.ProtocolIs("quic-transport")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL's scheme must be 'quic-transport'. '" + url_.Protocol() +
            "' is not allowed.");
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('#" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in QuicTransport URLs.");
    return;
  }

  ready_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  ready_ = ready_resolver_->Promise();

  closed_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  closed_ = closed_resolver_->Promise();

  auto* execution_context = GetExecutionContext();

  if (!execution_context->GetContentSecurityPolicyForCurrentWorld()
           ->AllowConnectToSource(url_, url_, RedirectStatus::kNoRedirect)) {
    // TODO(ricea): This error should probably be asynchronous like it is for
    // WebSockets and fetch.
    exception_state.ThrowSecurityError(
        "Failed to connect to '" + url_.ElidedString() + "'",
        "Refused to connect to '" + url_.ElidedString() +
            "' because it violates the document's Content Security Policy");
    return;
  }

  Vector<network::mojom::blink::QuicTransportCertificateFingerprintPtr>
      fingerprints;
  if (options.hasServerCertificateFingerprints()) {
    for (const auto& fingerprint : options.serverCertificateFingerprints()) {
      fingerprints.push_back(
          network::mojom::blink::QuicTransportCertificateFingerprint::New(
              fingerprint->algorithm(), fingerprint->value()));
    }
  }

  // TODO(ricea): Register SchedulingPolicy so that we don't get throttled and
  // to disable bfcache. Must be done before shipping.

  // TODO(ricea): Check the SubresourceFilter and fail asynchronously if
  // disallowed. Must be done before shipping.

  mojo::Remote<mojom::blink::QuicTransportConnector> connector;
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      connector.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));

  connector->Connect(
      url_, std::move(fingerprints),
      handshake_client_receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kNetworking)));

  handshake_client_receiver_.set_disconnect_handler(
      WTF::Bind(&QuicTransport::OnConnectionError, WrapWeakPersistent(this)));

  // TODO(ricea): Report something to devtools.

  // The choice of 1 for the ReadableStream means that it will queue one
  // datagram even when read() is not being called. Unfortunately, that datagram
  // may become arbitrarily stale.
  // TODO(ricea): Consider having a datagram queue inside this class instead.
  received_datagrams_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_,
      MakeGarbageCollected<DatagramUnderlyingSource>(script_state_, this), 1);
  outgoing_datagrams_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_, MakeGarbageCollected<DatagramUnderlyingSink>(this), 1);

  received_streams_underlying_source_ =
      StreamVendingUnderlyingSource::CreateWithVendor<ReceiveStreamVendor>(
          script_state_, this);
  received_streams_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_, received_streams_underlying_source_, 1);

  received_bidirectional_streams_underlying_source_ =
      StreamVendingUnderlyingSource::CreateWithVendor<
          BidirectionalStreamVendor>(script_state_, this);

  received_bidirectional_streams_ =
      ReadableStream::CreateWithCountQueueingStrategy(
          script_state_, received_bidirectional_streams_underlying_source_, 1);
}

void QuicTransport::ResetAll() {
  DVLOG(1) << "QuicTransport::ResetAll() this=" << this;

  // This loop is safe even if re-entered. It will always terminate because
  // every iteration erases one entry from the map.
  while (!stream_map_.IsEmpty()) {
    auto it = stream_map_.begin();
    auto close_proxy = it->value;
    stream_map_.erase(it);
    close_proxy->Reset();
  }
  Dispose();
}

void QuicTransport::Dispose() {
  DVLOG(1) << "QuicTransport::Dispose() this=" << this;
  stream_map_.clear();
  quic_transport_.reset();
  handshake_client_receiver_.reset();
  client_receiver_.reset();
}

void QuicTransport::OnConnectionError() {
  DVLOG(1) << "QuicTransport::OnConnectionError() this=" << this;

  ScriptState::Scope scope(script_state_);
  if (!cleanly_closed_) {
    v8::Local<v8::Value> reason = V8ThrowException::CreateTypeError(
        script_state_->GetIsolate(), "Connection lost.");
    if (received_datagrams_controller_) {
      received_datagrams_controller_->Error(reason);
      received_datagrams_controller_ = nullptr;
    }
    received_streams_underlying_source_->Error(reason);
    received_bidirectional_streams_underlying_source_->Error(reason);
    WritableStreamDefaultController::ErrorIfNeeded(
        script_state_, outgoing_datagrams_->Controller(), reason);
    ready_resolver_->Reject(reason);
    closed_resolver_->Reject(reason);
  }

  RejectPendingStreamResolvers();
  ResetAll();
}

void QuicTransport::RejectPendingStreamResolvers() {
  v8::Local<v8::Value> reason = V8ThrowException::CreateTypeError(
      script_state_->GetIsolate(), "Connection lost.");
  for (ScriptPromiseResolver* resolver : create_stream_resolvers_) {
    resolver->Reject(reason);
  }
  create_stream_resolvers_.clear();
}

void QuicTransport::OnCreateSendStreamResponse(
    ScriptPromiseResolver* resolver,
    mojo::ScopedDataPipeProducerHandle producer,
    bool succeeded,
    uint32_t stream_id) {
  DVLOG(1) << "QuicTransport::OnCreateSendStreamResponse() this=" << this
           << " succeeded=" << succeeded << " stream_id=" << stream_id;

  // Shouldn't resolve the promise if the execution context has gone away.
  if (!GetExecutionContext())
    return;

  // Shouldn't resolve the promise if the mojo interface is disconnected.
  if (!resolver || !create_stream_resolvers_.Take(resolver))
    return;

  ScriptState::Scope scope(script_state_);
  if (!succeeded) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kNetworkError,
        "Failed to create send stream."));
    return;
  }

  auto* send_stream = MakeGarbageCollected<SendStream>(
      script_state_, this, stream_id, std::move(producer));
  send_stream->Init();

  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);
  stream_map_.insert(stream_id, send_stream);

  resolver->Resolve(send_stream);
}

void QuicTransport::OnCreateBidirectionalStreamResponse(
    ScriptPromiseResolver* resolver,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer,
    bool succeeded,
    uint32_t stream_id) {
  DVLOG(1) << "QuicTransport::OnCreateBidirectionalStreamResponse() this="
           << this << " succeeded=" << succeeded << " stream_id=" << stream_id;

  // Shouldn't resolve the promise if the execution context has gone away.
  if (!GetExecutionContext())
    return;

  // Shouldn't resolve the promise if the mojo interface is disconnected.
  if (!resolver || !create_stream_resolvers_.Take(resolver))
    return;

  ScriptState::Scope scope(script_state_);
  if (!succeeded) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kNetworkError,
        "Failed to create bidirectional stream."));
    return;
  }

  auto* bidirectional_stream = MakeGarbageCollected<BidirectionalStream>(
      script_state_, this, stream_id, std::move(outgoing_producer),
      std::move(incoming_consumer));
  bidirectional_stream->Init();

  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);
  stream_map_.insert(stream_id, bidirectional_stream);

  resolver->Resolve(bidirectional_stream);
}

}  // namespace blink
