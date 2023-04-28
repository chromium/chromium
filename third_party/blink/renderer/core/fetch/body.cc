// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BodyConsumerBaseFetchCheckPoint {
  kConstructor = 0,
  kDidFetchDataLoadFailed = 1,
  kMaxValue = kDidFetchDataLoadFailed,
};

void SendHistogram(BodyConsumerBaseFetchCheckPoint cp) {
  base::UmaHistogramEnumeration("Net.Fetch.CheckPoint.BodyConsumerBase", cp);
}

class BodyConsumerBase : public GarbageCollected<BodyConsumerBase>,
                         public FetchDataLoader::Client {
 public:
  explicit BodyConsumerBase(ScriptPromiseResolver* resolver)
      : resolver_(resolver),
        task_runner_(ExecutionContext::From(resolver_->GetScriptState())
                         ->GetTaskRunner(TaskType::kNetworking)) {
    SendHistogram(BodyConsumerBaseFetchCheckPoint::kConstructor);
  }
  BodyConsumerBase(const BodyConsumerBase&) = delete;
  BodyConsumerBase& operator=(const BodyConsumerBase&) = delete;

  ScriptPromiseResolver* Resolver() { return resolver_; }
  void DidFetchDataLoadFailed() override {
    ScriptState::Scope scope(Resolver()->GetScriptState());
    resolver_->Reject(V8ThrowException::CreateTypeError(
        Resolver()->GetScriptState()->GetIsolate(), "Failed to fetch"));
    SendHistogram(BodyConsumerBaseFetchCheckPoint::kDidFetchDataLoadFailed);
  }

  void Abort() override {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  // Resource Timing event is not yet added, so delay the resolution timing
  // a bit. See https://crbug.com/507169.
  // TODO(yhirano): Fix this problem in a more sophisticated way.
  template <typename T>
  void ResolveLater(const T& object) {
    task_runner_->PostTask(FROM_HERE,
                           WTF::BindOnce(&BodyConsumerBase::ResolveNow<T>,
                                         WrapPersistent(this), object));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  template <typename T>
  void ResolveNow(const T& object) {
    resolver_->Resolve(object);
  }

  const Member<ScriptPromiseResolver> resolver_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class BodyBlobConsumer final : public BodyConsumerBase {
 public:
  explicit BodyBlobConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}
  BodyBlobConsumer(const BodyBlobConsumer&) = delete;
  BodyBlobConsumer& operator=(const BodyBlobConsumer&) = delete;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> blob_data_handle) override {
    ResolveLater(WrapPersistent(
        MakeGarbageCollected<Blob>(std::move(blob_data_handle))));
  }
};

class BodyArrayBufferConsumer final : public BodyConsumerBase {
 public:
  explicit BodyArrayBufferConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}
  BodyArrayBufferConsumer(const BodyArrayBufferConsumer&) = delete;
  BodyArrayBufferConsumer& operator=(const BodyArrayBufferConsumer&) = delete;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    ResolveLater(WrapPersistent(array_buffer));
  }
};

class BodyFormDataConsumer final : public BodyConsumerBase {
 public:
  explicit BodyFormDataConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}
  BodyFormDataConsumer(const BodyFormDataConsumer&) = delete;
  BodyFormDataConsumer& operator=(const BodyFormDataConsumer&) = delete;

  void DidFetchDataLoadedFormData(FormData* formData) override {
    ResolveLater(WrapPersistent(formData));
  }

  void DidFetchDataLoadedString(const String& string) override {
    auto* formData = MakeGarbageCollected<FormData>();
    for (const auto& pair : URLSearchParams::Create(string)->Params())
      formData->append(pair.first, pair.second);
    DidFetchDataLoadedFormData(formData);
  }
};

class BodyTextConsumer final : public BodyConsumerBase {
 public:
  explicit BodyTextConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}
  BodyTextConsumer(const BodyTextConsumer&) = delete;
  BodyTextConsumer& operator=(const BodyTextConsumer&) = delete;

  void DidFetchDataLoadedString(const String& string) override {
    ResolveLater(string);
  }
};

class BodyJsonConsumer final : public BodyConsumerBase {
 public:
  explicit BodyJsonConsumer(ScriptPromiseResolver* resolver)
      : BodyConsumerBase(resolver) {}
  BodyJsonConsumer(const BodyJsonConsumer&) = delete;
  BodyJsonConsumer& operator=(const BodyJsonConsumer&) = delete;

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
            .ToLocal(&parsed)) {
      ResolveLater(WrapPersistent(WrapDisallowNew(
          ScriptValue(Resolver()->GetScriptState()->GetIsolate(), parsed))));
    } else
      Resolver()->Reject(trycatch.Exception());
  }
};

}  // namespace

ScriptPromise Body::arrayBuffer(ScriptState* script_state,
                                ExceptionState& exception_state) {
  RejectInvalidConsumption(exception_state);
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

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(
        FetchDataLoader::CreateLoaderAsArrayBuffer(),
        MakeGarbageCollected<BodyArrayBufferConsumer>(resolver),
        exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    resolver->Resolve(DOMArrayBuffer::Create(size_t{0}, size_t{0}));
  }
  return promise;
}

