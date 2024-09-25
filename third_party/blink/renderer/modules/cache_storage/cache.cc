// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
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
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_request_usvstring.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_error.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_trace_utils.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_utils.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
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
    String (String::*strip_whitespace)() const = &String::StripWhiteSpace;
    return base::Contains(fields, "*", strip_whitespace);
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

void ValidateRequestForPut(const Request* request,
                           ExceptionState& exception_state) {
  const KURL& url = request->url();
  if (!url.ProtocolIsInHTTPFamily()) {
    exception_state.ThrowTypeError("Request scheme '" + url.Protocol() +
                                   "' is unsupported");
    return;
  }
  if (request->method() != http_names::kGET) {
    exception_state.ThrowTypeError("Request method '" + request->method() +
                                   "' is unsupported");
    return;
  }
  DCHECK(!request->HasBody());
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

  if (EqualIgnoringASCIICase(header_value, "none")) {
    return CodeCachePolicy::kNone;
  }

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

// Waits for all expected Responses and their blob bodies to be available.
class Cache::BarrierCallbackForPutResponse final
    : public GarbageCollected<BarrierCallbackForPutResponse> {
 public:
  BarrierCallbackForPutResponse(ScriptState* script_state,
                                Cache* cache,
                                const String& method_name,
                                const HeapVector<Member<Request>>& request_list,
                                const ExceptionContext& exception_context,
                                int64_t trace_id)
      : resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
            script_state,
            exception_context)),
        cache_(cache),
        method_name_(method_name),
        request_list_(request_list),
        exception_context_(exception_context),
        trace_id_(trace_id),
        response_list_(request_list_.size()),
        blob_list_(request_list_.size()) {
    if (request_list.size() > 1) {
      abort_controller_ = cache_->CreateAbortController(script_state);
    }
  }

  // Must be called prior to starting the load of any response.
  ScriptPromise<IDLUndefined> Promise() const { return resolver_->Promise(); }

  AbortSignal* Signal() const {
    return abort_controller_ ? abort_controller_->signal() : nullptr;
  }

  void CompletedResponse(int index,
                         Response* response,
                         scoped_refptr<BlobDataHandle> blob) {
    if (stopped_)
      return;

    DCHECK(!response_list_[index]);
    DCHECK(!blob_list_[index]);
    DCHECK_LT(num_complete_, request_list_.size());

    response_list_[index] = response;
    blob_list_[index] = std::move(blob);
    num_complete_ += 1;

    if (num_complete_ == request_list_.size()) {
      ScriptState* script_state = resolver_->GetScriptState();
      ExceptionState exception_state(script_state->GetIsolate(),
                                     exception_context_);
      cache_->PutImpl(resolver_, method_name_, request_list_, response_list_,
                      blob_list_, exception_state, trace_id_);
      blob_list_.clear();
      stopped_ = true;
    }
  }

  void FailedResponse() {
    if (resolver_->GetScriptState()->ContextIsValid()) {
      resolver_->RejectWithDOMException(
          DOMExceptionCode::kNetworkError,
          method_name_ + " encountered a network error");
    }
    Stop();
  }

  void AbortedResponse() {
    if (resolver_->GetScriptState()->ContextIsValid()) {
      resolver_->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                        method_name_ + " was aborted");
    }
    Stop();
  }

  void OnError(ScriptValue value) {
    resolver_->Reject(value);
    Stop();
  }

  void OnError(v8::Local<v8::Value> value) {
    resolver_->Reject(value);
    Stop();
  }

  void OnError(const String& message) {
    resolver_->RejectWithTypeError(message);
    Stop();
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(abort_controller_);
    visitor->Trace(cache_);
    visitor->Trace(request_list_);
    visitor->Trace(response_list_);
  }

 private:
  void Stop() {
    if (stopped_)
      return;
    if (abort_controller_) {
      ScriptState::Scope scope(resolver_->GetScriptState());
      abort_controller_->abort(resolver_->GetScriptState());
    }
    blob_list_.clear();
    stopped_ = true;
  }

  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Member<AbortController> abort_controller_;
  Member<Cache> cache_;
  const String method_name_;
  const HeapVector<Member<Request>> request_list_;
  const ExceptionContext exception_context_;
  const int64_t trace_id_;
  HeapVector<Member<Response>> response_list_;
  WTF::Vector<scoped_refptr<BlobDataHandle>> blob_list_;
  size_t num_complete_ = 0;
  bool stopped_ = false;
};

