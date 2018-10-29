// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"

#include <memory>
#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_wrapper.h"
#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final
    : public GarbageCollectedFinalized<LoaderClient>,
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
  if (!RetainWrapperDuringConstruction(this, script_state))
    stream_broken_ = true;

  {
    // Leaving an exception pending will cause Blink to crash in the bindings
    // code later, so catch instead.
    v8::TryCatch try_catch(script_state->GetIsolate());
    ScriptValue strategy =
        ReadableStreamOperations::CreateCountQueuingStrategy(script_state, 0);
    if (!strategy.IsEmpty()) {
      ScriptValue readable_stream =
          ReadableStreamOperations::CreateReadableStream(script_state, this,
                                                         strategy);
      if (!readable_stream.IsEmpty()) {
        stream_.Set(script_state->GetIsolate(),
                    readable_stream.V8Value().As<v8::Object>());
      } else {
        stream_broken_ = true;
      }
    } else {
      stream_broken_ = true;
    }
    DCHECK_EQ(stream_broken_, try_catch.HasCaught());
  }
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
                                   ScriptValue stream,
                                   ExceptionState& exception_state)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      signal_(nullptr),
      made_from_readable_stream_(true) {
  // This is needed because sometimes a BodyStreamBuffer can be detached from
  // the owner object such as Request. We rely on the wrapper and
  // HasPendingActivity in such a case.
  RetainWrapperDuringConstruction(this, script_state);
  DCHECK(ReadableStreamOperations::IsReadableStreamForDCheck(script_state,
                                                             stream));

  stream_.Set(script_state->GetIsolate(), stream.V8Value().As<v8::Object>());
}

ScriptValue BodyStreamBuffer::Stream() {
  // Since this is the implementation of response.body, we return the stream
  // even if |stream_broken_| is true, so that expected JavaScript attribute
  // behaviour is not changed. User code is still permitted to access the
  // stream even when it has thrown an exception.
  return ScriptValue(script_state_,
                     stream_.NewLocal(script_state_->GetIsolate()));
}

scoped_refptr<BlobDataHandle> BodyStreamBuffer::DrainAsBlobDataHandle(
    BytesConsumer::BlobSizePolicy policy,
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck());
  DCHECK(!IsStreamDisturbedForDCheck());
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
  DCHECK(!IsStreamLockedForDCheck());
  DCHECK(!IsStreamDisturbedForDCheck());
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
  loader->Start(handle, new LoaderClient(ExecutionContext::From(script_state_),
                                         this, client));
}

void BodyStreamBuffer::Tee(BodyStreamBuffer** branch1,
                           BodyStreamBuffer** branch2,
                           ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck());
  DCHECK(!IsStreamDisturbedForDCheck());
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
    ScriptValue stream1, stream2;
    ReadableStreamOperations::Tee(script_state_, Stream(), &stream1, &stream2,
                                  exception_state);
    if (exception_state.HadException()) {
      stream_broken_ = true;
      return;
    }

    // Exceptions here imply that |stream1| and/or |stream2| are broken, not the
    // stream owned by this object, so we shouldn't set |stream_broken_|.
    auto* tmp1 = new BodyStreamBuffer(script_state_, stream1, exception_state);
    if (exception_state.HadException())
      return;
    auto* tmp2 = new BodyStreamBuffer(script_state_, stream2, exception_state);
    if (exception_state.HadException())
      return;
    *branch1 = tmp1;
    *branch2 = tmp2;
    return;
  }
  BytesConsumer* dest1 = nullptr;
  BytesConsumer* dest2 = nullptr;
  auto* handle = ReleaseHandle(exception_state);
  if (exception_state.HadException()) {
    stream_broken_ = true;
    return;
  }
  BytesConsumer::Tee(ExecutionContext::From(script_state_), handle, &dest1,
                     &dest2);
  *branch1 = new BodyStreamBuffer(script_state_, dest1, signal_);
  *branch2 = new BodyStreamBuffer(script_state_, dest2, signal_);
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
  if (loader_)
    return true;
  return UnderlyingSourceBase::HasPendingActivity();
}

