// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"

#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// The first `kWireBytesDigestSize` bytes of CachedMetadata body stores the
// SHA-256 hash of the wire bytes and created/consumed here in Blink. The
// remaining part of CachedMetadata is created/consumed by V8.
static const size_t kWireBytesDigestSize = 32;

// Wasm only has a single metadata type, but we need to tag it.
// `2` is used to invalidate old cached data (which used kWasmModuleTag = 1).
static const int kWasmModuleTag = 2;

void SendCachedData(String response_url,
                    base::Time response_time,
                    String cache_storage_cache_name,
                    ExecutionContext* execution_context,
                    Vector<uint8_t> serialized_module) {
  if (!execution_context)
    return;
  scoped_refptr<CachedMetadata> cached_metadata =
      CachedMetadata::CreateFromSerializedData(std::move(serialized_module));

  CodeCacheHost* code_cache_host =
      ExecutionContext::GetCodeCacheHostFromContext(execution_context);
  base::span<const uint8_t> serialized_data = cached_metadata->SerializedData();
  CachedMetadataSender::SendToCodeCacheHost(
      code_cache_host, mojom::blink::CodeCacheType::kWebAssembly, response_url,
      response_time, cache_storage_cache_name, serialized_data.data(),
      serialized_data.size());
}

class WasmCodeCachingCallback {
 public:
  WasmCodeCachingCallback(
      const String& response_url,
      const base::Time& response_time,
      const String& cache_storage_cache_name,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ExecutionContext* execution_context)
      : response_url_(response_url),
        response_time_(response_time),
        cache_storage_cache_name_(cache_storage_cache_name),
        execution_context_task_runner_(std::move(task_runner)),
        execution_context_(execution_context) {}

  WasmCodeCachingCallback(const WasmCodeCachingCallback&) = delete;
  WasmCodeCachingCallback operator=(const WasmCodeCachingCallback&) = delete;

  void OnMoreFunctionsCanBeSerialized(v8::CompiledWasmModule compiled_module) {
    // Called from V8 background thread.
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.compiledModule", TRACE_EVENT_SCOPE_THREAD,
                         "url", response_url_.Utf8());
    v8::OwnedBuffer serialized_module;
    {
      // Use a standard milliseconds based timer (up to 10 seconds, 50 buckets),
      // similar to "V8.WasmDeserializationTimeMilliSeconds" defined in V8.
      SCOPED_UMA_HISTOGRAM_TIMER("V8.WasmSerializationTimeMilliSeconds");
      serialized_module = compiled_module.Serialize();
    }
    // V8 might not be able to serialize the module.
    if (serialized_module.size == 0)
      return;

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.cachedModule", TRACE_EVENT_SCOPE_THREAD,
                         "producedCacheSize", serialized_module.size);

    v8::MemorySpan<const uint8_t> wire_bytes =
        compiled_module.GetWireBytesRef();
    DigestValue wire_bytes_digest;
    {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                   "v8.wasm.compileDigestForCreate");
      if (!ComputeDigest(kHashAlgorithmSha256, wire_bytes, wire_bytes_digest)) {
        return;
      }
      if (wire_bytes_digest.size() != kWireBytesDigestSize)
        return;
    }

    // The resources needed for caching may have been GC'ed, but we should still
    // save the compiled module. Use the platform API directly.
    Vector<uint8_t> serialized_data = CachedMetadata::GetSerializedDataHeader(
        kWasmModuleTag, kWireBytesDigestSize + base::checked_cast<wtf_size_t>(
                                                   serialized_module.size));
    serialized_data.AppendSpan(base::span(wire_bytes_digest));
    serialized_data.Append(
        reinterpret_cast<const uint8_t*>(serialized_module.buffer.get()),
        base::checked_cast<wtf_size_t>(serialized_module.size));

    // Make sure the data could be copied.
    if (serialized_data.size() < serialized_module.size)
      return;

    DCHECK(execution_context_task_runner_.get());
    execution_context_task_runner_->PostTask(
        FROM_HERE, ConvertToBaseOnceCallback(WTF::CrossThreadBindOnce(
                       &SendCachedData, response_url_, response_time_,
                       cache_storage_cache_name_, execution_context_,
                       std::move(serialized_data))));
  }

  void SetBuffer(scoped_refptr<CachedMetadata> cached_module) {
    cached_module_ = cached_module;
  }

 private:
  const String response_url_;
  const base::Time response_time_;
  const String cache_storage_cache_name_;
  scoped_refptr<CachedMetadata> cached_module_;
  scoped_refptr<base::SingleThreadTaskRunner> execution_context_task_runner_;
  CrossThreadWeakPersistent<ExecutionContext> execution_context_;
};

