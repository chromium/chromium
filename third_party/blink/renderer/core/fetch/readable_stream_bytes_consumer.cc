// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"

#include <string.h>

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class ReadableStreamBytesConsumer::BytesConsumerReadRequest final
    : public ReadRequest {
 public:
  explicit BytesConsumerReadRequest(ReadableStreamBytesConsumer* consumer)
      : consumer_(consumer) {}

  void ChunkSteps(ScriptState* script_state,
                  v8::Local<v8::Value> chunk,
                  ExceptionState& exception_state) const override {
    if (!chunk->IsUint8Array()) {
      consumer_->OnRejected();
      return;
    }
    ScriptState::Scope scope(script_state);
    consumer_->OnRead(
        NativeValueTraits<MaybeShared<DOMUint8Array>>::NativeValue(
            script_state->GetIsolate(), chunk, exception_state)
            .Get());
    DCHECK(!exception_state.HadException());
  }

  void CloseSteps(ScriptState* script_state) const override {
    consumer_->OnReadDone();
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    consumer_->OnRejected();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    ReadRequest::Trace(visitor);
  }

 private:
  Member<ReadableStreamBytesConsumer> consumer_;
};

ReadableStreamBytesConsumer::ReadableStreamBytesConsumer(
    ScriptState* script_state,
    ReadableStream* stream)
    : script_state_(script_state) {
  DCHECK(!ReadableStream::IsLocked(stream));

  // Since the stream is not locked, AcquireDefaultReader cannot fail.
  NonThrowableExceptionState exception_state(__FILE__, __LINE__);
  reader_ = ReadableStream::AcquireDefaultReader(script_state, stream,
                                                 exception_state);
}

ReadableStreamBytesConsumer::~ReadableStreamBytesConsumer() {}

BytesConsumer::Result ReadableStreamBytesConsumer::BeginRead(
    base::span<const char>& buffer) {
  buffer = {};
  if (state_ == PublicState::kErrored)
    return Result::kError;
  if (state_ == PublicState::kClosed)
    return Result::kDone;

  if (pending_buffer_) {
    // The UInt8Array has become detached due to, for example, the site
    // transferring it away via postMessage().  Since we were in the middle
    // of reading the array we must error out.
    if (pending_buffer_->IsDetached()) {
      SetErrored();
      return Result::kError;
    }

    DCHECK_LE(pending_offset_, pending_buffer_->length());
    buffer =
        base::as_chars(pending_buffer_->ByteSpan().subspan(pending_offset_));
    return Result::kOk;
  }
  if (!is_reading_) {
    is_reading_ = true;
    is_inside_read_ = true;
    ScriptState::Scope scope(script_state_);
    DCHECK(reader_);

    ExceptionState exception_state(script_state_->GetIsolate(),
                                   v8::ExceptionContext::kUnknown, "", "");
    auto* read_request = MakeGarbageCollected<BytesConsumerReadRequest>(this);
    ReadableStreamDefaultReader::Read(script_state_, reader_, read_request,
                                      exception_state);
    is_inside_read_ = false;
  }
  return Result::kShouldWait;
}

BytesConsumer::Result ReadableStreamBytesConsumer::EndRead(size_t read_size) {
  DCHECK(pending_buffer_);

  // While the buffer size is immutable once constructed, the buffer can be
  // detached if the site does something like transfer it away using
  // postMessage().  Since we were in the middle of a read we must error out.
  if (pending_buffer_->IsDetached()) {
    SetErrored();
    return Result::kError;
  }

  DCHECK_LE(pending_offset_ + read_size, pending_buffer_->length());
  pending_offset_ += read_size;
  if (pending_offset_ >= pending_buffer_->length()) {
    pending_buffer_ = nullptr;
    pending_offset_ = 0;
  }
  return Result::kOk;
}

void ReadableStreamBytesConsumer::SetClient(Client* client) {
  DCHECK(!client_);
  DCHECK(client);
  client_ = client;
}

void ReadableStreamBytesConsumer::ClearClient() {
  client_ = nullptr;
}

void ReadableStreamBytesConsumer::Cancel() {
  if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
    return;
  // BytesConsumer::Cancel can be called with ScriptForbiddenScope (e.g.,
  // in ExecutionContextLifecycleObserver::ContextDestroyed()). We don't run
  // ReadableStreamDefaultReader::cancel in such a case.
  if (!ScriptForbiddenScope::IsScriptForbidden()) {
    ScriptState::Scope scope(script_state_);
    ExceptionState exception_state(script_state_->GetIsolate(),
                                   v8::ExceptionContext::kUnknown, "", "");
    reader_->cancel(script_state_, exception_state);
    // We ignore exceptions as we can do nothing here.
  }
  state_ = PublicState::kClosed;
  ClearClient();
  reader_ = nullptr;
}

BytesConsumer::PublicState ReadableStreamBytesConsumer::GetPublicState() const {
  return state_;
}

BytesConsumer::Error ReadableStreamBytesConsumer::GetError() const {
  return Error("Failed to read from a ReadableStream.");
}

void ReadableStreamBytesConsumer::Trace(Visitor* visitor) const {
  visitor->Trace(reader_);
  visitor->Trace(client_);
  visitor->Trace(pending_buffer_);
  visitor->Trace(script_state_);
  BytesConsumer::Trace(visitor);
}

void ReadableStreamBytesConsumer::OnRead(DOMUint8Array* buffer) {
  DCHECK(is_reading_);
  DCHECK(buffer);
  DCHECK(!pending_buffer_);
  DCHECK(!pending_offset_);
  if (is_inside_read_) {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state_)->GetAgent()->event_loop();
    event_loop->EnqueueMicrotask(
        WTF::BindOnce(&ReadableStreamBytesConsumer::OnRead,
                      WrapPersistent(this), WrapPersistent(buffer)));
    return;
  }
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  pending_buffer_ = buffer;
  if (client_)
    client_->OnStateChange();
}

void ReadableStreamBytesConsumer::OnReadDone() {
  DCHECK(is_reading_);
  DCHECK(!pending_buffer_);
  if (is_inside_read_) {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state_)->GetAgent()->event_loop();
    event_loop->EnqueueMicrotask(WTF::BindOnce(
        &ReadableStreamBytesConsumer::OnReadDone, WrapPersistent(this)));
    return;
  }
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  state_ = PublicState::kClosed;
  reader_ = nullptr;
  Client* client = client_;
  ClearClient();
  if (client)
    client->OnStateChange();
}

void ReadableStreamBytesConsumer::OnRejected() {
  DCHECK(is_reading_);
  DCHECK(!pending_buffer_);
  if (is_inside_read_) {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state_)->GetAgent()->event_loop();
    event_loop->EnqueueMicrotask(WTF::BindOnce(
        &ReadableStreamBytesConsumer::OnRejected, WrapPersistent(this)));
    return;
  }
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  Client* client = client_;
  SetErrored();
  if (client)
    client->OnStateChange();
}

void ReadableStreamBytesConsumer::SetErrored() {
  DCHECK_NE(state_, PublicState::kClosed);
  DCHECK_NE(state_, PublicState::kErrored);
  state_ = PublicState::kErrored;
  ClearClient();
  reader_ = nullptr;
}

}  // namespace blink
