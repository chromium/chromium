// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/public/platform/web_data_consumer_handle.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"

namespace blink {

namespace {

class BodyConsumerBase : public GarbageCollectedFinalized<BodyConsumerBase>,
                         public FetchDataLoader::Client {
  USING_GARBAGE_COLLECTED_MIXIN(BodyConsumerBase);

 public:
  explicit BodyConsumerBase(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ScriptPromiseResolver* Resolver() { return resolver_; }
  void DidFetchDataLoadFailed() override {
    ScriptState::Scope scope(Resolver()->GetScriptState());
    resolver_->Reject(V8ThrowException::CreateTypeError(
        Resolver()->GetScriptState()->GetIsolate(), "Failed to fetch"));
  }

  void Abort() override {
    resolver_->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(resolver_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  DISALLOW_COPY_AND_ASSIGN(BodyConsumerBase);
};

class BodyBlobConsumer final : public BodyConsumerBase {
 public:
  explicit BodyBlobConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> blob_data_handle) override {
    Resolver()->Resolve(Blob::Create(std::move(blob_data_handle)));
  }
  DISALLOW_COPY_AND_ASSIGN(BodyBlobConsumer);
};

class BodyArrayBufferConsumer final : public BodyConsumerBase {
 public:
  explicit BodyArrayBufferConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    Resolver()->Resolve(array_buffer);
  }
  DISALLOW_COPY_AND_ASSIGN(BodyArrayBufferConsumer);
};

class BodyFormDataConsumer final : public BodyConsumerBase {
 public:
  explicit BodyFormDataConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}

  void DidFetchDataLoadedFormData(FormData* formData) override {
    Resolver()->Resolve(formData);
  }

  void DidFetchDataLoadedString(const String& string) override {
    FormData* formData = FormData::Create();
    for (const auto& pair : URLSearchParams::Create(string)->Params())
      formData->append(pair.first, pair.second);
    DidFetchDataLoadedFormData(formData);
  }
  DISALLOW_COPY_AND_ASSIGN(BodyFormDataConsumer);
};

class BodyTextConsumer final : public BodyConsumerBase {
 public:
  explicit BodyTextConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}

  void DidFetchDataLoadedString(const String& string) override {
    Resolver()->Resolve(string);
  }
  DISALLOW_COPY_AND_ASSIGN(BodyTextConsumer);
};

class BodyJsonConsumer final : public BodyConsumerBase {
 public:
  explicit BodyJsonConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}

  void DidFetchDataLoadedString(const String& string) override {
    if (!Resolver()->GetExecutionContext() ||
        Resolver()->GetExecutionContext()->IsContextDestroyed())
      return;
    ScriptState::Scope scope(Resolver()->GetScriptState());
    v8::Isolate* isolate = Resolver()->GetScriptState()->GetIsolate();
    v8::Local<v8::String> input_string = V8String(isolate, string);
    v8::TryCatch trycatch(isolate);
    v8::Local<v8::Value> parsed;
    if (v8::JSON::Parse(Resolver()->GetScriptState()->GetContext(),
                        input_string)
            .ToLocal(&parsed))
      Resolver()->Resolve(parsed);
    else
      Resolver()->Reject(trycatch.Exception());
  }
  DISALLOW_COPY_AND_ASSIGN(BodyJsonConsumer);
};

}  // namespace

