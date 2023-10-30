// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer_underlying_byte_source.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer_underlying_source.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_tee.h"
#include "third_party/blink/renderer/core/fetch/bytes_uploader.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final
    : public GarbageCollected<LoaderClient>,
      public ExecutionContextLifecycleObserver,
      public FetchDataLoader::Client {
 public:
  LoaderClient(ExecutionContext* execution_context,
               BodyStreamBuffer* buffer,
               FetchDataLoader::Client* client)
      : ExecutionContextLifecycleObserver(execution_context),
        buffer_(buffer),
        client_(client) {}
  LoaderClient(const LoaderClient&) = delete;
  LoaderClient& operator=(const LoaderClient&) = delete;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> blob_data_handle) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedBlobHandle(std::move(blob_data_handle));
  }

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedArrayBuffer(array_buffer);
  }

  void DidFetchDataLoadedFormData(FormData* form_data) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedFormData(form_data);
  }

  void DidFetchDataLoadedString(const String& string) override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedString(string);
  }

  void DidFetchDataStartedDataPipe(
      mojo::ScopedDataPipeConsumerHandle data_pipe) override {
    client_->DidFetchDataStartedDataPipe(std::move(data_pipe));
  }

  void DidFetchDataLoadedDataPipe() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedDataPipe();
  }

  void DidFetchDataLoadedCustomFormat() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadedCustomFormat();
  }

  void DidFetchDataLoadFailed() override {
    buffer_->EndLoading();
    client_->DidFetchDataLoadFailed();
  }

  void Abort() override { NOTREACHED(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(buffer_);
    visitor->Trace(client_);
    ExecutionContextLifecycleObserver::Trace(visitor);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  void ContextDestroyed() override { buffer_->StopLoading(); }

  Member<BodyStreamBuffer> buffer_;
  Member<FetchDataLoader::Client> client_;
};

// Use a Create() method to split construction from initialisation.
// Initialisation may result in nested calls to ContextDestroyed() and so is not
// safe to do during construction.

// static
BodyStreamBuffer* BodyStreamBuffer::Create(
    ScriptState* script_state,
    BytesConsumer* consumer,
    AbortSignal* signal,
    ScriptCachedMetadataHandler* cached_metadata_handler,
    scoped_refptr<BlobDataHandle> side_data_blob) {
  auto* buffer = MakeGarbageCollected<BodyStreamBuffer>(
      PassKey(), script_state, consumer, signal, cached_metadata_handler,
      std::move(side_data_blob));
  buffer->Init();
  return buffer;
}

BodyStreamBuffer::BodyStreamBuffer(
    PassKey,
    ScriptState* script_state,
    BytesConsumer* consumer,
    AbortSignal* signal,
    ScriptCachedMetadataHandler* cached_metadata_handler,
    scoped_refptr<BlobDataHandle> side_data_blob)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      consumer_(consumer),
      signal_(signal),
      cached_metadata_handler_(cached_metadata_handler),
      side_data_blob_(std::move(side_data_blob)),
      made_from_readable_stream_(false) {}

void BodyStreamBuffer::Init() {
  DCHECK(consumer_);

  if (RuntimeEnabledFeatures::ByobFetchEnabled()) {
    underlying_byte_source_ =
        MakeGarbageCollected<BodyStreamBufferUnderlyingByteSource>(
            script_state_, this);
    stream_ = ReadableStream::CreateByteStream(script_state_,
                                               underlying_byte_source_);
  } else {
    underlying_source_ = MakeGarbageCollected<BodyStreamBufferUnderlyingSource>(
        script_state_, this);

    stream_ = ReadableStream::CreateWithCountQueueingStrategy(
        script_state_, underlying_source_, 0);
  }
  stream_broken_ = !stream_;

  // ContextDestroyed() can be called inside the ReadableStream constructor when
  // a worker thread is being terminated. See https://crbug.com/1007162 for
  // details. If consumer_ is null, assume that this happened and this object
  // will never actually be used, and so it is fine to skip the rest of
  // initialisation.
  if (!consumer_)
    return;

  consumer_->SetClient(this);
  if (signal_) {
    if (signal_->aborted()) {
      Abort();
    } else {
      stream_buffer_abort_handle_ = signal_->AddAlgorithm(
          WTF::BindOnce(&BodyStreamBuffer::Abort, WrapWeakPersistent(this)));
    }
  }
  OnStateChange();
}

BodyStreamBuffer::BodyStreamBuffer(
    ScriptState* script_state,
    ReadableStream* stream,
    ScriptCachedMetadataHandler* cached_metadata_handler,
    scoped_refptr<BlobDataHandle> side_data_blob)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      stream_(stream),
      signal_(nullptr),
      cached_metadata_handler_(cached_metadata_handler),
      side_data_blob_(std::move(side_data_blob)),
      made_from_readable_stream_(true) {
  DCHECK(stream_);
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::DrainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy,
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored() || stream_broken_)
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer_->DrainAsBlobDataHandle(policy);
  if (blob_data_handle) {
    CloseAndLockAndDisturb(exception_state);
    return blob_data_handle;
  }
  return nullptr;
}