// Waits for a single Response and then loads its body as a blob.  This class
// also performs validation on the Response and triggers a failure if
// necessary.  Passing true for |require_response_ok| will also trigger a
// failure if the Response status code is not ok.  This is necessary for the
// add/addAll case, but is not used in the put case.
class Cache::ResponseBodyLoader final
    : public GarbageCollected<Cache::ResponseBodyLoader>,
      public FetchDataLoader::Client {
 public:
  ResponseBodyLoader(ScriptState* script_state,
                     BarrierCallbackForPutResponse* barrier_callback,
                     int index,
                     bool require_ok_response,
                     int64_t trace_id)
      : script_state_(script_state),
        barrier_callback_(barrier_callback),
        index_(index),
        require_ok_response_(require_ok_response),
        trace_id_(trace_id) {}

  void OnResponse(Response* response, ExceptionState& exception_state) {
    TRACE_EVENT_WITH_FLOW0(
        "CacheStorage", "Cache::ResponseBodyLoader::OnResponse",
        TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

    if (require_ok_response_ && !response->ok()) {
      barrier_callback_->OnError("Request failed");
      return;
    }

    if (VaryHeaderContainsAsterisk(response)) {
      barrier_callback_->OnError("Vary header contains *");
      return;
    }
    if (response->GetResponse()->Status() == 206) {
      barrier_callback_->OnError(
          "Partial response (status code 206) is unsupported");
      return;
    }
    if (response->IsBodyLocked() || response->IsBodyUsed()) {
      barrier_callback_->OnError("Response body is already used");
      return;
    }

    BodyStreamBuffer* buffer = response->InternalBodyBuffer();
    if (!buffer) {
      barrier_callback_->CompletedResponse(index_, response, nullptr);
      return;
    }

    response_ = response;

    ExecutionContext* context = ExecutionContext::From(script_state_);
    fetch_loader_ = FetchDataLoader::CreateLoaderAsBlobHandle(
        response_->InternalMIMEType(),
        context->GetTaskRunner(TaskType::kNetworking));
    buffer->StartLoading(fetch_loader_, this, exception_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(barrier_callback_);
    visitor->Trace(response_);
    visitor->Trace(fetch_loader_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  void DidFetchDataLoadedBlobHandle(
      scoped_refptr<BlobDataHandle> handle) override {
    barrier_callback_->CompletedResponse(index_, response_, std::move(handle));
  }

  void DidFetchDataLoadFailed() override {
    barrier_callback_->FailedResponse();
  }

  void Abort() override { barrier_callback_->AbortedResponse(); }

  Member<ScriptState> script_state_;
  Member<BarrierCallbackForPutResponse> barrier_callback_;
  const int index_;
  const bool require_ok_response_;
  const int64_t trace_id_;
  Member<Response> response_;
  Member<FetchDataLoader> fetch_loader_;
};

// Waits for code cache to be generated and writing to cache_storage to
// complete.
class Cache::BarrierCallbackForPutComplete final
    : public GarbageCollected<BarrierCallbackForPutComplete> {
 public:
  BarrierCallbackForPutComplete(wtf_size_t number_of_operations,
                                Cache* cache,
                                const String& method_name,
                                ScriptPromiseResolver<IDLUndefined>* resolver,
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
        "CacheStorage", "Cache::BarrierCallbackForPutComplete::OnSuccess",
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
        resolver_->WrapCallbackInScriptScope(WTF::BindOnce(
            [](const String& method_name, base::TimeTicks start_time,
               int operation_count, int64_t trace_id, Cache* _,
               ScriptPromiseResolver<IDLUndefined>* resolver,
               mojom::blink::CacheStorageVerboseErrorPtr error) {
              base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
              TRACE_EVENT_WITH_FLOW1(
                  "CacheStorage",
                  "Cache::BarrierCallbackForPutComplete::OnSuccess::Callback",
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
            method_name_, base::TimeTicks::Now(), operation_count, trace_id_,
            WrapPersistent(cache_.Get()))));
  }

  void OnError(v8::Local<v8::Value> exception) {
    if (!StillActive())
      return;
    completed_ = true;
    resolver_->Reject(exception);
  }

  void OnError(const String& error_message) {
    if (!StillActive())
      return;
    completed_ = true;
    resolver_->RejectWithTypeError(error_message);
  }

  void Abort() {
    if (!StillActive())
      return;
    completed_ = true;
    resolver_->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                      method_name_ + " was aborted");
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
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  Vector<mojom::blink::BatchOperationPtr> batch_operations_;
  const int64_t trace_id_;
};

// Used to handle the GlobalFetch::ScopedFetcher::Fetch promise in AddAllImpl.
// TODO(nhiroki): Unfortunately, we have to go through V8 to wait for the fetch
// promise. It should be better to achieve this only within C++ world.
class Cache::FetchHandler final : public ScriptFunction::Callable {
 public:
  // |exception_state| is passed so that the context_type, interface_name and
  // property_name can be copied and then used to construct a new ExceptionState
  // object asynchronously later.
  FetchHandler(ResponseBodyLoader* response_loader,
               BarrierCallbackForPutResponse* barrier_callback,
               const ExceptionContext& exception_context)
      : response_loader_(response_loader),
        barrier_callback_(barrier_callback),
        exception_context_(exception_context) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    // We always resolve undefined from this promise handler since the
    // promise is never returned to script or chained to another handler.
    // If we return our real result and an exception occurs then unhandled
    // promise errors will occur.
    ScriptValue rtn(script_state->GetIsolate(),
                    ToResolvedUndefinedPromise(script_state).V8Promise());

    // If there is no loader, we were created as a reject handler.
    if (!response_loader_) {
      barrier_callback_->OnError(value);
      return rtn;
    }

    v8::TryCatch try_catch(script_state->GetIsolate());

    // Resolve handler, so try to process a Response.
    Response* response = NativeValueTraits<Response>::NativeValue(
        script_state->GetIsolate(), value.V8Value(),
        PassThroughException(script_state->GetIsolate()));
    if (try_catch.HasCaught()) {
      barrier_callback_->OnError(try_catch.Exception());
    } else {
      response_loader_->OnResponse(response, ASSERT_NO_EXCEPTION);
    }

    return rtn;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(response_loader_);
    visitor->Trace(barrier_callback_);
    ScriptFunction::Callable::Trace(visitor);
  }

 private:
  Member<ResponseBodyLoader> response_loader_;
  Member<BarrierCallbackForPutResponse> barrier_callback_;
  const ExceptionContext exception_context_;
};

class Cache::CodeCacheHandleCallbackForPut final
    : public GarbageCollected<CodeCacheHandleCallbackForPut>,
      public FetchDataLoader::Client {
 public:
  CodeCacheHandleCallbackForPut(ScriptState* script_state,
                                wtf_size_t index,
                                BarrierCallbackForPutComplete* barrier_callback,
                                Request* request,
                                Response* response,
                                scoped_refptr<BlobDataHandle> blob_handle,
                                int64_t trace_id)
      : script_state_(script_state),
        index_(index),
        barrier_callback_(barrier_callback),
        mime_type_(response->InternalMIMEType()),
        blob_handle_(std::move(blob_handle)),
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
    batch_operation->response->blob = std::move(blob_handle_);

    scoped_refptr<CachedMetadata> cached_metadata =
        GenerateFullCodeCache(array_buffer);
    if (cached_metadata) {
      base::span<const uint8_t> serialized_data =
          cached_metadata->SerializedData();
      auto side_data_blob_data = std::make_unique<BlobData>();
      side_data_blob_data->AppendBytes(serialized_data);

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
        script_state_, text_decoder->Decode(array_buffer->ByteSpan()), url_,
        text_decoder->Encoding(), opaque_mode_);
  }

  const Member<ScriptState> script_state_;
  const wtf_size_t index_;
  Member<BarrierCallbackForPutComplete> barrier_callback_;
  const String mime_type_;
  scoped_refptr<BlobDataHandle> blob_handle_;
  KURL url_;
  V8CodeCache::OpaqueMode opaque_mode_;
  const int64_t trace_id_;

  mojom::blink::FetchAPIRequestPtr fetch_api_request_;
  mojom::blink::FetchAPIResponsePtr fetch_api_response_;
};

ScriptPromise<Response> Cache::match(ScriptState* script_state,
                                     const V8RequestInfo* request,
                                     const CacheQueryOptions* options,
                                     ExceptionState& exception_state) {
  DCHECK(request);
  Request* request_object = nullptr;
  switch (request->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      request_object = request->GetAsRequest();
      break;
    case V8RequestInfo::ContentType::kUSVString:
      request_object = Request::Create(script_state, request->GetAsUSVString(),
                                       exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      break;
  }
  return MatchImpl(script_state, request_object, options, exception_state);
}

ScriptPromise<IDLSequence<Response>> Cache::matchAll(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return MatchAllImpl(script_state, nullptr, CacheQueryOptions::Create(),
                      exception_state);
}

ScriptPromise<IDLSequence<Response>> Cache::matchAll(
    ScriptState* script_state,
    const V8RequestInfo* request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  Request* request_object = nullptr;
  if (request) {
    switch (request->GetContentType()) {
      case V8RequestInfo::ContentType::kRequest:
        request_object = request->GetAsRequest();
        break;
      case V8RequestInfo::ContentType::kUSVString:
        request_object = Request::Create(
            script_state, request->GetAsUSVString(), exception_state);
        if (exception_state.HadException())
          return ScriptPromise<IDLSequence<Response>>();
        break;
    }
  }
  return MatchAllImpl(script_state, request_object, options, exception_state);
}

ScriptPromise<IDLUndefined> Cache::add(ScriptState* script_state,
                                       const V8RequestInfo* request,
                                       ExceptionState& exception_state) {
  DCHECK(request);
  HeapVector<Member<Request>> requests;
  switch (request->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      requests.push_back(request->GetAsRequest());
      break;
    case V8RequestInfo::ContentType::kUSVString:
      requests.push_back(Request::Create(
          script_state, request->GetAsUSVString(), exception_state));
      if (exception_state.HadException())
        return EmptyPromise();
      break;
  }
  return AddAllImpl(script_state, "Cache.add()", requests, exception_state);
}

ScriptPromise<IDLUndefined> Cache::addAll(
    ScriptState* script_state,
    const HeapVector<Member<V8RequestInfo>>& requests,
    ExceptionState& exception_state) {
  HeapVector<Member<Request>> request_objects;
  for (const V8RequestInfo* request : requests) {
    switch (request->GetContentType()) {
      case V8RequestInfo::ContentType::kRequest:
        request_objects.push_back(request->GetAsRequest());
        break;
      case V8RequestInfo::ContentType::kUSVString:
        request_objects.push_back(Request::Create(
            script_state, request->GetAsUSVString(), exception_state));
        if (exception_state.HadException())
          return EmptyPromise();
        break;
    }
  }
  return AddAllImpl(script_state, "Cache.addAll()", request_objects,
                    exception_state);
}

ScriptPromise<IDLBoolean> Cache::Delete(ScriptState* script_state,
                                        const V8RequestInfo* request,
                                        const CacheQueryOptions* options,
                                        ExceptionState& exception_state) {
  DCHECK(request);
  Request* request_object = nullptr;
  switch (request->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      request_object = request->GetAsRequest();
      break;
    case V8RequestInfo::ContentType::kUSVString:
      request_object = Request::Create(script_state, request->GetAsUSVString(),
                                       exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      break;
  }
  return DeleteImpl(script_state, request_object, options, exception_state);
}

ScriptPromise<IDLUndefined> Cache::put(ScriptState* script_state,
                                       const V8RequestInfo* request_info,
                                       Response* response,
                                       ExceptionState& exception_state) {
  DCHECK(request_info);
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::put",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);
  Request* request = nullptr;
  switch (request_info->GetContentType()) {
    case V8RequestInfo::ContentType::kRequest:
      request = request_info->GetAsRequest();
      break;
    case V8RequestInfo::ContentType::kUSVString:
      request = Request::Create(script_state, request_info->GetAsUSVString(),
                                exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      break;
  }

  ValidateRequestForPut(request, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* barrier_callback = MakeGarbageCollected<BarrierCallbackForPutResponse>(
      script_state, this, "Cache.put()",
      HeapVector<Member<Request>>(1, request), exception_state.GetContext(),
      trace_id);

  // We must get the promise before any rejections can happen during loading.
  auto promise = barrier_callback->Promise();

  auto* loader = MakeGarbageCollected<ResponseBodyLoader>(
      script_state, barrier_callback, /*index=*/0,
      /*require_ok_response=*/false, trace_id);
  loader->OnResponse(response, exception_state);

  return promise;
}

ScriptPromise<IDLSequence<Request>> Cache::keys(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return KeysImpl(script_state, nullptr, CacheQueryOptions::Create(),
                  exception_state);
}

ScriptPromise<IDLSequence<Request>> Cache::keys(
    ScriptState* script_state,
    const V8RequestInfo* request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  Request* request_object = nullptr;
  if (request) {
    switch (request->GetContentType()) {
      case V8RequestInfo::ContentType::kRequest:
        request_object = request->GetAsRequest();
        break;
      case V8RequestInfo::ContentType::kUSVString:
        request_object = Request::Create(
            script_state, request->GetAsUSVString(), exception_state);
        if (exception_state.HadException())
          return ScriptPromise<IDLSequence<Request>>();
        break;
    }
  }
  return KeysImpl(script_state, request_object, options, exception_state);
}

Cache::Cache(GlobalFetch::ScopedFetcher* fetcher,
             CacheStorageBlobClientList* blob_client_list,
             mojo::PendingAssociatedRemote<mojom::blink::CacheStorageCache>
                 cache_pending_remote,
             ExecutionContext* execution_context,
             TaskType task_type)
    : scoped_fetcher_(fetcher),
      blob_client_list_(blob_client_list),
      cache_remote_(execution_context) {
  cache_remote_.Bind(std::move(cache_pending_remote),
                     execution_context->GetTaskRunner(task_type));
}

void Cache::Trace(Visitor* visitor) const {
  visitor->Trace(scoped_fetcher_);
  visitor->Trace(blob_client_list_);
  visitor->Trace(cache_remote_);
  ScriptWrappable::Trace(visitor);
}

AbortController* Cache::CreateAbortController(ScriptState* script_state) {
  return AbortController::Create(script_state);
}

ScriptPromise<Response> Cache::MatchImpl(ScriptState* script_state,
                                         const Request* request,
                                         const CacheQueryOptions* options,
                                         ExceptionState& exception_state) {
  mojom::blink::FetchAPIRequestPtr mojo_request =
      request->CreateFetchAPIRequest();
  mojom::blink::CacheQueryOptionsPtr mojo_options =
      mojom::blink::CacheQueryOptions::From(options);

  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW2("CacheStorage", "Cache::MatchImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT,
                         "request", CacheStorageTracedValue(mojo_request),
                         "options", CacheStorageTracedValue(mojo_options));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Response>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  if (request->method() != http_names::kGET && !options->ignoreMethod()) {
    resolver->Resolve();
    return promise;
  }

  bool in_related_fetch_event = false;
  bool in_range_fetch_event = false;
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (auto* global_scope = DynamicTo<ServiceWorkerGlobalScope>(context)) {
    in_related_fetch_event = global_scope->HasRelatedFetchEvent(request->url());
    in_range_fetch_event = global_scope->HasRangeFetchEvent(request->url());
  }

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->Match(
      std::move(mojo_request), std::move(mojo_options), in_related_fetch_event,
      in_range_fetch_event, trace_id,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::TimeTicks start_time, const CacheQueryOptions* options,
             int64_t trace_id, Cache* self,
             ScriptPromiseResolver<Response>* resolver,
             mojom::blink::MatchResultPtr result) {
            base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Match",
                                     elapsed);
            if (options->hasIgnoreSearch() && options->ignoreSearch()) {
              UMA_HISTOGRAM_LONG_TIMES(
                  "ServiceWorkerCache.Cache.Renderer.Match.IgnoreSearch",
                  elapsed);
            }
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
          base::TimeTicks::Now(), WrapPersistent(options), trace_id,
          WrapPersistent(this))));

  return promise;
}

ScriptPromise<IDLSequence<Response>> Cache::MatchAllImpl(
    ScriptState* script_state,
    const Request* request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<Response>>>(
          script_state, exception_state.GetContext());
  const auto promise = resolver->Promise();

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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::TimeTicks start_time, const CacheQueryOptions* options,
             int64_t trace_id, Cache* _,
             ScriptPromiseResolver<IDLSequence<Response>>* resolver,
             mojom::blink::MatchAllResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.MatchAll",
                base::TimeTicks::Now() - start_time);
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
          base::TimeTicks::Now(), WrapPersistent(options), trace_id,
          WrapPersistent(this))));
  return promise;
}

