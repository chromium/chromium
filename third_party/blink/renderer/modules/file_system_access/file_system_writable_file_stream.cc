// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_writable_file_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_queuing_strategy_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_arraybuffer_arraybufferview_blob_usvstring_writeparams.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_write_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/count_queuing_strategy.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_underlying_sink.h"

namespace blink {

FileSystemWritableFileStream* FileSystemWritableFileStream::Create(
    ScriptState* script_state,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileWriter>
        writer_pending_remote) {
  DCHECK(writer_pending_remote);
  ScriptState::Scope scope(script_state);

  ExecutionContext* context = ExecutionContext::From(script_state);

  auto* stream = MakeGarbageCollected<FileSystemWritableFileStream>();

  auto* underlying_sink = MakeGarbageCollected<FileSystemUnderlyingSink>(
      context, std::move(writer_pending_remote));
  stream->underlying_sink_ = underlying_sink;
  auto underlying_sink_value = ScriptValue::From(script_state, underlying_sink);

  auto* init = QueuingStrategyInit::Create();
  // HighWaterMark set to 1 here. This allows the stream to appear available
  // without adding additional buffering.
  init->setHighWaterMark(1);
  auto* strategy = CountQueuingStrategy::Create(script_state, init);
  ScriptValue strategy_value = ScriptValue::From(script_state, strategy);

  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "FileSystemWritableFileStream");
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  stream->InitInternal(script_state, underlying_sink_value, strategy_value,
                       exception_state);

  if (exception_state.HadException())
    return nullptr;

  return stream;
}

ScriptPromise FileSystemWritableFileStream::write(
    ScriptState* script_state,
    const V8UnionBlobOrBufferSourceOrUSVStringOrWriteParams* data,
    ExceptionState& exception_state) {
  WritableStreamDefaultWriter* writer =
      WritableStream::AcquireDefaultWriter(script_state, this, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  v8::Local<v8::Value> v8_data;
  {
    v8::TryCatch v8_try_block(script_state->GetIsolate());
    if (!ToV8Traits<V8UnionBlobOrBufferSourceOrUSVStringOrWriteParams>::ToV8(
             script_state, data)
             .ToLocal(&v8_data)) {
      exception_state.RethrowV8Exception(v8_try_block.Exception());
      return ScriptPromise();
    }
  }
  ScriptPromise promise = writer->write(
      script_state, ScriptValue(script_state->GetIsolate(), v8_data),
      exception_state);

  WritableStreamDefaultWriter::Release(script_state, writer);
  return promise;
}

ScriptPromise FileSystemWritableFileStream::truncate(
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

ScriptPromise FileSystemWritableFileStream::seek(
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

void FileSystemWritableFileStream::Trace(Visitor* visitor) const {
  WritableStream::Trace(visitor);
  visitor->Trace(underlying_sink_);
}

}  // namespace blink
