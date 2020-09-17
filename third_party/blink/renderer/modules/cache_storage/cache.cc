// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/cache_storage/cache_storage_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_trace_utils.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

bool VaryHeaderContainsAsterisk(const Response* response) {
  const FetchHeaderList* headers = response->headers()->HeaderList();
  String varyHeader;
  if (headers->Get("vary", varyHeader)) {
    Vector<String> fields;
    varyHeader.Split(',', fields);
    return std::any_of(fields.begin(), fields.end(), [](const String& field) {
      return field.StripWhiteSpace() == "*";
    });
  }
  return false;
}

bool HasJavascriptMimeType(const Response* response) {
  // Strip charset parameters from the MIME type since MIMETypeRegistry does
  // not expect them to be present.
  auto mime_type =
      ExtractMIMETypeFromMediaType(AtomicString(response->InternalMIMEType()));
  return MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type);
}

enum class CodeCachePolicy {
  // Use the default policy.  Currently that policy generates full code cache
  // when a script is stored during service worker install.
  kAuto,
  // Do not generate code cache when putting a script in cache_storage.
  kNone,
};

CodeCachePolicy GetCodeCachePolicy(ExecutionContext* context,
                                   const Response* response) {
  DCHECK(context);
  if (!RuntimeEnabledFeatures::CacheStorageCodeCacheHintEnabled(context))
    return CodeCachePolicy::kAuto;

  // It's important we don't look at the header hint for opaque responses since
  // it could leak cross-origin information.
  if (response->GetResponse()->GetType() ==
      network::mojom::FetchResponseType::kOpaque) {
    return CodeCachePolicy::kAuto;
  }

  String header_name(
      features::kCacheStorageCodeCacheHintHeaderName.Get().data());
  String header_value;
  if (!response->InternalHeaderList()->Get(header_name, header_value))
    return CodeCachePolicy::kAuto;

  // Count the hint usage regardless of its value.
  context->CountUse(mojom::WebFeature::kCacheStorageCodeCacheHint);

  if (header_value.LowerASCII() == "none")
    return CodeCachePolicy::kNone;

  return CodeCachePolicy::kAuto;
}

bool ShouldGenerateV8CodeCache(ScriptState* script_state,
                               const Response* response) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
  if (!global_scope)
    return false;

  if (!response->InternalBodyBuffer())
    return false;

  if (!HasJavascriptMimeType(response))
    return false;

  auto policy = GetCodeCachePolicy(context, response);
  if (policy == CodeCachePolicy::kNone)
    return false;

  DCHECK_EQ(policy, CodeCachePolicy::kAuto);
  if (!global_scope->IsInstalling())
    return false;

  return true;
}

}  // namespace

// TODO(nhiroki): Unfortunately, we have to go through V8 to wait for the fetch
// promise. It should be better to achieve this only within C++ world.
class Cache::FetchResolvedForAdd final : public ScriptFunction {
 public:
  // |exception_state| is passed so that the context_type, interface_name and
  // property_name can be copied and then used to construct a new ExceptionState
  // object asynchronously later.
  static v8::Local<v8::Function> Create(
      ScriptState* script_state,
      Cache* cache,
      const String& method_name,
      const HeapVector<Member<Request>>& requests,
      const ExceptionState& exception_state,
      int64_t trace_id) {
    FetchResolvedForAdd* self = MakeGarbageCollected<FetchResolvedForAdd>(
        script_state, cache, method_name, requests, exception_state, trace_id);
    return self->BindToV8Function();
  }

  FetchResolvedForAdd(ScriptState* script_state,
                      Cache* cache,
                      const String& method_name,
                      const HeapVector<Member<Request>>& requests,
                      const ExceptionState& exception_state,
                      int64_t trace_id)
      : ScriptFunction(script_state),
        cache_(cache),
        method_name_(method_name),
        requests_(requests),
        context_type_(exception_state.Context()),
        property_name_(exception_state.PropertyName()),
        interface_name_(exception_state.InterfaceName()),
        trace_id_(trace_id) {}