scoped_refptr<EncodedFormData> BodyStreamBuffer::DrainAsFormData(
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored() || stream_broken_)
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<EncodedFormData> form_data = consumer_->DrainAsFormData();
  if (form_data) {
    CloseAndLockAndDisturb(exception_state);
    return form_data;
  }
  return nullptr;
}

void BodyStreamBuffer::DrainAsChunkedDataPipeGetter(
    ScriptState* script_state,
    mojo::PendingReceiver<network::mojom::blink::ChunkedDataPipeGetter>
        pending_receiver,
    BytesUploader::Client* client) {
  DCHECK(!IsStreamLocked());
  auto* consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream_);
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  stream_uploader_ = MakeGarbageCollected<BytesUploader>(
      execution_context, consumer, std::move(pending_receiver),
      execution_context->GetTaskRunner(TaskType::kNetworking), client);
}

void BodyStreamBuffer::StartLoading(FetchDataLoader* loader,
                                    FetchDataLoader::Client* client,
                                    ExceptionState& exception_state) {
  DCHECK(!loader_);
  DCHECK(!keep_alive_);
  DCHECK(script_state_->ContextIsValid());
  if (signal_) {
    if (signal_->aborted()) {
      client->Abort();
      return;
    }
    loader_client_abort_handle_ = signal_->AddAlgorithm(WTF::BindOnce(
        &FetchDataLoader::Client::Abort, WrapWeakPersistent(client)));
  }
  loader_ = loader;
  auto* handle = ReleaseHandle(exception_state);
  if (exception_state.HadException())
    return;
  keep_alive_ = this;

  auto* execution_context = GetExecutionContext();
  if (execution_context) {
    virtual_time_pauser_ =
        execution_context->GetScheduler()->CreateWebScopedVirtualTimePauser(
            "ResponseBody",
            WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
    virtual_time_pauser_.PauseVirtualTime();
  }
  loader->Start(handle, MakeGarbageCollected<LoaderClient>(execution_context,
                                                           this, client));
}

void BodyStreamBuffer::Tee(BodyStreamBuffer** branch1,
                           BodyStreamBuffer** branch2,
                           ExceptionState& exception_state) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  *branch1 = nullptr;
  *branch2 = nullptr;
  auto* cached_metadata_handler = cached_metadata_handler_.Get();
  scoped_refptr<BlobDataHandle> side_data_blob = TakeSideDataBlob();

  if (made_from_readable_stream_) {
    if (stream_broken_) {
      // We don't really know what state the stream is in, so throw an exception
      // rather than making things worse.
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Unsafe to tee stream in unknown state");
      return;
    }
    ReadableStream* stream1 = nullptr;
    ReadableStream* stream2 = nullptr;

    // IsByteStreamController() can be false if the stream was constructed from
    // a user-defined stream.
    if (RuntimeEnabledFeatures::ByobFetchEnabled() &&
        stream_->GetController()->IsByteStreamController()) {
      stream_->ByteStreamTee(script_state_, &stream1, &stream2,
                             exception_state);
    } else {
      DCHECK(stream_->GetController()->IsDefaultController());
      stream_->Tee(script_state_, &stream1, &stream2, true, exception_state);
    }
    if (exception_state.HadException()) {
      stream_broken_ = true;
      return;
    }

    *branch1 = MakeGarbageCollected<BodyStreamBuffer>(
        script_state_, stream1, cached_metadata_handler, side_data_blob);
    *branch2 = MakeGarbageCollected<BodyStreamBuffer>(
        script_state_, stream2, cached_metadata_handler, side_data_blob);
    return;
  }
  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  auto* handle = ReleaseHandle(exception_state);
  if (exception_state.HadException()) {
    stream_broken_ = true;
    return;
  }
  BytesConsumerTee(ExecutionContext::From(script_state_), handle, &dest1,
                   &dest2);
  *branch1 = BodyStreamBuffer::Create(script_state_, dest1, signal_,
                                      cached_metadata_handler, side_data_blob);
  *branch2 = BodyStreamBuffer::Create(script_state_, dest2, signal_,
                                      cached_metadata_handler, side_data_blob);
}

