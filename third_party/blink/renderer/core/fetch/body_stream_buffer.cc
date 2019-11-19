// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_tee.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final
    : public GarbageCollected<LoaderClient>,
      public ContextLifecycleObserver,
      public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);

 public:
  LoaderClient(ExecutionContext* execution_context,
               BodyStreamBuffer* buffer,
               FetchDataLoader::Client* client)
      : ContextLifecycleObserver(execution_context),
        buffer_(buffer),
        client_(client) {}

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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(buffer_);
    visitor->Trace(client_);
    ContextLifecycleObserver::Trace(visitor);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  void ContextDestroyed(ExecutionContext*) override { buffer_->StopLoading(); }

  Member<BodyStreamBuffer> buffer_;
  Member<FetchDataLoader::Client> client_;
  DISALLOW_COPY_AND_ASSIGN(LoaderClient);
};

BodyStreamBuffer::BodyStreamBuffer(ScriptState* script_state,
                                   BytesConsumer* consumer,
                                   AbortSignal* signal)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      consumer_(consumer),
      signal_(signal),
      made_from_readable_stream_(false) {
  // This CHECK is temporary to track down the cause of
  // https://crbug.com/1007162.
  // TODO(ricea): Remove it once we know whether it crashes or not.
  CHECK(consumer_);

  stream_ =
      ReadableStream::CreateWithCountQueueingStrategy(script_state_, this, 0);
  stream_broken_ = !stream_;

  // This CHECK is temporary to track down the cause of
  // https://crbug.com/1007162.
  // TODO(ricea): Remove it once we know whether it crashes or not.
  CHECK(consumer_);

  consumer_->SetClient(this);
  if (signal) {
    if (signal->aborted()) {
      Abort();
    } else {
      signal->AddAlgorithm(
          WTF::Bind(&BodyStreamBuffer::Abort, WrapWeakPersistent(this)));
    }
  }
  OnStateChange();
}

BodyStreamBuffer::BodyStreamBuffer(ScriptState* script_state,
                                   ReadableStream* stream)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      stream_(stream),
      signal_(nullptr),
      made_from_readable_stream_(true) {
  DCHECK(stream_);
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::DrainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy,
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck(exception_state));
  DCHECK(!IsStreamDisturbedForDCheck(exception_state));
  const base::Optional<bool> is_closed = IsStreamClosed(exception_state);
  if (exception_state.HadException() || is_closed.value())
    return nullptr;
  const base::Optional<bool> is_errored = IsStreamErrored(exception_state);
  if (exception_state.HadException() || is_errored.value())
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<BlobDataHandle> blob_data_handle =
      consumer_->DrainAsBlobDataHandle(policy);
  if (blob_data_handle) {
    CloseAndLockAndDisturb(exception_state);
    if (exception_state.HadException())
      return nullptr;
    return blob_data_handle;
  }
  return nullptr;
}

scoped_refptr<EncodedFormData> BodyStreamBuffer::DrainAsFormData(
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck(exception_state));
  DCHECK(!IsStreamDisturbedForDCheck(exception_state));
  const base::Optional<bool> is_closed = IsStreamClosed(exception_state);
  if (exception_state.HadException() || is_closed.value())
    return nullptr;
  const base::Optional<bool> is_errored = IsStreamErrored(exception_state);
  if (exception_state.HadException() || is_errored.value())
    return nullptr;

  if (made_from_readable_stream_)
    return nullptr;

  scoped_refptr<EncodedFormData> form_data = consumer_->DrainAsFormData();
  if (form_data) {
    CloseAndLockAndDisturb(exception_state);
    if (exception_state.HadException())
      return nullptr;
    return form_data;
  }
  return nullptr;
}

void BodyStreamBuffer::StartLoading(FetchDataLoader* loader,
                                    FetchDataLoader::Client* client,
                                    ExceptionState& exception_state) {
  DCHECK(!loader_);
  DCHECK(script_state_->ContextIsValid());
  loader_ = loader;
  if (signal_) {
    if (signal_->aborted()) {
      client->Abort();
      return;
    }
    signal_->AddAlgorithm(
        WTF::Bind(&FetchDataLoader::Client::Abort, WrapWeakPersistent(client)));
  }
  auto* handle = ReleaseHandle(exception_state);
  if (exception_state.HadException())
    return;
  loader->Start(handle,
                MakeGarbageCollected<LoaderClient>(
                    ExecutionContext::From(script_state_), this, client));
}