void BodyStreamBuffer::ContextDestroyed(ExecutionContext* destroyed_context) {
  CancelConsumer();
  UnderlyingSourceBase::ContextDestroyed(destroyed_context);
}

base::Optional<bool> BodyStreamBuffer::IsStreamReadable(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(ReadableStreamOperations::IsReadable,
                                exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamClosed(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(ReadableStreamOperations::IsClosed,
                                exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamErrored(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(ReadableStreamOperations::IsErrored,
                                exception_state);
}

base::Optional<bool> BodyStreamBuffer::IsStreamLocked(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(ReadableStreamOperations::IsLocked,
                                exception_state);
}

bool BodyStreamBuffer::IsStreamLockedForDCheck() {
  return ReadableStreamOperations::IsLockedForDCheck(script_state_, Stream());
}

base::Optional<bool> BodyStreamBuffer::IsStreamDisturbed(
    ExceptionState& exception_state) {
  return BooleanStreamOperation(ReadableStreamOperations::IsDisturbed,
                                exception_state);
}

bool BodyStreamBuffer::IsStreamDisturbedForDCheck() {
  return ReadableStreamOperations::IsDisturbedForDCheck(script_state_,
                                                        Stream());
}

void BodyStreamBuffer::CloseAndLockAndDisturb(ExceptionState& exception_state) {
  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be disturbed");
    return;
  }

  if (stream_.IsEmpty()) {
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

  ScriptState::Scope scope(script_state_);

  const base::Optional<bool> is_locked = IsStreamLocked(exception_state);
  if (exception_state.HadException() || is_locked.value())
    return;

  ScriptValue reader = ReadableStreamOperations::GetReader(
      script_state_, Stream(), exception_state);
  if (exception_state.HadException()) {
    stream_broken_ = true;
    return;
  }

  ReadableStreamOperations::DefaultReaderRead(script_state_, reader);
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
  Controller()->GetError(DOMException::Create(DOMExceptionCode::kAbortError));
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
    Controller()->GetError(V8ThrowException::CreateTypeError(
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
      array = DOMUint8Array::Create(
          reinterpret_cast<const unsigned char*>(buffer), available);
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
    base::Optional<bool> (*predicate)(ScriptState*,
                                      ScriptValue,
                                      ExceptionState&),
    ExceptionState& exception_state) {
  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be inspected");
    return base::nullopt;
  }
  ScriptState::Scope scope(script_state_);
  base::Optional<bool> result =
      predicate(script_state_, Stream(), exception_state);
  if (exception_state.HadException()) {
    stream_broken_ = true;
    return base::nullopt;
  }
  return result;
}

BytesConsumer* BodyStreamBuffer::ReleaseHandle(
    ExceptionState& exception_state) {
  DCHECK(!IsStreamLockedForDCheck());
  DCHECK(!IsStreamDisturbedForDCheck());

  if (stream_broken_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Body stream has suffered a fatal error and cannot be inspected");
    return nullptr;
  }

  if (made_from_readable_stream_) {
    ScriptState::Scope scope(script_state_);
    // We need to have |reader| alive by some means (as noted in
    // ReadableStreamDataConsumerHandle). Based on the following facts:
    //  - This function is used only from Tee and StartLoading.
    //  - This branch cannot be taken when called from Tee.
    //  - StartLoading makes HasPendingActivity return true while loading.
    //  - ReadableStream holds a reference to |reader| inside JS.
    // we don't need to keep the reader explicitly.
    ScriptValue reader = ReadableStreamOperations::GetReader(
        script_state_, Stream(), exception_state);
    if (exception_state.HadException()) {
      stream_broken_ = true;
      return nullptr;
    }
    return new ReadableStreamBytesConsumer(script_state_, reader);
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
