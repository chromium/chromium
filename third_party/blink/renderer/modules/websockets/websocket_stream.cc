// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_stream.h"

#include <memory>
#include <string>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_open_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_websocket_stream_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"
#include "third_party/blink/renderer/modules/websockets/websocket_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

// This is used in several places, so use a constant to avoid typos.
constexpr char kWebSocketNotCleanlyClosedErrorMessage[] =
    "WebSocket was not cleanly closed.";

}  // namespace

class WebSocketStream::UnderlyingSource final : public UnderlyingSourceBase {
 public:
  UnderlyingSource(ScriptState* script_state, WebSocketStream* creator)
      : UnderlyingSourceBase(script_state), creator_(creator) {}

  // UnderlyingSourceBase implementation.
  ScriptPromiseUntyped Pull(ScriptState*, ExceptionState&) override;
  ScriptPromiseUntyped Cancel(ScriptState*,
                              ScriptValue reason,
                              ExceptionState&) override;

  // API for WebSocketStream.
  void DidReceiveTextMessage(const String&);
  void DidReceiveBinaryMessage(const Vector<base::span<const char>>&);
  void DidStartClosingHandshake();
  void DidCloseCleanly(uint16_t code, const String& reason);
  void CloseWithError(v8::Local<v8::Value> error);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(creator_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  Member<WebSocketStream> creator_;
  bool closed_ = false;
};

class WebSocketStream::UnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit UnderlyingSink(WebSocketStream* creator) : creator_(creator) {}

  // UnderlyingSinkBase implementation.
  ScriptPromise<IDLUndefined> start(ScriptState*,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override;
  ScriptPromise<IDLUndefined> write(ScriptState*,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override;
  ScriptPromise<IDLUndefined> close(ScriptState*, ExceptionState&) override;
  ScriptPromise<IDLUndefined> abort(ScriptState*,
                                    ScriptValue reason,
                                    ExceptionState&) override;

  // API for WebSocketStream.
  void DidStartClosingHandshake();
  void DidCloseCleanly(uint16_t code, const String& reason);
  void CloseWithError(v8::Local<v8::Value> error);
  bool AllDataHasBeenConsumed() { return !is_writing_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(creator_);
    visitor->Trace(close_resolver_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  void ErrorControllerBecauseClosed();
  void FinishWriteCallback(ScriptPromiseResolver<IDLUndefined>*);
  void ResolveClose(bool was_clean);
  void SendArrayBuffer(ScriptState*,
                       DOMArrayBuffer*,
                       size_t offset,
                       size_t length,
                       ScriptPromiseResolver<IDLUndefined>*,
                       base::OnceClosure callback);
  void SendString(ScriptState*,
                  v8::Local<v8::Value> v8chunk,
                  ScriptPromiseResolver<IDLUndefined>*,
                  base::OnceClosure callback);

  Member<WebSocketStream> creator_;
  Member<ScriptPromiseResolver<IDLUndefined>> close_resolver_;
  bool closed_ = false;
  bool is_writing_ = false;
};

ScriptPromiseUntyped WebSocketStream::UnderlyingSource::Pull(
    ScriptState* script_state,
    ExceptionState&) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this << " Pull()";
  creator_->channel_->RemoveBackpressure();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromiseUntyped WebSocketStream::UnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this << " Cancel()";
  closed_ = true;
  creator_->CloseMaybeWithReason(reason, exception_state);
  return ToResolvedUndefinedPromise(script_state);
}

void WebSocketStream::UnderlyingSource::DidReceiveTextMessage(
    const String& string) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this
           << " DidReceiveTextMessage() string=" << string;

  DCHECK(!closed_);
  Controller()->Enqueue(
      V8String(creator_->script_state_->GetIsolate(), string));
  creator_->channel_->ApplyBackpressure();
}

void WebSocketStream::UnderlyingSource::DidReceiveBinaryMessage(
    const Vector<base::span<const char>>& data) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this
           << " DidReceiveBinaryMessage()";

  DCHECK(!closed_);
  auto* buffer = DOMArrayBuffer::Create(data);
  Controller()->Enqueue(buffer);
  creator_->channel_->ApplyBackpressure();
}

void WebSocketStream::UnderlyingSource::DidStartClosingHandshake() {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this
           << " DidStartClosingHandshake()";

  DCHECK(!closed_);
  Controller()->Close();
  closed_ = true;
}

void WebSocketStream::UnderlyingSource::DidCloseCleanly(uint16_t code,
                                                        const String& reason) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource " << this
           << " DidCloseCleanly() code=" << code << " reason=" << reason;

