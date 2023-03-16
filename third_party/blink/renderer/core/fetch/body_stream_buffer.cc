// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_tee.h"
#include "third_party/blink/renderer/core/fetch/bytes_uploader.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      consumer_(consumer),
      signal_(signal),
      cached_metadata_handler_(cached_metadata_handler),
      side_data_blob_(std::move(side_data_blob)),
      made_from_readable_stream_(false) {}

void BodyStreamBuffer::Init() {
  DCHECK(consumer_);

  stream_ =
      ReadableStream::CreateWithCountQueueingStrategy(script_state_, this, 0);
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
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      stream_(stream),
      signal_(nullptr),
      cached_metadata_handler_(cached_metadata_handler),
      side_data_blob_(std::move(side_data_blob)),
      made_from_readable_stream_(true) {
  DCHECK(stream_);
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::DrainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy) {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored() || stream_broken_)
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer_->DrainAsBlobDataHandle(policy);
  if (blob_data_handle) {
    CloseAndLockAndDisturb();
    return blob_data_handle;
  }
  return nullptr;
}

scoped_refptr<EncodedFormData> BodyStreamBuffer::DrainAsFormData() {
  DCHECK(!IsStreamLocked());
  DCHECK(!IsStreamDisturbed());
  if (IsStreamClosed() || IsStreamErrored() || stream_broken_)
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<EncodedFormData> form_data = consumer_->DrainAsFormData();
  if (form_data) {
    CloseAndLockAndDisturb();
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

    stream_->Tee(script_state_, &stream1, &stream2, exception_state);
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

ScriptPromise BodyStreamBuffer::pull(ScriptState* script_state) {
  DCHECK_EQ(script_state, script_state_);
  if (!consumer_) {
    // This is a speculative workaround for a crash. See
    // https://crbug.com/773525.
    // TODO(yhirano): Remove this branch or have a better comment.
    return ScriptPromise::CastUndefined(script_state);
  }

  if (stream_needs_more_)
    return ScriptPromise::CastUndefined(script_state);
  stream_needs_more_ = true;
  if (!in_process_data_)
    ProcessData();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise BodyStreamBuffer::Cancel(ScriptState* script_state,
                                       ScriptValue reason) {
  DCHECK_EQ(script_state, script_state_);
  Controller()->Close();
  CancelConsumer();
  return ScriptPromise::CastUndefined(script_state);
}

void BodyStreamBuffer::OnStateChange() {
  if (!consumer_ || !GetExecutionContext() ||
      GetExecutionContext()->IsContextDestroyed())
    return;

  switch (consumer_->GetPublicState()) {
    case BytesConsumer::PublicState::kReadableOrWaiting:
      break;
    case BytesConsumer::PublicState::kClosed:
      Close();
      return;
    case BytesConsumer::PublicState::kErrored:
      GetError();
      return;
  }
  ProcessData();
}

void BodyStreamBuffer::ContextDestroyed() {
  CancelConsumer();
  UnderlyingSourceBase::ContextDestroyed();
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

void BodyStreamBuffer::CloseAndLockAndDisturb() {
  DCHECK(!stream_broken_);

  cached_metadata_handler_ = nullptr;

  if (IsStreamReadable()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    Close();
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
  visitor->Trace(stream_uploader_);
  visitor->Trace(consumer_);
  visitor->Trace(loader_);
  visitor->Trace(signal_);
  visitor->Trace(stream_buffer_abort_handle_);
  visitor->Trace(loader_client_abort_handle_);
  visitor->Trace(cached_metadata_handler_);
  UnderlyingSourceBase::Trace(visitor);
}

void BodyStreamBuffer::Abort() {
  if (!GetExecutionContext()) {
    DCHECK(!consumer_);
    return;
  }
  Controller()->Error(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  CancelConsumer();
}

void BodyStreamBuffer::Close() {
  // Close() can be called during construction, in which case Controller()
  // will not be set yet.
  if (Controller())
    Controller()->Close();
  CancelConsumer();
}

void BodyStreamBuffer::GetError() {
  {
    ScriptState::Scope scope(script_state_);
    Controller()->Error(V8ThrowException::CreateTypeError(
        script_state_->GetIsolate(), "network error"));
  }
  CancelConsumer();
}

void BodyStreamBuffer::RaiseOOMError() {
  {
    ScriptState::Scope scope(script_state_);
    Controller()->Error(V8ThrowException::CreateRangeError(
        script_state_->GetIsolate(), "Array buffer allocation failed"));
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

void BodyStreamBuffer::ProcessData() {
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
    if (result == BytesConsumer::Result::kOk) {
      array = DOMUint8Array::CreateOrNull(
          reinterpret_cast<const unsigned char*>(buffer),
          base::checked_cast<uint32_t>(available));
      result = consumer_->EndRead(available);
      if (!array) {
        RaiseOOMError();
        return;
      }
    }
    switch (result) {
      case BytesConsumer::Result::kOk:
      case BytesConsumer::Result::kDone:
        if (array) {
          // Clear m_streamNeedsMore in order to detect a pull call.
          stream_needs_more_ = false;
          Controller()->Enqueue(array);
        }
        if (result == BytesConsumer::Result::kDone) {
          Close();
          return;
        }
        // If m_streamNeedsMore is true, it means that pull is called and
        // the stream needs more data even if the desired size is not
        // positive.
        if (!stream_needs_more_)
          stream_needs_more_ = Controller()->DesiredSize() > 0;
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
  DCHECK(loader_);
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

  CloseAndLockAndDisturb();

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