ScriptPromise BodyStreamBuffer::Cancel(ScriptState* script_state,
                                       ScriptValue reason,
                                       ExceptionState& exception_state) {
  if (underlying_byte_source_) {
    ScriptPromise cancel_promise = underlying_byte_source_->Cancel(
        ToV8(reason, script_state->GetContext()->Global(),
             script_state->GetIsolate()),
        exception_state);
    if (exception_state.HadException()) {
      exception_state.ClearException();
      return ScriptPromise::CastUndefined(script_state);
    } else {
      return cancel_promise;
    }
  } else {
    CHECK(underlying_source_);
    return underlying_source_->Cancel(script_state, reason, exception_state);
  }
}

void BodyStreamBuffer::OnStateChange() {
  if (!consumer_ || !GetExecutionContext() ||
      GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  ExceptionState exception_state(script_state_->GetIsolate(),
                                 ExceptionContextType::kUnknown, "", "");

  switch (consumer_->GetPublicState()) {
    case BytesConsumer::PublicState::kReadableOrWaiting:
      break;
    case BytesConsumer::PublicState::kClosed:
      Close(exception_state);
      return;
    case BytesConsumer::PublicState::kErrored:
      GetError();
      return;
  }
  ProcessData(exception_state);
}

void BodyStreamBuffer::ContextDestroyed() {
  CancelConsumer();
  keep_alive_.Clear();
}

bool BodyStreamBuffer::IsStreamReadable() const {
  return stream_->IsReadable();
}

bool BodyStreamBuffer::IsStreamClosed() const {
  return stream_->IsClosed();
}

bool BodyStreamBuffer::IsStreamErrored() const {
  return stream_->IsErrored();
}

bool BodyStreamBuffer::IsStreamLocked() const {
  return stream_->IsLocked();
}

bool BodyStreamBuffer::IsStreamDisturbed() const {
  return stream_->IsDisturbed();
}

void BodyStreamBuffer::CloseAndLockAndDisturb(ExceptionState& exception_state) {
  DCHECK(!stream_broken_);

  cached_metadata_handler_ = nullptr;

  if (IsStreamReadable()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    Close(exception_state);
  }

  stream_->LockAndDisturb(script_state_);
}

bool BodyStreamBuffer::IsAborted() {
  if (!signal_)
    return false;
  return signal_->aborted();
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::TakeSideDataBlob() {
  return std::move(side_data_blob_);
}

void BodyStreamBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(stream_);
  visitor->Trace(underlying_byte_source_);
  visitor->Trace(underlying_source_);
  visitor->Trace(stream_uploader_);
  visitor->Trace(consumer_);
  visitor->Trace(loader_);
  visitor->Trace(signal_);
  visitor->Trace(stream_buffer_abort_handle_);
  visitor->Trace(loader_client_abort_handle_);
  visitor->Trace(cached_metadata_handler_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void BodyStreamBuffer::Abort() {
  if (!GetExecutionContext()) {
    DCHECK(!consumer_);
    return;
  }
  if (underlying_byte_source_) {
    auto* byte_controller =
        To<ReadableByteStreamController>(stream_->GetController());
    v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kAbortError,
        "BodyStreamBuffer was aborted");
    CHECK(!dom_exception.IsEmpty());
    ReadableByteStreamController::Error(script_state_, byte_controller,
                                        dom_exception);
  } else {
    CHECK(underlying_source_);
    underlying_source_->Controller()->Error(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }
  CancelConsumer();
}

void BodyStreamBuffer::Close(ExceptionState& exception_state) {
  // Close() can be called during construction, in which case Controller()
  // will not be set yet.
  if (underlying_byte_source_) {
    if (script_state_->ContextIsValid()) {
      ScriptState::Scope scope(script_state_);
      stream_->CloseStream(script_state_, exception_state);
    } else {
      // If the context is not valid then Close() will not try to resolve the
      // promises, and that is not a problem.
      stream_->CloseStream(script_state_, exception_state);
    }
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Controller::close throws exception "
                    << exception_state.Code() << ", "
                    << exception_state.Message();
      exception_state.ClearException();
      return;
    }
  } else if (underlying_source_->Controller()) {
    underlying_source_->Controller()->Close();
  }
  CancelConsumer();
}

void BodyStreamBuffer::GetError() {
  {
    ScriptState::Scope scope(script_state_);
    auto error = V8ThrowException::CreateTypeError(script_state_->GetIsolate(),
                                                   "network error");
    if (underlying_byte_source_) {
      auto* byte_controller =
          To<ReadableByteStreamController>(stream_->GetController());
      ReadableByteStreamController::Error(script_state_, byte_controller,
                                          error);
    } else {
      CHECK(underlying_source_);
      underlying_source_->Controller()->Error(error);
    }
  }
  CancelConsumer();
}

void BodyStreamBuffer::RaiseOOMError() {
  {
    ScriptState::Scope scope(script_state_);
    auto error = V8ThrowException::CreateRangeError(
        script_state_->GetIsolate(), "Array buffer allocation failed");
    if (underlying_byte_source_) {
      auto* byte_controller =
          To<ReadableByteStreamController>(stream_->GetController());
      ReadableByteStreamController::Error(script_state_, byte_controller,
                                          error);
    } else {
      CHECK(underlying_source_);
      underlying_source_->Controller()->Error(error);
    }
  }
  CancelConsumer();
}

void BodyStreamBuffer::CancelConsumer() {
  side_data_blob_.reset();
  virtual_time_pauser_.UnpauseVirtualTime();
  if (consumer_) {
    consumer_->Cancel();
    consumer_ = nullptr;
  }
}

void BodyStreamBuffer::ProcessData(ExceptionState& exception_state) {
  DCHECK(consumer_);
  DCHECK(!in_process_data_);

  base::AutoReset<bool> auto_reset(&in_process_data_, true);
  while (stream_needs_more_) {
    const char* buffer = nullptr;
    size_t available = 0;
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;
    DOMUint8Array* array = nullptr;
    DOMArrayBufferView* byob_view = nullptr;
    if (result == BytesConsumer::Result::kOk) {
      if (stream_->GetController()->IsByteStreamController()) {
        auto* byte_controller =
            To<ReadableByteStreamController>(stream_->GetController());
        if (ReadableStreamBYOBRequest* request =
                byte_controller->byobRequest()) {
          DOMArrayBufferView* view = request->view().Get();
          available = std::min(view->byteLength(), available);
          memcpy(
              static_cast<char*>(view->buffer()->Data()) + view->byteOffset(),
              buffer, available);
          byob_view = view;
        }
      }
      if (!byob_view) {
        CHECK(!array);
        array = DOMUint8Array::CreateOrNull(
            reinterpret_cast<const unsigned char*>(buffer),
            base::checked_cast<uint32_t>(available));
      }
      result = consumer_->EndRead(available);
      if (!array && !byob_view) {
        RaiseOOMError();
        return;
      }
    }
    switch (result) {
      case BytesConsumer::Result::kOk:
      case BytesConsumer::Result::kDone:
        if (array || byob_view) {
          // Clear |stream_needs_more_| in order to detect a pull call.
          stream_needs_more_ = false;
          if (underlying_byte_source_) {
            ScriptState::Scope scope(script_state_);
            auto* byte_controller =
                To<ReadableByteStreamController>(stream_->GetController());
            if (byob_view) {
              ReadableByteStreamController::Respond(
                  script_state_, byte_controller, available, exception_state);
            } else {
              CHECK(array);
              ReadableByteStreamController::Enqueue(
                  script_state_, byte_controller, NotShared(array),
                  exception_state);
            }
            if (exception_state.HadException()) {
              exception_state.ClearException();
              return;
            }
          } else {
            CHECK(underlying_source_);
            CHECK(array);
            underlying_source_->Controller()->Enqueue(array);
          }
        }
        if (result == BytesConsumer::Result::kDone) {
          Close(exception_state);
          return;
        }
        // If |stream_needs_more_| is true, it means that pull is called and
        // the stream needs more data even if the desired size is not
        // positive.
        if (!stream_needs_more_) {
          if (underlying_byte_source_) {
            auto* byte_controller =
                To<ReadableByteStreamController>(stream_->GetController());
            absl::optional<double> desired_size =
                ReadableByteStreamController::GetDesiredSize(byte_controller);
            DCHECK(desired_size.has_value());
            stream_needs_more_ = desired_size.value() > 0;
          } else {
            CHECK(underlying_source_);
            stream_needs_more_ =
                underlying_source_->Controller()->DesiredSize() > 0;
          }
        }
        break;
      case BytesConsumer::Result::kShouldWait:
        NOTREACHED();
        return;
      case BytesConsumer::Result::kError:
        GetError();
        return;
    }
  }
}

void BodyStreamBuffer::EndLoading() {
  if (!loader_) {
    DCHECK(!keep_alive_);
    return;
  }
  virtual_time_pauser_.UnpauseVirtualTime();
  keep_alive_.Clear();
  loader_ = nullptr;
}

void BodyStreamBuffer::StopLoading() {
  if (!loader_) {
    DCHECK(!keep_alive_);
    return;
  }
  loader_->Cancel();
  EndLoading();
}

BytesConsumer* BodyStreamBuffer::ReleaseHandle(
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());

  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be inspected");
    return nullptr;
  }

  if (!GetExecutionContext()) {
    // Avoid crashing if ContextDestroyed() has been called.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot release body in a window or worker than has been detached");
    return nullptr;
  }

  // Do this after state checks to avoid side-effects when the method does
  // nothing.
  side_data_blob_.reset();

  if (made_from_readable_stream_) {
    DCHECK(script_state_->ContextIsValid());
    ScriptState::Scope scope(script_state_);
    return MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state_,
                                                             stream_);
  }
  // We need to call these before calling CloseAndLockAndDisturb.
  const bool is_closed = IsStreamClosed();
  const bool is_errored = IsStreamErrored();

  BytesConsumer* consumer = consumer_.Release();

  CloseAndLockAndDisturb(exception_state);

  if (is_closed) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    return BytesConsumer::CreateClosed();
  }
  if (is_errored)
    return BytesConsumer::CreateErrored(BytesConsumer::Error("error"));

  DCHECK(consumer);
  consumer->ClearClient();
  return consumer;
}

}  // namespace blink