  ScriptValue Call(ScriptValue value) override {
    TRACE_EVENT_WITH_FLOW0(
        "CacheStorage", "Cache::FetchResolverForAdd::Call",
        TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

    ExceptionState exception_state(GetScriptState()->GetIsolate(),
                                   context_type_, property_name_,
                                   interface_name_);
    HeapVector<Member<Response>> responses =
        NativeValueTraits<IDLSequence<Response>>::NativeValue(
            GetScriptState()->GetIsolate(), value.V8Value(), exception_state);
    if (exception_state.HadException()) {
      ScriptPromise rejection =
          ScriptPromise::Reject(GetScriptState(), exception_state);
      return ScriptValue(GetScriptState()->GetIsolate(), rejection.V8Value());
    }

    for (const auto& response : responses) {
      if (!response->ok()) {
        ScriptPromise rejection = ScriptPromise::Reject(
            GetScriptState(),
            V8ThrowException::CreateTypeError(GetScriptState()->GetIsolate(),
                                              "Request failed"));
        return ScriptValue(GetScriptState()->GetIsolate(), rejection.V8Value());
      }
      if (VaryHeaderContainsAsterisk(response)) {
        ScriptPromise rejection = ScriptPromise::Reject(
            GetScriptState(),
            V8ThrowException::CreateTypeError(GetScriptState()->GetIsolate(),
                                              "Vary header contains *"));
        return ScriptValue(GetScriptState()->GetIsolate(), rejection.V8Value());
      }
    }

    ScriptPromise put_promise =
        cache_->PutImpl(GetScriptState(), method_name_, requests_, responses,
                        exception_state, trace_id_);
    return ScriptValue(GetScriptState()->GetIsolate(), put_promise.V8Value());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(cache_);
    visitor->Trace(requests_);
    ScriptFunction::Trace(visitor);
  }

 private:
  Member<Cache> cache_;
  const String method_name_;
  HeapVector<Member<Request>> requests_;
  ExceptionState::ContextType context_type_;
  const char* property_name_;
  const char* interface_name_;
  const int64_t trace_id_;
};

class Cache::BarrierCallbackForPut final
    : public GarbageCollected<BarrierCallbackForPut> {
 public:
  BarrierCallbackForPut(wtf_size_t number_of_operations,
                        Cache* cache,
                        const String& method_name,
                        ScriptPromiseResolver* resolver,
                        int64_t trace_id)
      : number_of_remaining_operations_(number_of_operations),
        cache_(cache),
        method_name_(method_name),
        resolver_(resolver),
        trace_id_(trace_id) {
    DCHECK_LT(0, number_of_remaining_operations_);
    batch_operations_.resize(number_of_operations);
  }

  void OnSuccess(wtf_size_t index,
                 mojom::blink::BatchOperationPtr batch_operation) {
    DCHECK_LT(index, batch_operations_.size());
    TRACE_EVENT_WITH_FLOW1(
        "CacheStorage", "Cache::BarrierCallbackForPut::OnSuccess",
        TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "batch_operation",
        CacheStorageTracedValue(batch_operation));
    if (!StillActive())
      return;
    batch_operations_[index] = std::move(batch_operation);
    if (--number_of_remaining_operations_ != 0)
      return;
    MaybeReportInstalledScripts();
    int operation_count = batch_operations_.size();
    DCHECK_GE(operation_count, 1);
    // Make sure to bind the Cache object to keep the mojo remote alive during
    // the operation. Otherwise GC might prevent the callback from ever being
    // executed.
    cache_->cache_remote_->Batch(
        std::move(batch_operations_), trace_id_,
        WTF::Bind(
            [](const String& method_name, ScriptPromiseResolver* resolver,
               base::TimeTicks start_time, int operation_count,
               int64_t trace_id, Cache* _,
               mojom::blink::CacheStorageVerboseErrorPtr error) {
              base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage",
                  "Cache::BarrierCallbackForPut::OnSuccess::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(error->value));
              if (operation_count > 1) {
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.Cache.Renderer.PutMany", elapsed);
              } else {
                DCHECK_EQ(operation_count, 1);
                UMA_HISTOGRAM_LONG_TIMES(
                    "ServiceWorkerCache.Cache.Renderer.PutOne", elapsed);
              }
              ExecutionContext* context = resolver->GetExecutionContext();
              if (!context || context->IsContextDestroyed())
                return;
              if (error->value == mojom::blink::CacheStorageError::kSuccess) {
                resolver->Resolve();
              } else {
                StringBuilder message;
                if (error->message) {
                  message.Append(method_name);
                  message.Append(": ");
                  message.Append(error->message);
                }
                RejectCacheStorageWithError(resolver, error->value,
                                            message.ToString());
              }
            },
            method_name_, WrapPersistent(resolver_.Get()),
            base::TimeTicks::Now(), operation_count, trace_id_,
            WrapPersistent(cache_.Get())));
  }

  void OnError(const String& error_message) {
    if (!StillActive())
      return;
    completed_ = true;
    ScriptState* state = resolver_->GetScriptState();
    ScriptState::Scope scope(state);
    resolver_->Reject(
        V8ThrowException::CreateTypeError(state->GetIsolate(), error_message));
  }

  void Abort() {
    if (!StillActive())
      return;
    completed_ = true;
    ScriptState::Scope scope(resolver_->GetScriptState());
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
  }

  virtual void Trace(Visitor* visitor) const {
    visitor->Trace(cache_);
    visitor->Trace(resolver_);
  }

 private:
  bool StillActive() {
    if (completed_)
      return false;
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return false;

    return true;
  }

  // Report the script stats if this cache storage is for service worker
  // execution context and it's in installation phase.
  void MaybeReportInstalledScripts() {
    ExecutionContext* context = resolver_->GetExecutionContext();
    auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
    if (!global_scope)
      return;
    if (!global_scope->IsInstalling())
      return;

    for (const auto& operation : batch_operations_) {
      scoped_refptr<BlobDataHandle> blob_data_handle =
          operation->response->blob;
      if (!blob_data_handle)
        continue;
      if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
              blob_data_handle->GetType())) {
        continue;
      }
      uint64_t side_data_blob_size =
          operation->response->side_data_blob_for_cache_put
              ? operation->response->side_data_blob_for_cache_put->size()
              : 0;
      global_scope->CountCacheStorageInstalledScript(blob_data_handle->size(),
                                                     side_data_blob_size);
    }
  }

  bool completed_ = false;
  int number_of_remaining_operations_;
  Member<Cache> cache_;
  const String method_name_;
  Member<ScriptPromiseResolver> resolver_;
  Vector<mojom::blink::BatchOperationPtr> batch_operations_;
  const int64_t trace_id_;
};