ScriptPromise<IDLUndefined> Cache::AddAllImpl(
    ScriptState* script_state,
    const String& method_name,
    const HeapVector<Member<Request>>& request_list,
    ExceptionState& exception_state) {
  int64_t trace_id = blink::cache_storage::CreateTraceId();
  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::AddAllImpl",
                         TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_OUT);

  if (request_list.empty())
    return ToResolvedUndefinedPromise(script_state);

  // Validate all requests before starting to load or store any of them.
  for (wtf_size_t i = 0; i < request_list.size(); ++i) {
    ValidateRequestForPut(request_list[i], exception_state);
    if (exception_state.HadException())
      return EmptyPromise();
  }

  auto* barrier_callback = MakeGarbageCollected<BarrierCallbackForPutResponse>(
      script_state, this, method_name, request_list,
      exception_state.GetContext(), trace_id);

  // We must get the promise before any rejections can happen during loading.
  auto promise = barrier_callback->Promise();

  // Begin loading each of the requests.
  for (wtf_size_t i = 0; i < request_list.size(); ++i) {
    auto* init = RequestInit::Create();
    if (barrier_callback->Signal()) {
      HeapVector<Member<AbortSignal>> signals;
      signals.push_back(barrier_callback->Signal());
      signals.push_back(request_list[i]->signal());
      init->setSignal(MakeGarbageCollected<AbortSignal>(script_state, signals));
    }

    V8RequestInfo* info = MakeGarbageCollected<V8RequestInfo>(request_list[i]);

    auto* response_loader = MakeGarbageCollected<ResponseBodyLoader>(
        script_state, barrier_callback, i, /*require_ok_response=*/true,
        trace_id);
    auto* on_resolve = MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<FetchHandler>(response_loader, barrier_callback,
                                           exception_state.GetContext()));
    // The |response_loader=nullptr| makes this handler a reject handler
    // internally.
    auto* on_reject = MakeGarbageCollected<ScriptFunction>(
        script_state, MakeGarbageCollected<FetchHandler>(
                          /*response_loader=*/nullptr, barrier_callback,
                          exception_state.GetContext()));
    scoped_fetcher_->Fetch(script_state, info, init, exception_state)
        .Then(on_resolve, on_reject);
  }

  return promise;
}