  if (closed_)
    return;

  closed_ = true;
  Controller()->Close();
}

void WebSocketStream::UnderlyingSource::CloseWithError(
    v8::Local<v8::Value> error) {
  DVLOG(1) << "WebSocketStream::UnderlyingSource::CloseWithError";
  if (closed_) {
    return;
  }

  closed_ = true;

  Controller()->Error(error);
}

ScriptPromise<IDLUndefined> WebSocketStream::UnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController*,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this << " start()";
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> WebSocketStream::UnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController*,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this << " write()";
  is_writing_ = true;

  v8::Local<v8::Value> v8chunk = chunk.V8Value();
  auto* isolate = script_state->GetIsolate();
  DOMArrayBuffer* data = nullptr;
  size_t offset = 0;
  size_t length = 0;
  if (v8chunk->IsArrayBuffer()) {
    data = NativeValueTraits<DOMArrayBuffer>::NativeValue(isolate, v8chunk,
                                                          exception_state);
    if (exception_state.HadException()) {
      closed_ = true;
      is_writing_ = false;
      return EmptyPromise();
    }
    length = data->ByteLength();
  } else if (v8chunk->IsArrayBufferView()) {
    NotShared<DOMArrayBufferView> data_view =
        NativeValueTraits<NotShared<DOMArrayBufferView>>::NativeValue(
            isolate, v8chunk, exception_state);
    if (exception_state.HadException()) {
      closed_ = true;
      is_writing_ = false;
      return EmptyPromise();
    }
    data = data_view->buffer();
    offset = data_view->byteOffset();
    length = data_view->byteLength();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto result = resolver->Promise();
  base::OnceClosure callback =
      WTF::BindOnce(&UnderlyingSink::FinishWriteCallback,
                    WrapWeakPersistent(this), WrapPersistent(resolver));
  if (data) {
    SendArrayBuffer(script_state, data, offset, length, resolver,
                    std::move(callback));
  } else {
    SendString(script_state, v8chunk, resolver, std::move(callback));
  }
  return result;
}

ScriptPromise<IDLUndefined> WebSocketStream::UnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this << " close()";
  closed_ = true;
  creator_->CloseWithUnspecifiedCode(exception_state);
  DCHECK(!close_resolver_);
  close_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  return close_resolver_->Promise();
}

ScriptPromise<IDLUndefined> WebSocketStream::UnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this << " abort()";

  closed_ = true;
  creator_->CloseMaybeWithReason(reason, exception_state);
  return ToResolvedUndefinedPromise(script_state);
}

void WebSocketStream::UnderlyingSink::DidStartClosingHandshake() {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this
           << " DidStartClosingHandshake()";

  if (closed_)
    return;
  closed_ = true;

  ErrorControllerBecauseClosed();
}

void WebSocketStream::UnderlyingSink::DidCloseCleanly(uint16_t code,
                                                      const String& reason) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this
           << " DidCloseCleanly() code=" << code << " reason=" << reason;

  if (close_resolver_) {
    ResolveClose(/*was_clean=*/true);
  }

  if (closed_)
    return;
  closed_ = true;

  ErrorControllerBecauseClosed();
}

void WebSocketStream::UnderlyingSink::CloseWithError(
    v8::Local<v8::Value> error) {
  if (close_resolver_) {
    ResolveClose(/*was_clean=*/false);
  }

  if (closed_) {
    return;
  }
  closed_ = true;

  ScriptState* script_state = creator_->script_state_;
  Controller()->error(script_state,
                      ScriptValue(script_state->GetIsolate(), error));
}

void WebSocketStream::UnderlyingSink::ErrorControllerBecauseClosed() {
  ScriptState* script_state = creator_->script_state_;
  Controller()->error(
      script_state,
      ScriptValue(
          script_state->GetIsolate(),
          V8ThrowDOMException::CreateOrEmpty(
              script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
              "Cannot write to a closed WebSocketStream")));
}

void WebSocketStream::UnderlyingSink::FinishWriteCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this
           << " FinishWriteCallback()";

  resolver->Resolve();
  is_writing_ = false;
}

void WebSocketStream::UnderlyingSink::ResolveClose(bool was_clean) {
  DCHECK(close_resolver_);

  if (was_clean) {
    close_resolver_->Resolve();
    return;
  }

  close_resolver_->Reject(
      creator_->CreateWebSocketError(kWebSocketNotCleanlyClosedErrorMessage));
}