class Cache::BlobHandleCallbackForPut final
    : public GarbageCollected<BlobHandleCallbackForPut>,
      public FetchDataLoader::Client {
 public:
  BlobHandleCallbackForPut(wtf_size_t index,
                           BarrierCallbackForPut* barrier_callback,
                           Request* request,
                           Response* response)
      : index_(index), barrier_callback_(barrier_callback) {
    fetch_api_request_ = request->CreateFetchAPIRequest();
    fetch_api_response_ = response->PopulateFetchAPIResponse(request->url());
  }
  ~BlobHandleCallbackForPut() override = default;

  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> handle) override {
    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = std::move(fetch_api_request_);
    batch_operation->response = std::move(fetch_api_response_);
    batch_operation->response->blob = handle;
    barrier_callback_->OnSuccess(index_, std::move(batch_operation));
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->OnError("network error");
  }

  void Abort() override { barrier_callback_->Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(barrier_callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  const wtf_size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;

  mojom::blink::FetchAPIRequestPtr fetch_api_request_;
  mojom::blink::FetchAPIResponsePtr fetch_api_response_;
};

class Cache::CodeCacheHandleCallbackForPut final
    : public GarbageCollected<CodeCacheHandleCallbackForPut>,
      public FetchDataLoader::Client {
 public:
  CodeCacheHandleCallbackForPut(ScriptState* script_state,
                                wtf_size_t index,
                                BarrierCallbackForPut* barrier_callback,
                                Request* request,
                                Response* response,
                                int64_t trace_id)
      : script_state_(script_state),
        index_(index),
        barrier_callback_(barrier_callback),
        mime_type_(response->InternalMIMEType()),
        trace_id_(trace_id) {
    fetch_api_request_ = request->CreateFetchAPIRequest();
    fetch_api_response_ = response->PopulateFetchAPIResponse(request->url());
    url_ = fetch_api_request_->url;
    opaque_mode_ = fetch_api_response_->response_type ==
                           network::mojom::FetchResponseType::kOpaque
                       ? V8CodeCache::OpaqueMode::kOpaque
                       : V8CodeCache::OpaqueMode::kNotOpaque;
  }
  ~CodeCacheHandleCallbackForPut() override = default;

  void DidFetchDataLoadedArrayBuffer(DOMArrayBuffer* array_buffer) override {
    TRACE_EVENT_WITH_FLOW1(
        "CacheStorage",
        "Cache::CodeCacheHandleCallbackForPut::DidFetchDataLoadedArrayBuffer",
        TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url",
        CacheStorageTracedValue(url_.GetString()));
    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = std::move(fetch_api_request_);
    batch_operation->response = std::move(fetch_api_response_);

    auto blob_data = std::make_unique<BlobData>();
    blob_data->SetContentType(mime_type_);
    blob_data->AppendBytes(array_buffer->Data(),
                           array_buffer->ByteLengthAsSizeT());
    batch_operation->response->blob = BlobDataHandle::Create(
        std::move(blob_data), array_buffer->ByteLengthAsSizeT());

    scoped_refptr<CachedMetadata> cached_metadata =
        GenerateFullCodeCache(array_buffer);
    if (cached_metadata) {
      base::span<const uint8_t> serialized_data =
          cached_metadata->SerializedData();
      auto side_data_blob_data = std::make_unique<BlobData>();
      side_data_blob_data->AppendBytes(serialized_data.data(),
                                       serialized_data.size());

      batch_operation->response->side_data_blob_for_cache_put =
          BlobDataHandle::Create(std::move(side_data_blob_data),
                                 serialized_data.size());
    }

    barrier_callback_->OnSuccess(index_, std::move(batch_operation));
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->OnError("network error");
  }

  void Abort() override { barrier_callback_->Abort(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(barrier_callback_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  ServiceWorkerGlobalScope* GetServiceWorkerGlobalScope() {
    ExecutionContext* context = ExecutionContext::From(script_state_);
    if (!context || context->IsContextDestroyed())
      return nullptr;
    // Currently |this| is only created for triggering V8 code caching after
    // Cache#put() is used by a service worker so |script_state_| should be
    // ServiceWorkerGlobalScope.
    auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context);
    DCHECK(global_scope);
    return global_scope;
  }

  scoped_refptr<CachedMetadata> GenerateFullCodeCache(
      DOMArrayBuffer* array_buffer) {
    TRACE_EVENT1("CacheStorage",
                 "Cache::CodeCacheHandleCallbackForPut::GenerateFullCodeCache",
                 "url", CacheStorageTracedValue(url_.GetString()));

    // Currently we only support UTF8 encoding.
    // TODO(horo): Use the charset in Content-type header of the response.
    // See crbug.com/743311.
    std::unique_ptr<TextResourceDecoder> text_decoder =
        std::make_unique<TextResourceDecoder>(
            TextResourceDecoderOptions::CreateUTF8Decode());

    return V8CodeCache::GenerateFullCodeCache(
        script_state_,
        text_decoder->Decode(static_cast<const char*>(array_buffer->Data()),
                             array_buffer->ByteLengthAsSizeT()),
        url_, text_decoder->Encoding(), opaque_mode_);
  }

  const Member<ScriptState> script_state_;
  const wtf_size_t index_;
  Member<BarrierCallbackForPut> barrier_callback_;
  const String mime_type_;
  KURL url_;
  V8CodeCache::OpaqueMode opaque_mode_;
  const int64_t trace_id_;

  mojom::blink::FetchAPIRequestPtr fetch_api_request_;
  mojom::blink::FetchAPIResponsePtr fetch_api_response_;
};

ScriptPromise Cache::match(ScriptState* script_state,
                           const RequestInfo& request,
                           const CacheQueryOptions* options,
                           ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return MatchImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return MatchImpl(script_state, new_request, options);
}

ScriptPromise Cache::matchAll(ScriptState* script_state,
                              ExceptionState& exception_state) {
  return MatchAllImpl(script_state, nullptr, CacheQueryOptions::Create());
}

ScriptPromise Cache::matchAll(ScriptState* script_state,
                              const RequestInfo& request,
                              const CacheQueryOptions* options,
                              ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return MatchAllImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return MatchAllImpl(script_state, new_request, options);
}

ScriptPromise Cache::add(ScriptState* script_state,
                         const RequestInfo& request,
                         ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  HeapVector<Member<Request>> requests;
  if (request.IsRequest()) {
    requests.push_back(request.GetAsRequest());
  } else {
    requests.push_back(Request::Create(script_state, request.GetAsUSVString(),
                                       exception_state));
    if (exception_state.HadException())
      return ScriptPromise();
  }

  return AddAllImpl(script_state, "Cache.add()", requests, exception_state);
}

ScriptPromise Cache::addAll(ScriptState* script_state,
                            const HeapVector<RequestInfo>& raw_requests,
                            ExceptionState& exception_state) {
  HeapVector<Member<Request>> requests;
  for (RequestInfo request : raw_requests) {
    if (request.IsRequest()) {
      requests.push_back(request.GetAsRequest());
    } else {
      requests.push_back(Request::Create(script_state, request.GetAsUSVString(),
                                         exception_state));
      if (exception_state.HadException())
        return ScriptPromise();
    }
  }

  return AddAllImpl(script_state, "Cache.addAll()", requests, exception_state);
}

ScriptPromise Cache::Delete(ScriptState* script_state,
                            const RequestInfo& request,
                            const CacheQueryOptions* options,
                            ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return DeleteImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return DeleteImpl(script_state, new_request, options);
}

ScriptPromise Cache::put(ScriptState* script_state,
                         const RequestInfo& request,
                         Response* response,
                         ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::put",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  if (request.IsRequest()) {
    return PutImpl(script_state, "Cache.put()",
                   HeapVector<Member<Request>>(1, request.GetAsRequest()),
                   HeapVector<Member<Response>>(1, response), exception_state,
                   trace_id);
  }
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return PutImpl(
      script_state, "Cache.put()", HeapVector<Member<Request>>(1, new_request),
      HeapVector<Member<Response>>(1, response), exception_state, trace_id);
}

ScriptPromise Cache::keys(ScriptState* script_state, ExceptionState&) {
  return KeysImpl(script_state, nullptr, CacheQueryOptions::Create());
}

ScriptPromise Cache::keys(ScriptState* script_state,
                          const RequestInfo& request,
                          const CacheQueryOptions* options,
                          ExceptionState& exception_state) {
  DCHECK(!request.IsNull());
  if (request.IsRequest())
    return KeysImpl(script_state, request.GetAsRequest(), options);
  Request* new_request =
      Request::Create(script_state, request.GetAsUSVString(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  return KeysImpl(script_state, new_request, options);
}

Cache::Cache(GlobalFetch::ScopedFetcher* fetcher,
             mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
                 cache_pending_remote,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : scoped_fetcher_(fetcher),
      blob_client_list_(MakeGarbageCollected<CacheStorageBlobClientList>()) {
  cache_remote_.Bind(std::move(cache_pending_remote), std::move(task_runner));
}

void Cache::Trace(Visitor* visitor) const {
  visitor->Trace(scoped_fetcher_);
  visitor->Trace(blob_client_list_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise Cache::MatchImpl(ScriptState* script_state,
                               const Request* request,
                               const CacheQueryOptions* options) {
  mojom::blink::FetchAPIRequestPtr mojo_request =
      request->CreateFetchAPIRequest();
  mojom::blink::CacheQueryOptionsPtr mojo_options =
      mojom::blink::CacheQueryOptions::From(options);

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "Cache::MatchImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(mojo_request),
                         "options", CacheStorageTracedValue(mojo_options));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();
  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  bool in_related_fetch_event = false;
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context))
    in_related_fetch_event = global_scope->HasRelatedFetchEvent(request->url());

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->Match(
      std::move(mojo_request), std::move(mojo_options), in_related_fetch_event,
      trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, int64_t trace_id, Cache* self,
             mojom::blink::MatchResultPtr result) {
            base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Match",
                                     elapsed);
            if (options->hasIgnoreSearch() && options->ignoreSearch()) {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.Cache.Renderer.Match.IgnoreSearch",
                  elapsed);
            }
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "Cache::MatchImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
              switch (result->get_status()) {
                case mojom::CacheStorageError::kErrorNotFound:
                  UMA_HISTOGRAM_LONG_TIMES(
                      "ServiceWorkerCache.Cache.Renderer.Match.Miss", elapsed);
                  resolver->Resolve();
                  break;
                default:
                  RejectCacheStorageWithError(resolver, result->get_status());
                  break;
              }
            } else {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.Cache.Renderer.Match.Hit", elapsed);
              ScriptState::Scope scope(resolver->GetScriptState());
              if (result->is_eager_response()) {
                TRACE_EVENT_WITH_FLOW1(
                    "CacheStorage", "Cache::MatchImpl::Callback",
                    TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                    "eager_response",
                    CacheStorageTracedValue(
                        result->get_eager_response()->response));
                resolver->Resolve(
                    CreateEagerResponse(resolver->GetScriptState(),
                                        std::move(result->get_eager_response()),
                                        self->blob_client_list_));
              } else {
                TRACE_EVENT_WITH_FLOW1(
                    "CacheStorage", "Cache::MatchImpl::Callback",
                    TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                    "response",
                    CacheStorageTracedValue(result->get_response()));
                resolver->Resolve(Response::Create(resolver->GetScriptState(),
                                                   *result->get_response()));
              }
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), trace_id, WrapPersistent(this)));

  return promise;
}