// The |FetchDataLoader| for streaming compilation of WebAssembly code. The
// received bytes get forwarded to the V8 API class |WasmStreaming|.
class FetchDataLoaderForWasmStreaming final : public FetchDataLoader,
                                              public BytesConsumer::Client {
 public:
  FetchDataLoaderForWasmStreaming(
      const String& url,
      std::shared_ptr<v8::WasmStreaming> streaming,
      ScriptState* script_state,
      ScriptCachedMetadataHandler* cache_handler,
      std::shared_ptr<WasmCodeCachingCallback> code_caching_callback)
      : url_(url),
        streaming_(std::move(streaming)),
        script_state_(script_state),
        cache_handler_(cache_handler),
        code_caching_callback_(std::move(code_caching_callback)) {}

  v8::WasmStreaming* streaming() const { return streaming_.get(); }

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!consumer_);
    DCHECK(!client_);
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  enum class CodeCacheState {
    kBeforeFirstByte,
    kUseCodeCache,
    kNoCodeCache,
  };

  void OnStateChange() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                 "v8.wasm.compileConsume");
    // Continue reading until we either finished, aborted, or no data is
    // available any more (handled below).
    while (streaming_) {
      // |buffer| is owned by |consumer_|.
      const char* buffer = nullptr;
      size_t available = 0;
      BytesConsumer::Result result = consumer_->BeginRead(&buffer, &available);

      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        // Ignore more bytes after an abort (streaming == nullptr).
        if (available > 0) {
          if (code_cache_state_ == CodeCacheState::kBeforeFirstByte)
            code_cache_state_ = MaybeConsumeCodeCache();

          DCHECK_NE(buffer, nullptr);
          if (code_cache_state_ == CodeCacheState::kUseCodeCache) {
            TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.compileDigestForConsume");
            digestor_.Update(
                base::as_bytes(base::make_span(buffer, available)));
          }
          streaming_->OnBytesReceived(reinterpret_cast<const uint8_t*>(buffer),
                                      available);
        }
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED_IN_MIGRATION();
          return;
        case BytesConsumer::Result::kOk:
          break;
        case BytesConsumer::Result::kDone: {
          TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "v8.wasm.compileConsumeDone");
          {
            ScriptState::Scope scope(script_state_);
            streaming_->Finish(HasValidCodeCache());
          }
          client_->DidFetchDataLoadedCustomFormat();
          streaming_.reset();
          return;
        }
        case BytesConsumer::Result::kError:
          DCHECK_EQ(BytesConsumer::PublicState::kErrored,
                    consumer_->GetPublicState());
          AbortCompilation("Network error: " + consumer_->GetError().Message());
          break;
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderForWasmModule"; }

  void Cancel() override {
    consumer_->Cancel();
    return AbortCompilation("Cancellation requested");
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(script_state_);
    visitor->Trace(cache_handler_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

  void AbortFromClient() {
    // Ignore a repeated abort request, or abort after successfully finishing.
    if (!streaming_) {
      return;
    }
    auto* exception =
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError);
    ScriptState::Scope scope(script_state_);

    // Calling ToV8 in a ScriptForbiddenScope will trigger a CHECK and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      v8::Local<v8::Value> v8_exception =
          ToV8Traits<DOMException>::ToV8(script_state_, exception);
      streaming_->Abort(v8_exception);
      streaming_.reset();
    }
  }

 private:
  // TODO(ahaas): replace with spec-ed error types, once spec clarifies
  // what they are.
  void AbortCompilation(String reason) {
    // Ignore a repeated abort request, or abort after successfully finishing.
    if (!streaming_) {
      return;
    }
    if (script_state_->ContextIsValid()) {
      ScriptState::Scope scope(script_state_);
      streaming_->Abort(V8ThrowException::CreateTypeError(
          script_state_->GetIsolate(),
          "WebAssembly compilation aborted: " + reason));
    } else {
      // We are not allowed to execute a script, which indicates that we should
      // not reject the promise of the streaming compilation. By passing no
      // abort reason, we indicate the V8 side that the promise should not get
      // rejected.
      streaming_->Abort(v8::Local<v8::Value>());
    }
    streaming_.reset();
  }

  CodeCacheState MaybeConsumeCodeCache() {
    // The enum values need to match "WasmCodeCaching" in
    // tools/metrics/histograms/enums.xml.
    enum class WasmCodeCaching {
      kMiss = 0,
      kHit = 1,
      kInvalidCacheEntry = 2,
      kNoCacheHandler = 3,

      kMaxValue = kNoCacheHandler
    };

    if (!cache_handler_) {
      base::UmaHistogramEnumeration("V8.WasmCodeCaching",
                                    WasmCodeCaching::kNoCacheHandler);
      return CodeCacheState::kNoCodeCache;
    }

    // We must wait until we see the first byte of the response body before
    // checking for GetCachedMetadata(). The serialized cache metadata is
    // guaranteed to be set on the handler before the body stream is provided,
    // but this can happen some time after the Response head is received.
    scoped_refptr<CachedMetadata> cached_module =
        cache_handler_->GetCachedMetadata(kWasmModuleTag);
    if (!cached_module) {
      base::UmaHistogramEnumeration("V8.WasmCodeCaching",
                                    WasmCodeCaching::kMiss);
      return CodeCacheState::kNoCodeCache;
    }

    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.moduleCacheHit", TRACE_EVENT_SCOPE_THREAD,
                         "url", url_.Utf8(), "consumedCacheSize",
                         cached_module->size());

    bool is_valid =
        cached_module->size() >= kWireBytesDigestSize &&
        streaming_->SetCompiledModuleBytes(
            reinterpret_cast<const uint8_t*>(cached_module->Data()) +
                kWireBytesDigestSize,
            cached_module->size() - kWireBytesDigestSize);

    if (!is_valid) {
      TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                           "v8.wasm.moduleCacheInvalid",
                           TRACE_EVENT_SCOPE_THREAD);
      base::UmaHistogramEnumeration("V8.WasmCodeCaching",
                                    WasmCodeCaching::kInvalidCacheEntry);
      // TODO(mythria): Also support using context specific code cache host
      // here. When we pass nullptr for CodeCacheHost we use per-process
      // interface. Currently this code is run on a thread started via a
      // Platform::PostJob. So it isn't safe to use CodeCacheHost interface
      // that was bound on the frame / worker threads. We should instead post
      // a task back to the frame / worker threads with the required data
      // which can then write to generated code caches.
      cache_handler_->ClearCachedMetadata(
          /*code_cache_host*/ nullptr,
          CachedMetadataHandler::kClearPersistentStorage);
      return CodeCacheState::kNoCodeCache;
    }

    base::UmaHistogramEnumeration("V8.WasmCodeCaching", WasmCodeCaching::kHit);
    // Keep the buffer alive until V8 is ready to deserialize it.
    // TODO(wasm): Shorten the life time of {cached_module} to reduce memory
    // usage.
    code_caching_callback_->SetBuffer(cached_module);
    return CodeCacheState::kUseCodeCache;
  }

  bool HasValidCodeCache() {
    if (code_cache_state_ != CodeCacheState::kUseCodeCache)
      return false;
    if (!cache_handler_)
      return false;
    scoped_refptr<CachedMetadata> cached_module =
        cache_handler_->GetCachedMetadata(kWasmModuleTag);
    if (!cached_module)
      return false;
    if (cached_module->size() < kWireBytesDigestSize)
      return false;

    DigestValue wire_bytes_digest;
    digestor_.Finish(wire_bytes_digest);
    if (digestor_.has_failed() ||
        memcmp(wire_bytes_digest.data(), cached_module->Data(),
               kWireBytesDigestSize) != 0) {
      TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                           "v8.wasm.moduleCacheInvalidDigest",
                           TRACE_EVENT_SCOPE_THREAD);
      cache_handler_->ClearCachedMetadata(
          /*code_cache_host*/ nullptr,
          CachedMetadataHandler::kClearPersistentStorage);
      return false;
    }

    return true;
  }

  const String url_;
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  std::shared_ptr<v8::WasmStreaming> streaming_;
  const Member<ScriptState> script_state_;
  Member<ScriptCachedMetadataHandler> cache_handler_;
  std::shared_ptr<WasmCodeCachingCallback> code_caching_callback_;
  CodeCacheState code_cache_state_ = CodeCacheState::kBeforeFirstByte;
  Digestor digestor_{kHashAlgorithmSha256};
};