void WebSocketStream::UnderlyingSink::SendArrayBuffer(
    ScriptState* script_state,
    DOMArrayBuffer* buffer,
    size_t offset,
    size_t length,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    base::OnceClosure callback) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this
           << " SendArrayBuffer() buffer = " << buffer << " offset = " << offset
           << " length = " << length;

  if (creator_->channel_->Send(*buffer, offset, length, std::move(callback)) ==
      WebSocketChannel::SendResult::kSentSynchronously) {
    is_writing_ = false;
    resolver->Resolve();
  }
}

void WebSocketStream::UnderlyingSink::SendString(
    ScriptState* script_state,
    v8::Local<v8::Value> v8chunk,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    base::OnceClosure callback) {
  DVLOG(1) << "WebSocketStream::UnderlyingSink " << this << " SendString()";
  auto* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> string_chunk;
  if (!v8chunk->ToString(script_state->GetContext()).ToLocal(&string_chunk)) {
    closed_ = true;
    resolver->Reject(try_catch.Exception());
    is_writing_ = false;
    return;
  }
  // Skip one string copy by using v8::String UTF8 conversion instead of going
  // via WTF::String.
  int expected_length = string_chunk->Utf8Length(isolate) + 1;
  std::string message(expected_length, '\0');
  int written_length = string_chunk->WriteUtf8(
      isolate, &message[0], -1, nullptr, v8::String::REPLACE_INVALID_UTF8);
  DCHECK_EQ(expected_length, written_length);
  DCHECK_GT(expected_length, 0);
  DCHECK_EQ(message.back(), '\0');
  message.pop_back();  // Remove the null terminator.
  if (creator_->channel_->Send(message, std::move(callback)) ==
      WebSocketChannel::SendResult::kSentSynchronously) {
    is_writing_ = false;
    resolver->Resolve();
  }
}

WebSocketStream* WebSocketStream::Create(ScriptState* script_state,
                                         const String& url,
                                         WebSocketStreamOptions* options,
                                         ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream Create() url=" << url << " options=" << options;
  return CreateInternal(script_state, url, options, nullptr, exception_state);
}

WebSocketStream* WebSocketStream::CreateForTesting(
    ScriptState* script_state,
    const String& url,
    WebSocketStreamOptions* options,
    WebSocketChannel* channel,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream CreateForTesting() url=" << url
           << " options=" << options << " channel=" << channel;

  DCHECK(channel) << "Don't use a real channel when testing";
  return CreateInternal(script_state, url, options, channel, exception_state);
}

WebSocketStream* WebSocketStream::CreateInternal(
    ScriptState* script_state,
    const String& url,
    WebSocketStreamOptions* options,
    WebSocketChannel* channel,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream CreateInternal() url=" << url
           << " options=" << options << " channel=" << channel;

  auto* execution_context = ExecutionContext::From(script_state);
  auto* stream =
      MakeGarbageCollected<WebSocketStream>(execution_context, script_state);
  if (channel) {
    stream->channel_ = channel;
  } else {
    stream->channel_ = WebSocketChannelImpl::Create(
        execution_context, stream, CaptureSourceLocation(execution_context));
  }
  stream->Connect(script_state, url, options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return stream;
}

WebSocketStream::WebSocketStream(ExecutionContext* execution_context,
                                 ScriptState* script_state)
    : ActiveScriptWrappable<WebSocketStream>({}),
      ExecutionContextLifecycleObserver(execution_context),
      script_state_(script_state),
      opened_(MakeGarbageCollected<
              ScriptPromiseProperty<WebSocketOpenInfo, IDLAny>>(
          GetExecutionContext())),
      closed_(MakeGarbageCollected<
              ScriptPromiseProperty<WebSocketCloseInfo, IDLAny>>(
          GetExecutionContext())) {}

WebSocketStream::~WebSocketStream() = default;

ScriptPromise<WebSocketOpenInfo> WebSocketStream::opened(
    ScriptState* script_state) const {
  return opened_->Promise(script_state->World());
}

ScriptPromise<WebSocketCloseInfo> WebSocketStream::closed(
    ScriptState* script_state) const {
  return closed_->Promise(script_state->World());
}

void WebSocketStream::close(WebSocketCloseInfo* info,
                            ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream " << this << " close() info=" << info;
  CloseInternal(info->hasCloseCode() ? std::make_optional(info->closeCode())
                                     : std::nullopt,
                info->reason(), exception_state);
}

void WebSocketStream::DidConnect(const String& subprotocol,
                                 const String& extensions) {
  DVLOG(1) << "WebSocketStream " << this
           << " DidConnect() subprotocol=" << subprotocol
           << " extensions=" << extensions;

  if (!channel_)
    return;

  ScriptState::Scope scope(script_state_);
  if (common_.GetState() != WebSocketCommon::kConnecting)
    return;
  common_.SetState(WebSocketCommon::kOpen);
  was_ever_connected_ = true;
  auto* open_info = MakeGarbageCollected<WebSocketOpenInfo>();
  open_info->setProtocol(subprotocol);
  open_info->setExtensions(extensions);
  source_ = MakeGarbageCollected<UnderlyingSource>(script_state_, this);
  auto* readable = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_, source_, 1);
  sink_ = MakeGarbageCollected<UnderlyingSink>(this);
  auto* writable =
      WritableStream::CreateWithCountQueueingStrategy(script_state_, sink_, 1);
  open_info->setReadable(readable);
  open_info->setWritable(writable);
  opened_->Resolve(open_info);
  abort_handle_.Clear();
}