ScriptPromise Cache::MatchAllImpl(ScriptState* script_state,
                                  const Request* request,
                                  const CacheQueryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();

  mojom::blink::CacheQueryOptionsPtr mojo_options =
      mojom::blink::CacheQueryOptions::From(options);
  mojom::blink::FetchAPIRequestPtr fetch_api_request;
  if (request) {
    fetch_api_request = request->CreateFetchAPIRequest();
  }

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "Cache::MatchAllImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(fetch_api_request),
                         "options", CacheStorageTracedValue(mojo_options));

  if (request && request->method() != http_names::kGET &&
      !options->ignoreMethod()) {
    resolver->Resolve(HeapVector<Member<Response>>());
    return promise;
  }

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->MatchAll(
      std::move(fetch_api_request), std::move(mojo_options), trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, int64_t trace_id, Cache* _,
             mojom::blink::MatchAllResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.MatchAll",
                base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "Cache::MatchAllImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
              RejectCacheStorageWithError(resolver, result->get_status());
            } else {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "Cache::MatchAllImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN,
                  "response_list",
                  CacheStorageTracedValue(result->get_responses()));
              ScriptState::Scope scope(resolver->GetScriptState());
              HeapVector<Member<Response>> responses;
              responses.ReserveInitialCapacity(result->get_responses().size());
              for (auto& response : result->get_responses()) {
                responses.push_back(
                    Response::Create(resolver->GetScriptState(), *response));
              }
              resolver->Resolve(responses);
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), trace_id, WrapPersistent(this)));
  return promise;
}