ScriptPromise Body::blob(ScriptState* script_state,
                         ExceptionState& exception_state) {
  RejectInvalidConsumption(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    ExecutionContext* context = ExecutionContext::From(script_state);
    BodyBuffer()->StartLoading(
        FetchDataLoader::CreateLoaderAsBlobHandle(
            MimeType(), context->GetTaskRunner(TaskType::kNetworking)),
        MakeGarbageCollected<BodyBlobConsumer>(resolver), exception_state);
    if (exception_state.HadException()) {
      // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
      resolver->Resolve();
      return ScriptPromise();
    }
  } else {
    auto blob_data = std::make_unique<BlobData>();
    blob_data->SetContentType(MimeType());
    resolver->Resolve(MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data), 0)));
  }
  return promise;
}

ScriptPromise Body::formData(ScriptState* script_state,
                             ExceptionState& exception_state) {
  RejectInvalidConsumption(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  const ParsedContentType parsedTypeWithParameters(ContentType());
  const String parsedType = parsedTypeWithParameters.MimeType().LowerASCII();
  ScriptPromise promise = resolver->Promise();
  if (parsedType == "multipart/form-data") {
    const String boundary =
        parsedTypeWithParameters.ParameterValueForName("boundary");
    auto* body_buffer = BodyBuffer();
    if (body_buffer && !boundary.empty()) {
      body_buffer->StartLoading(
          FetchDataLoader::CreateLoaderAsFormData(boundary),
          MakeGarbageCollected<BodyFormDataConsumer>(resolver),
          exception_state);
      if (exception_state.HadException()) {
        // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
        resolver->Resolve();
        return ScriptPromise();
      }
      return promise;
    }
  } else if (parsedType == "application/x-www-form-urlencoded") {
    if (BodyBuffer()) {
      // According to https://fetch.spec.whatwg.org/#concept-body-package-data
      // application/x-www-form-urlencoded FormData bytes are parsed using
      // https://url.spec.whatwg.org/#concept-urlencoded-parser
      // which does not decode BOM.
      BodyBuffer()->StartLoading(
          FetchDataLoader::CreateLoaderAsString(
              TextResourceDecoderOptions::CreateUTF8DecodeWithoutBOM()),
          MakeGarbageCollected<BodyFormDataConsumer>(resolver),
          exception_state);
      if (exception_state.HadException()) {
        // Need to resolve the ScriptPromiseResolver to avoid a DCHECK().
        resolver->Resolve();
        return ScriptPromise();
      }
    } else {
      resolver->Resolve(MakeGarbageCollected<FormData>());
    }
    return promise;
  } else {
    if (BodyBuffer()) {
      BodyBuffer()->StartLoading(
          FetchDataLoader::CreateLoaderAsFailure(),
          MakeGarbageCollected<BodyFormDataConsumer>(resolver),
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
  RejectInvalidConsumption(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(
        FetchDataLoader::CreateLoaderAsString(
            TextResourceDecoderOptions::CreateUTF8Decode()),
        MakeGarbageCollected<BodyJsonConsumer>(resolver), exception_state);
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
  RejectInvalidConsumption(exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // See above comment.
  if (!ExecutionContext::From(script_state))
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();
  if (BodyBuffer()) {
    BodyBuffer()->StartLoading(
        FetchDataLoader::CreateLoaderAsString(
            TextResourceDecoderOptions::CreateUTF8Decode()),
        MakeGarbageCollected<BodyTextConsumer>(resolver), exception_state);
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

ReadableStream* Body::body() {
  if (auto* execution_context = GetExecutionContext()) {
    if (execution_context->IsServiceWorkerGlobalScope()) {
      execution_context->CountUse(WebFeature::kFetchBodyStreamInServiceWorker);
    } else {
      execution_context->CountUse(
          WebFeature::kFetchBodyStreamOutsideServiceWorker);
    }
  }

  if (!BodyBuffer())
    return nullptr;
  return BodyBuffer()->Stream();
}

bool Body::IsBodyUsed() const {
  auto* body_buffer = BodyBuffer();
  return body_buffer && body_buffer->IsStreamDisturbed();
}

bool Body::IsBodyLocked() const {
  auto* body_buffer = BodyBuffer();
  return body_buffer && body_buffer->IsStreamLocked();
}

bool Body::HasPendingActivity() const {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return false;
  auto* body_buffer = BodyBuffer();
  if (!body_buffer)
    return false;
  return body_buffer->HasPendingActivity();
}

Body::Body(ExecutionContext* context) : ExecutionContextClient(context) {}

void Body::RejectInvalidConsumption(ExceptionState& exception_state) const {
  if (IsBodyLocked()) {
    exception_state.ThrowTypeError("body stream is locked");
  }

  if (IsBodyUsed()) {
    exception_state.ThrowTypeError("body stream already read");
  }
}

}  // namespace blink
