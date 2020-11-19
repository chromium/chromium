// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/native_file_system_writable_file_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_queuing_strategy_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_write_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/count_queuing_strategy.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/file_system_access/native_file_system_underlying_sink.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"

namespace blink {

NativeFileSystemWritableFileStream* NativeFileSystemWritableFileStream::Create(
    ScriptState* script_state,
    mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>
        writer_pending_remote) {
  DCHECK(writer_pending_remote);
  ScriptState::Scope scope(script_state);

  ExecutionContext* context = ExecutionContext::From(script_state);

  auto* stream = MakeGarbageCollected<NativeFileSystemWritableFileStream>();

  auto* underlying_sink = MakeGarbageCollected<NativeFileSystemUnderlyingSink>(
      context, std::move(writer_pending_remote));
  stream->underlying_sink_ = underlying_sink;
  auto underlying_sink_value = ScriptValue::From(script_state, underlying_sink);

  auto* init = QueuingStrategyInit::Create();
  // HighWaterMark set to 1 here. This allows the stream to appear available
  // without adding additional buffering.
  init->setHighWaterMark(1);
  auto* strategy = CountQueuingStrategy::Create(script_state, init);
  ScriptValue strategy_value = ScriptValue::From(script_state, strategy);

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "NativeFileSystemWritableFileStream");
  stream->InitInternal(script_state, underlying_sink_value, strategy_value,
                       exception_state);

  if (exception_state.HadException())
    return nullptr;

  return stream;
}

ScriptPromise NativeFileSystemWritableFileStream::write(
    ScriptState* script_state,
    const ArrayBufferOrArrayBufferViewOrBlobOrUSVStringOrWriteParams& data,
    ExceptionState& exception_state) {
  WritableStreamDefaultWriter* writer =
      WritableStream::AcquireDefaultWriter(script_state, this, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  ScriptPromise promise = writer->write(
      script_state, ScriptValue::From(script_state, data), exception_state);

  WritableStreamDefaultWriter::Release(script_state, writer);
  return promise;
}

ScriptPromise NativeFileSystemWritableFileStream::truncate(
    ScriptState* script_state,
    uint64_t size,
    ExceptionState& exception_state) {
  WritableStreamDefaultWriter* writer =
      WritableStream::AcquireDefaultWriter(script_state, this, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* options = WriteParams::Create();
  options->setType("truncate");
  options->setSize(size);

  ScriptPromise promise = writer->write(
      script_state, ScriptValue::From(script_state, options), exception_state);

  WritableStreamDefaultWriter::Release(script_state, writer);
  return promise;
}

ScriptPromise NativeFileSystemWritableFileStream::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  WritableStreamDefaultWriter* writer =
      WritableStream::AcquireDefaultWriter(script_state, this, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  ScriptPromise promise = writer->close(script_state, exception_state);

  WritableStreamDefaultWriter::Release(script_state, writer);
  return promise;
}

ScriptPromise NativeFileSystemWritableFileStream::seek(
    ScriptState* script_state,
    uint64_t offset,
    ExceptionState& exception_state) {
  WritableStreamDefaultWriter* writer =
      WritableStream::AcquireDefaultWriter(script_state, this, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* options = WriteParams::Create();
  options->setType("seek");
  options->setPosition(offset);

  ScriptPromise promise = writer->write(
      script_state, ScriptValue::From(script_state, options), exception_state);

  WritableStreamDefaultWriter::Release(script_state, writer);
  return promise;
}

void NativeFileSystemWritableFileStream::Trace(Visitor* visitor) const {
  WritableStream::Trace(visitor);
  visitor->Trace(underlying_sink_);
}

}  // namespace blink