ScriptPromise Cache::AddAllImpl(ScriptState* script_state,
                                const String& method_name,
                                const HeapVector<Member<Request>>& requests,
                                ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::AddAllImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  if (requests.IsEmpty())
    return ScriptPromise::CastUndefined(script_state);

  HeapVector<RequestInfo> request_infos;
  request_infos.resize(requests.size());
  HeapVector<ScriptPromise> promises;
  promises.resize(requests.size());
  for (wtf_size_t i = 0; i < requests.size(); ++i) {
    if (!requests[i]->url().ProtocolIsInHTTPFamily()) {
      exception_state.ThrowTypeError(
          "Add/AddAll does not support schemes "
          "other than \"http\" or \"https\"");
      return ScriptPromise();
    }
    if (requests[i]->method() != http_names::kGET) {
      exception_state.ThrowTypeError(
          "Add/AddAll only supports the GET request method.");
      return ScriptPromise();
    }
    request_infos[i].SetRequest(requests[i]);

    promises[i] = scoped_fetcher_->Fetch(
        script_state, request_infos[i], RequestInit::Create(), exception_state);
  }

  return ScriptPromise::All(script_state, promises)
      .Then(FetchResolvedForAdd::Create(script_state, this, method_name,
                                        requests, exception_state, trace_id));
}

