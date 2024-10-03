// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
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

class BodyConsumerBase : public GarbageCollected<BodyConsumerBase>,
                         public FetchDataLoader::Client {
 public:
  explicit BodyConsumerBase(ScriptPromiseResolverBase* resolver)
      : resolver_(resolver),
        task_runner_(ExecutionContext::From(resolver_->GetScriptState())
                         ->GetTaskRunner(TaskType::kNetworking)) {
  }
  BodyConsumerBase(const BodyConsumerBase&) = delete;
  BodyConsumerBase& operator=(const BodyConsumerBase&) = delete;

  ScriptPromiseResolverBase* Resolver() { return resolver_.Get(); }
  void DidFetchDataLoadFailed() override {
    ScriptState::Scope scope(Resolver()->GetScriptState());
    resolver_->Reject(V8ThrowException::CreateTypeError(
        Resolver()->GetScriptState()->GetIsolate(), "Failed to fetch"));
  }

  void Abort() override {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  // Resource Timing event is not yet added, so delay the resolution timing
  // a bit. See https://crbug.com/507169.
  // TODO(yhirano): Fix this problem in a more sophisticated way.
  template <typename IDLType, typename T>
  void ResolveLater(const T& object) {
    task_runner_->PostTask(
        FROM_HERE, WTF::BindOnce(&BodyConsumerBase::ResolveNow<IDLType, T>,
                                 WrapPersistent(this), object));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  template <typename IDLType, typename T>
    requires(
        !std::is_same<T, Persistent<DisallowNewWrapper<ScriptValue>>>::value)
  void ResolveNow(const T& object) {
    resolver_->DowncastTo<IDLType>()->Resolve(object);
  }

  template <typename IDLType, typename T>
    requires std::is_same<T, Persistent<DisallowNewWrapper<ScriptValue>>>::value
  void ResolveNow(const Persistent<DisallowNewWrapper<ScriptValue>>& object) {
    resolver_->DowncastTo<IDLType>()->Resolve(object->Value());
  }

  const Member<ScriptPromiseResolverBase> resolver_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};
class BodyBlobConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = Blob;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> blob_data_handle) override {
    ResolveLater<ResolveType>(WrapPersistent(
        MakeGarbageCollected<Blob>(std::move(blob_data_handle))));
  }
};

class BodyArrayBufferConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = DOMArrayBuffer;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    ResolveLater<ResolveType>(WrapPersistent(array_buffer));
  }
};

class BodyUint8ArrayConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = NotShared<DOMUint8Array>;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    ResolveLater<ResolveType>(WrapPersistent(
        DOMUint8Array::Create(array_buffer, 0, array_buffer->ByteLength())));
  }
};

class BodyFormDataConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = FormData;

  void DidFetchDataLoadedFormData(FormData* form_data) override {
    ResolveLater<ResolveType>(WrapPersistent(form_data));
  }

  void DidFetchDataLoadedString(const String& string) override {
    auto* form_data = MakeGarbageCollected<FormData>();
    // URLSearchParams::Create() returns an on-heap object, but it can be
    // garbage collected, so making it a persistent variable on the stack
    // mitigates use-after-free scenarios. See crbug.com/1497997.
    Persistent<URLSearchParams> search_params = URLSearchParams::Create(string);
    for (const auto& [name, value] : search_params->Params()) {
      form_data->append(name, value);
    }
    DidFetchDataLoadedFormData(form_data);
  }
};

class BodyTextConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = IDLUSVString;

  void DidFetchDataLoadedString(const String& string) override {
    ResolveLater<ResolveType>(string);
  }
};

class BodyJsonConsumer final : public BodyConsumerBase {
 public:
  using BodyConsumerBase::BodyConsumerBase;
  using ResolveType = IDLAny;

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
      ResolveLater<ResolveType>(WrapPersistent(WrapDisallowNew(
          ScriptValue(Resolver()->GetScriptState()->GetIsolate(), parsed))));
    } else
      Resolver()->Reject(trycatch.Exception());
  }
};

FetchDataLoader* CreateLoaderAsStringWithUTF8Decode() {
  return FetchDataLoader::CreateLoaderAsString(
      TextResourceDecoderOptions::CreateUTF8Decode());
}

}  // namespace

bool Body::ShouldLoadBody(ScriptState* script_state,
                          ExceptionState& exception_state) {
  RejectInvalidConsumption(exception_state);
  if (exception_state.HadException())
    return false;

  // When the main thread sends a V8::TerminateExecution() signal to a worker
  // thread, any V8 API on the worker thread starts returning an empty
  // handle. This can happen in this function. To avoid the situation, we
  // first check the ExecutionContext and return immediately if it's already
  // gone (which means that the V8::TerminateExecution() signal has been sent
  // to this worker thread).
  return ExecutionContext::From(script_state);
}

// `Consumer` must be a subclass of BodyConsumerBase which takes a
// ScriptPromiseResolverBase* as its constructor argument. `create_loader`
// should take no arguments and return a FetchDataLoader*. `on_no_body` should
// take a ScriptPromiseResolverBase* object and resolve or reject it, returning
// nothing.
template <class Consumer,
          typename CreateLoaderFunction,
          typename OnNoBodyFunction>