// TODO(mtrofin): WasmDataLoaderClient is necessary so we may provide an
// argument to BodyStreamBuffer::startLoading, however, it fulfills
// a very small role. Consider refactoring to avoid it.
class WasmDataLoaderClient final
    : public GarbageCollected<WasmDataLoaderClient>,
      public FetchDataLoader::Client {
 public:
  explicit WasmDataLoaderClient(FetchDataLoaderForWasmStreaming* loader)
      : loader_(loader) {}

  WasmDataLoaderClient(const WasmDataLoaderClient&) = delete;
  WasmDataLoaderClient& operator=(const WasmDataLoaderClient&) = delete;

  void DidFetchDataLoadedCustomFormat() override {}
  void DidFetchDataLoadFailed() override { NOTREACHED_IN_MIGRATION(); }
  void Abort() override { loader_->AbortFromClient(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  Member<FetchDataLoaderForWasmStreaming> loader_;
};

// Convert an exception to an abort message for WasmStreaming. This rejects the
// promise instead of actually throwing the exception.
// No further methods should be called on the WasmStreaming object afterwards,
// hence we receive the shared_ptr by reference and clear it.
void PropagateExceptionToWasmStreaming(
    ScriptState* script_state,
    v8::Local<v8::Value> exception,
    std::shared_ptr<v8::WasmStreaming>& streaming) {
  ApplyContextToException(script_state, exception,
                          ExceptionContext(v8::ExceptionContext::kOperation,
                                           "WebAssembly", "compile"));
  streaming->Abort(exception);
  streaming.reset();
}

scoped_refptr<base::SingleThreadTaskRunner> GetContextTaskRunner(
    ExecutionContext& execution_context) {
  if (execution_context.IsWorkerGlobalScope()) {
    WorkerOrWorkletGlobalScope& global_scope =
        To<WorkerOrWorkletGlobalScope>(execution_context);
    return global_scope.GetThread()
        ->GetWorkerBackingThread()
        .BackingThread()
        .GetTaskRunner();
  }

  if (execution_context.IsWindow()) {
    return DynamicTo<LocalDOMWindow>(execution_context)
        ->GetTaskRunner(TaskType::kInternalNavigationAssociated);
  }

  DCHECK(execution_context.IsWorkletGlobalScope());
  WorkletGlobalScope& worklet_global_scope =
      To<WorkletGlobalScope>(execution_context);
  if (worklet_global_scope.IsMainThreadWorkletGlobalScope()) {
    return worklet_global_scope.GetFrame()->GetTaskRunner(
        TaskType::kInternalNavigationAssociated);
  }

  return worklet_global_scope.GetThread()
      ->GetWorkerBackingThread()
      .BackingThread()
      .GetTaskRunner();
}

void StreamFromResponseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "v8.wasm.streamFromResponseCallback",
                       TRACE_EVENT_SCOPE_THREAD);
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());

  ScriptState* script_state = ScriptState::ForCurrentRealm(args);
  if (!script_state->ContextIsValid()) {
    // We do not have an execution context, we just abort streaming compilation
    // immediately without error.
    streaming->Abort(v8::Local<v8::Value>());
    return;
  }

  // The enum values need to match "WasmStreamingInputType" in
  // tools/metrics/histograms/enums.xml.
  enum class WasmStreamingInputType {
    kNoResponse = 0,
    kResponseNotOK = 1,
    kWrongMimeType = 2,
    kReponseEmpty = 3,
    kReponseLocked = 4,
    kNoURL = 5,
    kValidHttp = 6,
    kValidHttps = 7,
    kValidDataURL = 8,
    kValidFileURL = 9,
    kValidBlob = 10,
    kValidChromeExtension = 11,
    kValidOtherProtocol = 12,

    kMaxValue = kValidOtherProtocol
  };

  Response* response = V8Response::ToWrappable(args.GetIsolate(), args[0]);
  if (!response) {
    base::UmaHistogramEnumeration("V8.WasmStreamingInputType",
                                  WasmStreamingInputType::kNoResponse);
    auto exception = V8ThrowException::CreateTypeError(
        args.GetIsolate(),
        "An argument must be provided, which must be a "
        "Response or Promise<Response> object");
    PropagateExceptionToWasmStreaming(script_state, exception, streaming);
    return;
  }

  if (!response->ok()) {
    base::UmaHistogramEnumeration("V8.WasmStreamingInputType",
                                  WasmStreamingInputType::kResponseNotOK);
    auto exception = V8ThrowException::CreateTypeError(
        args.GetIsolate(), "HTTP status code is not ok");
    PropagateExceptionToWasmStreaming(script_state, exception, streaming);
    return;
  }

  // The spec explicitly disallows any extras on the Content-Type header,
  // so we check against ContentType() rather than MimeType(), which
  // implicitly strips extras.
  if (!EqualIgnoringASCIICase(response->ContentType(), "application/wasm")) {
    base::UmaHistogramEnumeration("V8.WasmStreamingInputType",
                                  WasmStreamingInputType::kWrongMimeType);
    auto exception = V8ThrowException::CreateTypeError(
        args.GetIsolate(),
        "Incorrect response MIME type. Expected 'application/wasm'.");
    PropagateExceptionToWasmStreaming(script_state, exception, streaming);
    return;
  }

  if (response->IsBodyLocked() || response->IsBodyUsed()) {
    base::UmaHistogramEnumeration("V8.WasmStreamingInputType",
                                  WasmStreamingInputType::kReponseLocked);
    auto exception = V8ThrowException::CreateTypeError(
        args.GetIsolate(),
        "Cannot compile WebAssembly.Module from an already read Response");
    PropagateExceptionToWasmStreaming(script_state, exception, streaming);
    return;
  }

  if (!response->BodyBuffer()) {
    base::UmaHistogramEnumeration("V8.WasmStreamingInputType",
                                  WasmStreamingInputType::kReponseEmpty);
    // Since the status is 2xx (ok), this must be status 204 (No Content),
    // status 205 (Reset Content) or a malformed status 200 (OK).
    auto exception = V8ThrowException::CreateWasmCompileError(
        args.GetIsolate(), "Empty WebAssembly module");
    PropagateExceptionToWasmStreaming(script_state, exception, streaming);
    return;
  }

  auto protocol_type = WasmStreamingInputType::kNoURL;
  if (const KURL* kurl = response->GetResponse()->Url()) {
    String protocol = kurl->Protocol();
    // Http and https can be cached; also track other protocols we expect in
    // Wasm streaming. If {kValidOtherProtocol} spikes, we should add more enum
    // values.
    protocol_type = protocol == "http"    ? WasmStreamingInputType::kValidHttp
                    : protocol == "https" ? WasmStreamingInputType::kValidHttps
                    : protocol == "data" ? WasmStreamingInputType::kValidDataURL
                    : protocol == "file" ? WasmStreamingInputType::kValidFileURL
                    : protocol == "blob" ? WasmStreamingInputType::kValidBlob
                    : protocol == "chrome-extension"
                        ? WasmStreamingInputType::kValidChromeExtension
                        : WasmStreamingInputType::kValidOtherProtocol;
  }
  base::UmaHistogramEnumeration("V8.WasmStreamingInputType", protocol_type);

  String url = response->url();
  const std::string& url_utf8 = url.Utf8();
  streaming->SetUrl(url_utf8.c_str(), url_utf8.size());
  auto* cache_handler = response->BodyBuffer()->GetCachedMetadataHandler();
  std::shared_ptr<WasmCodeCachingCallback> code_caching_callback;
  if (cache_handler) {
    auto* execution_context = ExecutionContext::From(script_state);
    DCHECK_NE(execution_context, nullptr);

    code_caching_callback = std::make_shared<WasmCodeCachingCallback>(
        url, response->GetResponse()->InternalResponse()->ResponseTime(),
        response->GetResponse()->InternalResponse()->CacheStorageCacheName(),
        GetContextTaskRunner(*execution_context), execution_context);
    streaming->SetMoreFunctionsCanBeSerializedCallback(
        [code_caching_callback](v8::CompiledWasmModule compiled_module) {
          code_caching_callback->OnMoreFunctionsCanBeSerialized(
              std::move(compiled_module));
        });
  }

  FetchDataLoaderForWasmStreaming* loader =
      MakeGarbageCollected<FetchDataLoaderForWasmStreaming>(
          url, std::move(streaming), script_state, cache_handler,
          code_caching_callback);
  response->BodyBuffer()->StartLoading(
      loader, MakeGarbageCollected<WasmDataLoaderClient>(loader),
      PassThroughException(args.GetIsolate()));
}

}  // namespace

void WasmResponseExtensions::Initialize(v8::Isolate* isolate) {
  isolate->SetWasmStreamingCallback(StreamFromResponseCallback);
}

}  // namespace blink