ScriptPromise Cache::DeleteImpl(ScriptState* script_state,
                                const Request* request,
                                const CacheQueryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();

  Vector<mojom::blink::BatchOperationPtr> batch_operations;
  batch_operations.push_back(mojom::blink::BatchOperation::New());
  auto& operation = batch_operations.back();
  operation->operation_type = mojom::blink::OperationType::kDelete;
  operation->request = request->CreateFetchAPIRequest();
  operation->match_options = mojom::blink::CacheQueryOptions::From(options);

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "Cache::DeleteImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(operation->request),
                         "options",
                         CacheStorageTracedValue(operation->match_options));

  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve(false);
    return promise;
  }

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->Batch(
      std::move(batch_operations), trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, int64_t trace_id, Cache* _,
             mojom::blink::CacheStorageVerboseErrorPtr error) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.DeleteOne",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "Cache::DeleteImpl::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(error->value));
            ExecutionContext* context = resolver->GetExecutionContext();
            if (!context || context->IsContextDestroyed())
              return;
            if (error->value != mojom::blink::CacheStorageError::kSuccess) {
              switch (error->value) {
                case mojom::blink::CacheStorageError::kErrorNotFound:
                  resolver->Resolve(false);
                  break;
                default:
                  StringBuilder message;
                  if (error->message) {
                    message.Append("Cache.delete(): ");
                    message.Append(error->message);
                  }
                  RejectCacheStorageWithError(resolver, error->value,
                                              message.ToString());
                  break;
              }
            } else {
              resolver->Resolve(true);
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), trace_id, WrapPersistent(this)));
  return promise;
}