ScriptPromise<IDLBoolean> Cache::DeleteImpl(ScriptState* script_state,
                                            const Request* request,
                                            const CacheQueryOptions* options,
                                            ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  const auto promise = resolver->Promise();

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
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::TimeTicks start_time, const CacheQueryOptions* options,
             int64_t trace_id, Cache* _,
             ScriptPromiseResolver<IDLBoolean>* resolver,
             mojom::blink::CacheStorageVerboseErrorPtr error) {
            UMA_HISTOGRAM_LONG_TIMES(
                "ServiceWorkerCache.Cache.Renderer.DeleteOne",
                base::TimeTicks::Now() - start_time);
            TRACE_EVENT_WITH_FLOW1(
                "CacheStorage", "Cache::DeleteImpl::Callback",
                TRACE_ID_GLOBAL(trace_id), TRACE_EVENT_FLAG_FLOW_IN, "status",
                CacheStorageTracedValue(error->value));
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
          base::TimeTicks::Now(), WrapPersistent(options), trace_id,
          WrapPersistent(this))));
  return promise;
}

void Cache::PutImpl(ScriptPromiseResolver<IDLUndefined>* resolver,
                    const String& method_name,
                    const HeapVector<Member<Request>>& requests,
                    const HeapVector<Member<Response>>& responses,
                    const WTF::Vector<scoped_refptr<BlobDataHandle>>& blob_list,
                    ExceptionState& exception_state,
                    int64_t trace_id) {
  DCHECK_EQ(requests.size(), responses.size());
  DCHECK_EQ(requests.size(), blob_list.size());

  TRACE_EVENT_WITH_FLOW0("CacheStorage", "Cache::PutImpl",
                         TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);
  ExecutionContext* context = ExecutionContext::From(script_state);

  BarrierCallbackForPutComplete* barrier_callback =
      MakeGarbageCollected<BarrierCallbackForPutComplete>(
          requests.size(), this, method_name, resolver, trace_id);

  for (wtf_size_t i = 0; i < requests.size(); ++i) {
    if (!blob_list[i] ||
        !ShouldGenerateV8CodeCache(script_state, responses[i])) {
      mojom::blink::BatchOperationPtr batch_operation =
          mojom::blink::BatchOperation::New();
      batch_operation->operation_type = mojom::blink::OperationType::kPut;
      batch_operation->request = requests[i]->CreateFetchAPIRequest();
      batch_operation->response =
          responses[i]->PopulateFetchAPIResponse(requests[i]->url());
      batch_operation->response->blob = std::move(blob_list[i]);
      barrier_callback->OnSuccess(i, std::move(batch_operation));
      continue;
    }

    BytesConsumer* consumer =
        MakeGarbageCollected<BlobBytesConsumer>(context, blob_list[i]);
    BodyStreamBuffer* buffer =
        BodyStreamBuffer::Create(script_state, consumer, /*signal=*/nullptr,
                                 /*cached_metadata_handler=*/nullptr);
    FetchDataLoader* loader = FetchDataLoader::CreateLoaderAsArrayBuffer();
    buffer->StartLoading(loader,
                         MakeGarbageCollected<CodeCacheHandleCallbackForPut>(
                             script_state, i, barrier_callback, requests[i],
                             responses[i], std::move(blob_list[i]), trace_id),
                         exception_state);
    if (exception_state.HadException()) {
      barrier_callback->OnError("Could not inspect response body state");
      return;
    }
  }
}

ScriptPromise<IDLSequence<Request>> Cache::KeysImpl(
    ScriptState* script_state,
    const Request* request,
    const CacheQueryOptions* options,
    ExceptionState& exception_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<Request>>>(
          script_state, exception_state.GetContext());
  const auto promise = resolver->Promise();

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
    resolver->Resolve(HeapVector<Member<Request>>());
    return promise;
  }

  // Make sure to bind the Cache object to keep the mojo remote alive during
  // the operation. Otherwise GC might prevent the callback from ever being
  // executed.
  cache_remote_->Keys(
      std::move(fetch_api_request), std::move(mojo_options), trace_id,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          [](base::TimeTicks start_time, const CacheQueryOptions* options,
             int64_t trace_id, Cache* _,
             ScriptPromiseResolver<IDLSequence<Request>>* resolver,
             mojom::blink::CacheKeysResultPtr result) {
            UMA_HISTOGRAM_LONG_TIMES("ServiceWorkerCache.Cache.Renderer.Keys",
                                     base::TimeTicks::Now() - start_time);
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
          base::TimeTicks::Now(), WrapPersistent(options), trace_id,
          WrapPersistent(this))));
  return promise;
}

}  // namespace blink