void WebSocketStream::DidReceiveTextMessage(const String& string) {
  DVLOG(1) << "WebSocketStream " << this
           << " DidReceiveTextMessage() string=" << string;

  if (!channel_)
    return;

  ScriptState::Scope scope(script_state_);
  source_->DidReceiveTextMessage(string);
}

void WebSocketStream::DidReceiveBinaryMessage(
    const Vector<base::span<const char>>& data) {
  DVLOG(1) << "WebSocketStream " << this << " DidReceiveBinaryMessage()";
  if (!channel_)
    return;

  ScriptState::Scope scope(script_state_);
  source_->DidReceiveBinaryMessage(data);
}

void WebSocketStream::DidError() {
  // This is not useful as it is always followed by a call to DidClose().
}

void WebSocketStream::DidConsumeBufferedAmount(uint64_t consumed) {
  // This is only relevant to DOMWebSocket.
}

void WebSocketStream::DidStartClosingHandshake() {
  DVLOG(1) << "WebSocketStream " << this << " DidStartClosingHandshake()";
  if (!channel_)
    return;

  ScriptState::Scope scope(script_state_);
  common_.SetState(WebSocketCommon::kClosing);
  source_->DidStartClosingHandshake();
  sink_->DidStartClosingHandshake();
}

void WebSocketStream::DidClose(
    ClosingHandshakeCompletionStatus closing_handshake_completion,
    uint16_t code,
    const String& reason) {
  DVLOG(1) << "WebSocketStream " << this
           << " DidClose() closing_handshake_completion="
           << closing_handshake_completion << " code=" << code
           << " reason=" << reason;

  if (!channel_)
    return;

  ScriptState::Scope scope(script_state_);
  if (!was_ever_connected_) {
    opened_->Reject(ScriptValue(
        script_state_->GetIsolate(),
        CreateWebSocketError("WebSocket closed before handshake complete.")));
  }
  bool all_data_was_consumed = sink_ ? sink_->AllDataHasBeenConsumed() : true;
  bool was_clean = common_.GetState() == WebSocketCommon::kClosing &&
                   all_data_was_consumed &&
                   closing_handshake_completion == kClosingHandshakeComplete &&
                   code != WebSocketChannel::kCloseEventCodeAbnormalClosure;
  common_.SetState(WebSocketCommon::kClosed);

  channel_->Disconnect();
  channel_ = nullptr;
  abort_handle_.Clear();
  if (was_clean) {
    if (source_) {
      source_->DidCloseCleanly(code, reason);
    }
    if (sink_) {
      sink_->DidCloseCleanly(code, reason);
    }
    closed_->Resolve(MakeCloseInfo(code, reason));
  } else {
    auto error = CreateWebSocketError(kWebSocketNotCleanlyClosedErrorMessage,
                                      code, reason);
    if (source_) {
      source_->CloseWithError(error);
    }
    if (sink_) {
      sink_->CloseWithError(error);
    }
    closed_->Reject(ScriptValue(script_state_->GetIsolate(), error));
  }
}

void WebSocketStream::ContextDestroyed() {
  DVLOG(1) << "WebSocketStream " << this << " ContextDestroyed()";
  if (channel_) {
    channel_ = nullptr;
  }
  if (common_.GetState() != WebSocketCommon::kClosed) {
    common_.SetState(WebSocketCommon::kClosed);
  }
  abort_handle_.Clear();
}

