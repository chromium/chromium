// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"

#include <string.h>

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class ReadableStreamBytesConsumer::OnFulfilled final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      ReadableStreamBytesConsumer* consumer) {
    return (MakeGarbageCollected<OnFulfilled>(script_state, consumer))
        ->BindToV8Function();
  }

  OnFulfilled(ScriptState* script_state, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(script_state), consumer_(consumer) {}

  ScriptValue Call(ScriptValue v) override {
    bool done;
    v8::Local<v8::Value> item = v.V8Value();
    if (!item->IsObject()) {
      consumer_->OnRejected();
      return ScriptValue();
    }
    v8::Local<v8::Value> value;
    if (!V8UnpackIteratorResult(GetScriptState(), item.As<v8::Object>(), &done)
             .ToLocal(&value)) {
      consumer_->OnRejected();
      return ScriptValue();
    }
    if (done) {
      consumer_->OnReadDone();
      return v;
    }
    if (!value->IsUint8Array()) {
      consumer_->OnRejected();
      return ScriptValue();
    }
    consumer_->OnRead(V8Uint8Array::ToImpl(value.As<v8::Object>()));
    return v;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  Member<ReadableStreamBytesConsumer> consumer_;
};

class ReadableStreamBytesConsumer::OnRejected final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> CreateFunction(
      ScriptState* script_state,
      ReadableStreamBytesConsumer* consumer) {
    return (MakeGarbageCollected<OnRejected>(script_state, consumer))
        ->BindToV8Function();
  }

  OnRejected(ScriptState* script_state, ReadableStreamBytesConsumer* consumer)
      : ScriptFunction(script_state), consumer_(consumer) {}

  ScriptValue Call(ScriptValue v) override {
    consumer_->OnRejected();
    return v;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    ScriptFunction::Trace(visitor);
  }

 private:
  Member<ReadableStreamBytesConsumer> consumer_;
};

ReadableStreamBytesConsumer::ReadableStreamBytesConsumer(
    ScriptState* script_state,
    ReadableStream* stream)
    : reader_(stream->GetReaderNotForAuthorCode(script_state)),
      script_state_(script_state) {}

ReadableStreamBytesConsumer::~ReadableStreamBytesConsumer() {}

BytesConsumer::Result ReadableStreamBytesConsumer::BeginRead(
    const char** buffer,
    size_t* available) {
  *buffer = nullptr;
  *available = 0;
  if (state_ == PublicState::kErrored)
    return Result::kError;
  if (state_ == PublicState::kClosed)
    return Result::kDone;

  if (pending_buffer_) {
    DCHECK_LE(pending_offset_, pending_buffer_->length());
    *buffer = reinterpret_cast<const char*>(pending_buffer_->Data()) +
              pending_offset_;
    *available = pending_buffer_->length() - pending_offset_;
    return Result::kOk;
  }
  if (!is_reading_) {
    is_reading_ = true;
    ScriptState::Scope scope(script_state_);
    DCHECK(reader_);

    ExceptionState exception_state(script_state_->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");

    ScriptPromise script_promise =
        reader_->read(script_state_, exception_state);
    if (exception_state.HadException())
      script_promise = ScriptPromise::Reject(script_state_, exception_state);

    script_promise
        .Then(OnFulfilled::CreateFunction(script_state_, this),
              OnRejected::CreateFunction(script_state_, this))
        .MarkAsHandled();
  }
  return Result::kShouldWait;
}

BytesConsumer::Result ReadableStreamBytesConsumer::EndRead(size_t read_size) {
  DCHECK(pending_buffer_);
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
  is_reading_ = false;
  if (state_ == PublicState::kClosed)
    return;
  DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
  state_ = PublicState::kErrored;
  reader_ = nullptr;
  Client* client = client_;
  ClearClient();
  if (client)
    client->OnStateChange();
}

}  // namespace blink