ScriptPromise Cache::PutImpl(ScriptState* script_state,
                             const String& method_name,
                             const HeapVector<Member<Request>>& requests,
                             const HeapVector<Member<Response>>& responses,
                             ExceptionState& exception_state,
                             int64_t trace_id) {
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::PutImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();
  BarrierCallbackForPut* barrier_callback =
      MakeGarbageCollected<BarrierCallbackForPut>(
          requests.size(), this, method_name, resolver, trace_id);

  for (wtf_size_t i = 0; i < requests.size(); ++i) {
    KURL url(NullURL(), requests[i]->url());
    if (!url.ProtocolIsInHTTPFamily()) {
      barrier_callback->OnError("Request scheme '" + url.Protocol() +
                                "' is unsupported");
      return promise;
    }
    if (requests[i]->method() != http_names::kGET) {
      barrier_callback->OnError("Request method '" + requests[i]->method() +
                                "' is unsupported");
      return promise;
    }
    DCHECK(!requests[i]->HasBody());

    if (VaryHeaderContainsAsterisk(responses[i])) {
      barrier_callback->OnError("Vary header contains *");
      return promise;
    }
    if (responses[i]->GetResponse()->InternalStatus() == 206) {
      barrier_callback->OnError(
          "Partial response (status code 206) is unsupported");
      return promise;
    }
    if (responses[i]->IsBodyLocked() || responses[i]->IsBodyUsed()) {
      barrier_callback->OnError("Response body is already used");
      return promise;
    }

    BodyStreamBuffer* buffer = responses[i]->InternalBodyBuffer();

    if (ShouldGenerateV8CodeCache(script_state, responses[i])) {
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsArrayBuffer();
      buffer->StartLoading(loader,
                           MakeGarbageCollected<CodeCacheHandleCallbackForPut>(
                               script_state, i, barrier_callback, requests[i],
                               responses[i], trace_id),
                           exception_state);
      if (exception_state.HadException()) {
        barrier_callback->OnError("Could not inspect response body state");
        return promise;
      }
      continue;
    }

    if (buffer) {
      ExecutionContext* context = ExecutionContext::From(script_state);
      // If the response has body, read the all data and create
      // the blob handle and dispatch the put batch asynchronously.
      FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsBlobHandle(
          responses[i]->InternalMIMEType(),
          context->GetTaskRunner(TaskType::kNetworking));
      buffer->StartLoading(loader,
                           MakeGarbageCollected<BlobHandleCallbackForPut>(
                               i, barrier_callback, requests[i], responses[i]),
                           exception_state);
      if (exception_state.HadException()) {
        barrier_callback->OnError("Could not inspect response body state");
        return promise;
      }
      continue;
    }

    mojom::blink::BatchOperationPtr batch_operation =
        mojom::blink::BatchOperation::New();
    batch_operation->operation_type = mojom::blink::OperationType::kPut;
    batch_operation->request = requests[i]->CreateFetchAPIRequest();
    batch_operation->response =
        responses[i]->PopulateFetchAPIResponse(requests[i]->url());
    barrier_callback->OnSuccess(i, std::move(batch_operation));
  }

  return promise;
}