bool WebSocketStream::HasPendingActivity() const {
  return channel_ != nullptr;
}

void WebSocketStream::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(opened_);
  visitor->Trace(closed_);
  visitor->Trace(channel_);
  visitor->Trace(source_);
  visitor->Trace(sink_);
  visitor->Trace(abort_handle_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  WebSocketChannelClient::Trace(visitor);
}

void WebSocketStream::Connect(ScriptState* script_state,
                              const String& url,
                              WebSocketStreamOptions* options,
                              ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream " << this << " Connect() url=" << url
           << " options=" << options;

  // Don't read all of a huge initial message before read() has been called.
  channel_->ApplyBackpressure();

  if (options->hasSignal()) {
    auto* signal = options->signal();
    if (signal->aborted()) {
      auto exception = V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kAbortError,
          "WebSocket handshake was aborted");
      opened_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
      closed_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
      return;
    }

    abort_handle_ = signal->AddAlgorithm(
        WTF::BindOnce(&WebSocketStream::OnAbort, WrapWeakPersistent(this)));
  }

  auto result = common_.Connect(
      ExecutionContext::From(script_state), url,
      options->hasProtocols() ? options->protocols() : Vector<String>(),
      channel_, exception_state);

  switch (result) {
    case WebSocketCommon::ConnectResult::kSuccess:
      DCHECK(!exception_state.HadException());
      return;

    case WebSocketCommon::ConnectResult::kException:
      DCHECK(exception_state.HadException());
      channel_ = nullptr;
      return;

    case WebSocketCommon::ConnectResult::kAsyncError:
      DCHECK(!exception_state.HadException());
      auto exception = V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kSecurityError,
          "An attempt was made to break through the security policy of the "
          "user agent.",
          "WebSocket mixed content check failed.");
      opened_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
      closed_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
      return;
  }
}

// If |maybe_reason| contains a valid code and reason, then closes with it,
// otherwise closes with unspecified code and reason.
void WebSocketStream::CloseMaybeWithReason(ScriptValue maybe_reason,
                                           ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream " << this << " CloseMaybeWithReason()";
  WebSocketError* info = V8WebSocketError::ToWrappable(
      script_state_->GetIsolate(), maybe_reason.V8Value());
  if (info) {
    CloseInternal(info->closeCode(), info->reason(), exception_state);
  } else {
    CloseWithUnspecifiedCode(exception_state);
  }
}

void WebSocketStream::CloseWithUnspecifiedCode(
    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream " << this << " CloseWithUnspecifiedCode()";
  CloseInternal(std::nullopt, String(), exception_state);
  DCHECK(!exception_state.HadException());
}

void WebSocketStream::CloseInternal(std::optional<uint16_t> code,
                                    const String& reason,
                                    ExceptionState& exception_state) {
  DVLOG(1) << "WebSocketStream " << this
           << " CloseInternal() code=" << (code ? code.value() : uint16_t{0})
           << " reason=" << reason;

  common_.CloseInternal(code, reason, channel_, exception_state);
}

v8::Local<v8::Value> WebSocketStream::CreateWebSocketError(
    String message,
    std::optional<uint16_t> close_code,
    String reason) {
  return WebSocketError::Create(script_state_->GetIsolate(), std::move(message),
                                close_code, std::move(reason));
}

void WebSocketStream::OnAbort() {
  DVLOG(1) << "WebSocketStream " << this << " OnAbort()";

  if (was_ever_connected_ || !channel_)
    return;

  channel_->CancelHandshake();
  channel_ = nullptr;

  auto exception = V8ThrowDOMException::CreateOrEmpty(
      script_state_->GetIsolate(), DOMExceptionCode::kAbortError,
      "WebSocket handshake was aborted");
  opened_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
  closed_->Reject(ScriptValue(script_state_->GetIsolate(), exception));
  abort_handle_.Clear();
}

WebSocketCloseInfo* WebSocketStream::MakeCloseInfo(uint16_t close_code,
                                                   const String& reason) {
  DVLOG(1) << "WebSocketStream MakeCloseInfo() code=" << close_code
           << " reason=" << reason;

  auto* info = MakeGarbageCollected<WebSocketCloseInfo>();
  info->setCloseCode(close_code);
  info->setReason(reason);
  return info;
}

}  // namespace blink
