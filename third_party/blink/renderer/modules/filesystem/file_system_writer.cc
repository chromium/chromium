// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_writer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

FileSystemWriter::FileSystemWriter(mojom::blink::FileWriterPtr writer)
    : writer_(std::move(writer)) {
  DCHECK(writer_);
}

ScriptPromise FileSystemWriter::write(ScriptState* script_state,
                                      uint64_t position,
                                      ScriptValue data,
                                      ExceptionState& exception_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (V8Blob::hasInstance(data.V8Value(), isolate)) {
    Blob* blob = V8Blob::ToImpl(data.V8Value().As<v8::Object>());
    return WriteBlob(script_state, position, blob);
  }
  if (!ReadableStreamOperations::IsReadableStream(script_state, data,
                                                  exception_state)
           .value_or(false)) {
    if (!exception_state.HadException())
      exception_state.ThrowTypeError("data should be a Blob or ReadableStream");
    return ScriptPromise();
  }
  return WriteStream(script_state, position, data, exception_state);
}

ScriptPromise FileSystemWriter::WriteBlob(ScriptState* script_state,
                                          uint64_t position,
                                          Blob* blob) {
  if (!writer_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_->Write(
      position, blob->AsMojoBlob(),
      WTF::Bind(&FileSystemWriter::WriteComplete, WrapPersistent(this)));
  return result;
}

class FileSystemWriter::StreamWriterClient
    : public GarbageCollectedFinalized<StreamWriterClient>,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(StreamWriterClient);

 public:
  explicit StreamWriterClient(FileSystemWriter* writer) : writer_(writer) {}

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle data_pipe) override {
    data_pipe_ = std::move(data_pipe);
  }

  mojo::ScopedDataPipeConsumerHandle TakeDataPipe() {
    DCHECK(data_pipe_);
    return std::move(data_pipe_);
  }

  void DidFetchDataLoadedDataPipe() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(!did_finish_writing_to_pipe_);
    DCHECK(writer_->pending_operation_);
    did_finish_writing_to_pipe_ = true;
  }

  void DidFetchDataLoadFailed() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    writer_->pending_operation_->Reject(
        FileError::CreateDOMException(base::File::FILE_ERROR_FAILED));
    Reset();
  }

  void Abort() override {
    // WriteComplete could have been called with an error before we reach this
    // point, in that case just return.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    writer_->pending_operation_->Reject(
        FileError::CreateDOMException(base::File::FILE_ERROR_ABORT));
    Reset();
  }

  void WriteComplete(base::File::Error result, uint64_t bytes_written) {
    // Early return if we already completed (with an error) before.
    if (did_complete_)
      return;
    DCHECK(writer_->pending_operation_);
    did_complete_ = true;
    if (result != base::File::FILE_OK) {
      writer_->pending_operation_->Reject(
          FileError::CreateDOMException(result));
    } else {
      DCHECK(did_finish_writing_to_pipe_);
      writer_->pending_operation_->Resolve();
    }
    Reset();
  }

  void Trace(Visitor* visitor) override {
    Client::Trace(visitor);
    visitor->Trace(writer_);
  }

 private:
  void Reset() {
    writer_->pending_operation_ = nullptr;
    writer_->stream_loader_ = nullptr;
  }

  Member<FileSystemWriter> writer_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  bool did_finish_writing_to_pipe_ = false;
  bool did_complete_ = false;
};

ScriptPromise FileSystemWriter::WriteStream(ScriptState* script_state,
                                            uint64_t position,
                                            ScriptValue stream,
                                            ExceptionState& exception_state) {
  if (!writer_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  DCHECK(!stream_loader_);

  auto reader = ReadableStreamOperations::GetReader(script_state, stream,
                                                    exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  auto* consumer = new ReadableStreamBytesConsumer(script_state, reader);

  stream_loader_ = FetchDataLoader::CreateLoaderAsDataPipe(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kInternalDefault));
  pending_operation_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = pending_operation_->Promise();
  auto* client = new StreamWriterClient(this);
  stream_loader_->Start(consumer, client);
  writer_->WriteStream(
      position, client->TakeDataPipe(),
      WTF::Bind(&StreamWriterClient::WriteComplete, WrapPersistent(client)));
  return result;
}

ScriptPromise FileSystemWriter::truncate(ScriptState* script_state,
                                         uint64_t size) {
  if (!writer_ || pending_operation_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  pending_operation_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise result = pending_operation_->Promise();
  writer_->Truncate(size, WTF::Bind(&FileSystemWriter::TruncateComplete,
                                    WrapPersistent(this)));
  return result;
}

ScriptPromise FileSystemWriter::close(ScriptState* script_state) {
  if (!writer_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError));
  }
  writer_ = nullptr;
  return ScriptPromise::CastUndefined(script_state);
}

void FileSystemWriter::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(pending_operation_);
  visitor->Trace(stream_loader_);
}

void FileSystemWriter::WriteComplete(base::File::Error result,
                                     uint64_t bytes_written) {
  DCHECK(pending_operation_);
  if (result == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(FileError::CreateDOMException(result));
  }
  pending_operation_ = nullptr;
}

void FileSystemWriter::TruncateComplete(base::File::Error result) {
  DCHECK(pending_operation_);
  if (result == base::File::FILE_OK) {
    pending_operation_->Resolve();
  } else {
    pending_operation_->Reject(FileError::CreateDOMException(result));
  }
  pending_operation_ = nullptr;
}

}  // namespace blink