ScriptPromise Body::arrayBuffer(ScriptState* script_state,
                                ExceptionState& exception_state) {
  RejectInvalidConsumption(script_state, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // When the main thread sends a V8::TerminateExecution() signal to a worker
  // thread, any V8 API on the worker thread starts returning an empty
  // handle. This can happen in this function. To avoid the situation, we
  // first check the ExecutionContext and return immediately if it's already
  // gone (which means that the V8::TerminateExecution() signal has been sent
  // to this worker thread).
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(FetchDataLoader::CreateLoaderAsArrayBuffer(),
                               new BodyArrayBufferConsumer(resolver),
                               exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    resolver->Resolve(DOMArrayBuffer::Create(0u, 1));
  }
  return promise;
}

ScriptPromise Body::blob(ScriptState* script_state,
                         ExceptionState& exception_state) {
  RejectInvalidConsumption(script_state, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(
        FetchDataLoader::CreateLoaderAsBlobHandle(MimeType()),
        new BodyBlobConsumer(resolver), exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->SetContentType(MimeType());
    resolver->Resolve(
        Blob::Create(BlobDataHandle::Create(std::move(blob_data), 0)));
  }
  return promise;
}

ScriptPromise Body::formData(ScriptState* script_state,
                             ExceptionState& exception_state) {
  RejectInvalidConsumption(script_state, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  const ParsedContentType parsedTypeWithParameters(ContentType());
  const String parsedType = parsedTypeWithParameters.MimeType().LowerASCII();
  ScriptPromise promise = resolver->Promise();
  if (parsedType == "multipart/form-data") {
    const String boundary =
        parsedTypeWithParameters.ParameterValueForName("boundary");
    auto* body_buffer = BodyBuffer();
    if (body_buffer && !boundary.IsEmpty()) {
      body_buffer->StartLoading(
          FetchDataLoader::CreateLoaderAsFormData(boundary),
          new BodyFormDataConsumer(resolver), exception_state);
      if (exception_state.HadException()) {
        // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
        resolver->Resolve();
        return ScriptPromise();
      }
      return promise;
    }
  } else if (parsedType == "application/x-www-form-urlencoded") {
    if (BodyBuffer()) {
      BodyBuffer()->StartLoading(FetchDataLoader::CreateLoaderAsString(),
                                 new BodyFormDataConsumer(resolver),
                                 exception_state);
      if (exception_state.HadException()) {
        // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
        resolver->Resolve();
        return ScriptPromise();
      }
    } else {
      resolver->Resolve(FormData::Create());
    }
    return promise;
  } else {
    if (BodyBuffer()) {
      BodyBuffer()->StartLoading(FetchDataLoader::CreateLoaderAsFailure(),
                                 new BodyFormDataConsumer(resolver),
                                 exception_state);
      if (exception_state.HadException()) {
        // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
        resolver->Resolve();
        return ScriptPromise();
      }
      return promise;
    }
  }

  resolver->Reject(V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                                     "Invalid MIME type"));
  return promise;
}

ScriptPromise Body::json(ScriptState* script_state,
                         ExceptionState& exception_state) {
  RejectInvalidConsumption(script_state, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(FetchDataLoader::CreateLoaderAsString(),
                               new BodyJsonConsumer(resolver), exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    resolver->Reject(V8ThrowException::CreateSyntaxError(
        script_state->GetIsolate(), "Unexpected end of input"));
  }
  return promise;
}

ScriptPromise Body::text(ScriptState* script_state,
                         ExceptionState& exception_state) {
  RejectInvalidConsumption(script_state, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(FetchDataLoader::CreateLoaderAsString(),
                               new BodyTextConsumer(resolver), exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    resolver->Resolve(String());
  }
  return promise;
}

ScriptValue Body::body(ScriptState* script_state) {
  if (!BodyBuffer())
    return ScriptValue::CreateNull(script_state);
  ScriptValue stream = BodyBuffer()->Stream();
  DCHECK_EQ(stream.GetScriptState(), script_state);
  return stream;
}

Body::BodyUsed Body::IsBodyUsed(ExceptionState& exception_state) {
  auto* body_buffer = BodyBuffer();
  if (!body_buffer)
    return BodyUsed::kUnused;
  base::Optional<bool> stream_disturbed =
      body_buffer->IsStreamDisturbed(exception_state);
  if (exception_state.HadException())
    return BodyUsed::kBroken;
  return stream_disturbed.value() ? BodyUsed::kUsed : BodyUsed::kUnused;
}

Body::BodyLocked Body::IsBodyLocked(ExceptionState& exception_state) {
  auto* body_buffer = BodyBuffer();
  if (!body_buffer)
    return BodyLocked::kUnlocked;
  base::Optional<bool> is_locked = body_buffer->IsStreamLocked(exception_state);
  if (exception_state.HadException())
    return BodyLocked::kBroken;
  return is_locked.value() ? BodyLocked::kLocked : BodyLocked::kUnlocked;
}

bool Body::HasPendingActivity() const {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return false;
  auto* body_buffer = BodyBuffer();
  if (!body_buffer)
    return false;
  return body_buffer->HasPendingActivity();
}

bool Body::IsBodyUsedForDCheck() {
  return BodyBuffer() && BodyBuffer()->IsStreamDisturbedForDCheck();
}

Body::Body(ExecutionContext* context) : ContextClient(context) {}

void Body::RejectInvalidConsumption(ScriptState* script_state,
                                    ExceptionState& exception_state) {
  const auto used = IsBodyUsed(exception_state);
  if (exception_state.HadException()) {
    DCHECK_EQ(used, BodyUsed::kBroken);
    return;
  }
  DCHECK_NE(used, BodyUsed::kBroken);

  if (IsBodyLocked(exception_state) == BodyLocked::kLocked) {
    DCHECK(!exception_state.HadException());
    exception_state.ThrowTypeError("body stream is locked");
  }
  if (exception_state.HadException())
    return;

  if (used == BodyUsed::kUsed)
    exception_state.ThrowTypeError("body stream already read");
}

}  // namespace blink