ScriptPromise Cache::KeysImpl(ScriptState* script_state,
                              const Request* request,
                              const CacheQueryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  const ScriptPromise promise = resolver->Promise();

  mojom::blink::CacheQueryOptionsPtr mojo_options =
      mojom::blink::CacheQueryOptions::From(options);
  mojom::blink::FetchAPIRequestPtr fetch_api_request;
  if (request) {
    fetch_api_request = request->CreateFetchAPIRequest();
  }

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "Cache::DeleteImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(fetch_api_request),
                         "options", CacheStorageTracedValue(mojo_options));

  if (request && request->method() != http_names::kGET &&
      !options->ignoreMethod()) {
    resolver->Resolve(HeapVector<Member<Response>>());
    return promise;
  }

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->Keys(
      std::move(fetch_api_request), std::move(mojo_options), trace_id,
      WTF::Bind(
          [](ScriptPromiseResolver* resolver, base::TimeTicks start_time,
             const CacheQueryOptions* options, int64_t trace_id, Cache* _,
             mojom::blink::CacheKeysResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Keys",
                                     base::TimeTicks::Now() - start_time);
            if (!resolver->GetExecutionContext() ||
                resolver->GetExecutionContext()->IsContextDestroyed())
              return;
            if (result->is_status()) {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "Cache::KeysImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_status()));
              RejectCacheStorageWithError(resolver, result->get_status());
            } else {
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage", "Cache::KeysImpl::Callback",
                  TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                  CacheStorageTracedValue(result->get_keys()));
              ScriptState::Scope scope(resolver->GetScriptState());
              HeapVector<Member<Request>> requests;
              requests.ReserveInitialCapacity(result->get_keys().size());
              for (auto& request : result->get_keys()) {
                requests.push_back(Request::Create(
                    resolver->GetScriptState(), std::move(request),
                    Request::ForServiceWorkerFetchEvent::kFalse));
              }
              resolver->Resolve(requests);
            }
          },
          WrapPersistent(resolver), base::TimeTicks::Now(),
          WrapPersistent(options), trace_id, WrapPersistent(this)));
  return promise;
}

}  // namespace blink