ScriptPromise<typename Consumer::ResolveType> Body::LoadAndConvertBody(
    ScriptState* script_state,
    CreateLoaderFunction create_loader,
    OnNoBodyFunction on_no_body,
    ExceptionState& exception_state) {
  if (!ShouldLoadBody(script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<typename Consumer::ResolveType>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (auto* body_buffer = BodyBuffer()) {
    body_buffer->StartLoading(create_loader(),
                              MakeGarbageCollected<Consumer>(resolver),
                              exception_state);
    if (exception_state.HadException()) {
      resolver->Detach();
      return EmptyPromise();
    }
  } else {
    on_no_body(resolver);
  }
  return promise;
}

ScriptPromise<DOMArrayBuffer> Body::arrayBuffer(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto on_no_body = [](ScriptPromiseResolver<DOMArrayBuffer>* resolver) {
    resolver->Resolve(DOMArrayBuffer::Create(size_t{0}, size_t{0}));
  };

  return LoadAndConvertBody<BodyArrayBufferConsumer>(
      script_state, &FetchDataLoader::CreateLoaderAsArrayBuffer, on_no_body,
      exception_state);
}

ScriptPromise<Blob> Body::blob(ScriptState* script_state,
                               ExceptionState& exception_state) {
  auto create_loader = [this, script_state]() {
    ExecutionContext* context = ExecutionContext::From(script_state);
    return FetchDataLoader::CreateLoaderAsBlobHandle(
        MimeType(), context->GetTaskRunner(TaskType::kNetworking));
  };
  auto on_no_body = [this](ScriptPromiseResolver<Blob>* resolver) {
    auto blob_data = std::make_unique<BlobData>();
    blob_data->SetContentType(MimeType());
    resolver->Resolve(MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data), 0)));
  };

  return LoadAndConvertBody<BodyBlobConsumer>(script_state, create_loader,
                                              on_no_body, exception_state);
}

ScriptPromise<NotShared<DOMUint8Array>> Body::bytes(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto on_no_body =
      [](ScriptPromiseResolver<NotShared<DOMUint8Array>>* resolver) {
        resolver->Resolve(
            NotShared<DOMUint8Array>(DOMUint8Array::Create(size_t{0})));
      };

  return LoadAndConvertBody<BodyUint8ArrayConsumer>(
      script_state, &FetchDataLoader::CreateLoaderAsArrayBuffer, on_no_body,
      exception_state);
}

ScriptPromise<FormData> Body::formData(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  auto on_no_body_reject = [script_state](ScriptPromiseResolverBase* resolver) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Invalid MIME type"));
  };
  const ParsedContentType parsed_type_with_parameters(ContentType());
  const String parsed_type =
      parsed_type_with_parameters.MimeType().LowerASCII();
  if (parsed_type == "multipart/form-data") {
    const String boundary =
        parsed_type_with_parameters.ParameterValueForName("boundary");
    if (!boundary.empty()) {
      auto create_loader = [&boundary]() {
        return FetchDataLoader::CreateLoaderAsFormData(boundary);
      };
      return LoadAndConvertBody<BodyFormDataConsumer>(
          script_state, create_loader, on_no_body_reject, exception_state);
    }
    if (!ShouldLoadBody(script_state, exception_state)) {
      return EmptyPromise();
    }
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<FormData>>(
        script_state, exception_state.GetContext());
    auto promise = resolver->Promise();
    on_no_body_reject(resolver);
    return promise;
  } else if (parsed_type == "application/x-www-form-urlencoded") {
    auto on_no_body_resolve = [](ScriptPromiseResolver<FormData>* resolver) {
      resolver->Resolve(MakeGarbageCollected<FormData>());
    };
    // According to https://fetch.spec.whatwg.org/#concept-body-package-data
    // application/x-www-form-urlencoded FormData bytes are parsed using
    // https://url.spec.whatwg.org/#concept-urlencoded-parser
    // which does not decode BOM.
    auto create_loader = []() {
      return FetchDataLoader::CreateLoaderAsString(
          TextResourceDecoderOptions::CreateUTF8DecodeWithoutBOM());
    };
    return LoadAndConvertBody<BodyFormDataConsumer>(
        script_state, create_loader, on_no_body_resolve, exception_state);
  } else {
    return LoadAndConvertBody<BodyFormDataConsumer>(
        script_state, &FetchDataLoader::CreateLoaderAsFailure,
        on_no_body_reject, exception_state);
  }
}

ScriptPromise<IDLAny> Body::json(ScriptState* script_state,
                                 ExceptionState& exception_state) {
  auto on_no_body = [script_state](ScriptPromiseResolverBase* resolver) {
    resolver->Reject(V8ThrowException::CreateSyntaxError(
        script_state->GetIsolate(), "Unexpected end of input"));
  };
  return LoadAndConvertBody<BodyJsonConsumer>(
      script_state, &CreateLoaderAsStringWithUTF8Decode, on_no_body,
      exception_state);
}

ScriptPromise<IDLUSVString> Body::text(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  auto on_no_body = [](ScriptPromiseResolver<IDLUSVString>* resolver) {
    resolver->Resolve(String());
  };
  return LoadAndConvertBody<BodyTextConsumer>(
      script_state, &CreateLoaderAsStringWithUTF8Decode, on_no_body,
      exception_state);
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

  if (auto* body_buffer = BodyBuffer()) {
    return body_buffer->Stream();
  }

  return nullptr;
}

bool Body::IsBodyUsed() const {
  auto* body_buffer = BodyBuffer();
  return body_buffer && body_buffer->IsStreamDisturbed();
}

bool Body::IsBodyLocked() const {
  auto* body_buffer = BodyBuffer();
  return body_buffer && body_buffer->IsStreamLocked();
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