void BodyStreamBuffer::Tee(BodyStreamBuffer** branch1,
                           BodyStreamBuffer** branch2,
                           ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck(exception_state));
  DCHECK(!IsStreamDisturbedForDCheck(exception_state));
  *branch1 = nullptr;
  *branch2 = nullptr;

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

    *branch1 = MakeGarbageCollected<BodyStreamBuffer>(script_state_, stream1);
    *branch2 = MakeGarbageCollected<BodyStreamBuffer>(script_state_, stream2);
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
  *branch1 =
      MakeGarbageCollected<BodyStreamBuffer>(script_state_, dest1, signal_);
  *branch2 =
      MakeGarbageCollected<BodyStreamBuffer>(script_state_, dest2, signal_);
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
  if (Controller())
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

bool BodyStreamBuffer::HasPendingActivity() const {
  return loader_;
}

void BodyStreamBuffer::ContextDestroyed(ExecutionContext* destroyed_context) {
  CancelConsumer();
  UnderlyingSourceBase::ContextDestroyed(destroyed_context);
}

base::Optional<bool> BodyStreamBuffer::IsStreamReadable(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(&ReadableStream::IsReadable, exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamClosed(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(&ReadableStream::IsClosed, exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamErrored(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(&ReadableStream::IsErrored, exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamLocked(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(&ReadableStream::IsLocked, exception_state);
}

bool BodyStreamBuffer::IsStreamLockedForDCheck(
    ExceptionState& exception_state) {
  auto result = IsStreamLocked(exception_state);
  return !result || *result;
}

base::Optional<bool> BodyStreamBuffer::IsStreamDisturbed(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(&ReadableStream::IsDisturbed, exception_state);
}

bool BodyStreamBuffer::IsStreamDisturbedForDCheck(
    ExceptionState& exception_state) {
  auto result = IsStreamDisturbed(exception_state);
  return !result || *result;
}

void BodyStreamBuffer::CloseAndLockAndDisturb(ExceptionState& exception_state) {
  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be disturbed");
    return;
  }

  if (stream_->IsBroken()) {
    stream_broken_ = true;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has been lost and cannot be disturbed");
    return;
  }

  base::Optional<bool> is_readable = IsStreamReadable(exception_state);
  if (exception_state.HadException())
    return;

  DCHECK(is_readable.has_value());
  if (is_readable.value()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    Close();
  }
  DCHECK(!stream_broken_);

  stream_->LockAndDisturb(script_state_, exception_state);
}

bool BodyStreamBuffer::IsAborted() {
  if (!signal_)
    return false;
  return signal_->aborted();
}

void BodyStreamBuffer::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(stream_);
  visitor->Trace(consumer_);
  visitor->Trace(loader_);
  visitor->Trace(signal_);
  UnderlyingSourceBase::Trace(visitor);
}

void BodyStreamBuffer::Abort() {
  if (!Controller()) {
    DCHECK(!GetExecutionContext());
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

void BodyStreamBuffer::CancelConsumer() {
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
      array =
          DOMUint8Array::Create(reinterpret_cast<const unsigned char*>(buffer),
                                SafeCast<uint32_t>(available));
      result = consumer_->EndRead(available);
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
  loader_ = nullptr;
}

void BodyStreamBuffer::StopLoading() {
  if (!loader_)
    return;
  loader_->Cancel();
  loader_ = nullptr;
}

base::Optional<bool> BodyStreamBuffer::BooleanStreamOperation(
    base::Optional<bool> (ReadableStream::*predicate)(ScriptState*,
                                                      ExceptionState&) const,
    ExceptionState& exception_state) {
  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be inspected");
    return base::nullopt;
  }
  ScriptState::Scope scope(script_state_);
  base::Optional<bool> result =
      (stream_->*predicate)(script_state_, exception_state);
  if (exception_state.HadException()) {
    stream_broken_ = true;
    return base::nullopt;
  }
  return result;
}

BytesConsumer* BodyStreamBuffer::ReleaseHandle(
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck(exception_state));
  DCHECK(!IsStreamDisturbedForDCheck(exception_state));

  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be inspected");
    return nullptr;
  }

  if (made_from_readable_stream_) {
    ScriptState::Scope scope(script_state_);
    auto* consumer = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state_, stream_, exception_state);
    if (exception_state.HadException()) {
      stream_broken_ = true;
      return nullptr;
    }
    return consumer;
  }
  // We need to call these before calling CloseAndLockAndDisturb.
  const base::Optional<bool> is_closed = IsStreamClosed(exception_state);
  if (exception_state.HadException())
    return nullptr;
  const base::Optional<bool> is_errored = IsStreamErrored(exception_state);
  if (exception_state.HadException())
    return nullptr;

  BytesConsumer* consumer = consumer_.Release();

  CloseAndLockAndDisturb(exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (is_closed.value()) {
    // Note that the stream cannot be "draining", because it doesn't have
    // the internal buffer.
    return BytesConsumer::CreateClosed();
  }
  if (is_errored.value())
    return BytesConsumer::CreateErrored(BytesConsumer::Error("error"));

  DCHECK(consumer);
  consumer->ClearClient();
  return consumer;
}

}  // namespace blink
