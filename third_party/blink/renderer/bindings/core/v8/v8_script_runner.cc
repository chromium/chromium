/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

// Used to throw an exception before we exceed the C++ stack and crash.
// This limit was arrived at arbitrarily. crbug.com/449744
const int kMaxRecursionDepth = 44;

bool InDiscardExperiment() {
  return base::FeatureList::IsEnabled(
      blink::features::kDiscardCodeCacheAfterFirstUse);
}

// In order to make sure all pending messages to be processed in
// v8::Function::Call, we don't call throwStackOverflowException
// directly. Instead, we create a v8::Function of
// throwStackOverflowException and call it.
void ThrowStackOverflowException(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8ThrowException::ThrowRangeError(info.GetIsolate(),
                                    "Maximum call stack size exceeded.");
}

void ThrowScriptForbiddenException(v8::Isolate* isolate) {
  V8ThrowException::ThrowError(isolate, "Script execution is forbidden.");
}

v8::MaybeLocal<v8::Value> ThrowStackOverflowExceptionIfNeeded(
    v8::Isolate* isolate,
    v8::MicrotaskQueue* microtask_queue) {
  if (V8PerIsolateData::From(isolate)->IsHandlingRecursionLevelError()) {
    // If we are already handling a recursion level error, we should
    // not invoke v8::Function::Call.
    return v8::Undefined(isolate);
  }
  v8::MicrotasksScope microtasks_scope(
      isolate, microtask_queue, v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8PerIsolateData::From(isolate)->SetIsHandlingRecursionLevelError(true);

  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MaybeLocal<v8::Value> result =
      v8::Function::New(context, ThrowStackOverflowException,
                        v8::Local<v8::Value>(), 0,
                        v8::ConstructorBehavior::kThrow)
          .ToLocalChecked()
          ->Call(context, v8::Undefined(isolate), 0, nullptr);

  V8PerIsolateData::From(isolate)->SetIsHandlingRecursionLevelError(false);
  return result;
}

v8::MaybeLocal<v8::Script> CompileScriptInternal(
    v8::Isolate* isolate,
    ExecutionContext* execution_context,
    const ScriptSourceCode& source_code,
    v8::ScriptOrigin origin,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    inspector_compile_script_event::V8CacheResult* cache_result) {
  v8::Local<v8::String> code = V8String(isolate, source_code.Source());

  if (ScriptStreamer* streamer = source_code.Streamer()) {
    // Final compile call for a streamed compilation.
    // Streaming compilation may involve use of code cache.
    // TODO(leszeks): Add compile timer to streaming compilation.
    DCHECK(streamer->IsFinished());
    DCHECK(!streamer->IsStreamingSuppressed());
    return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(),
                                       streamer->Source(), code, origin);
  }

  // Allow inspector to use its own compilation cache store.
  v8::ScriptCompiler::CachedData* inspector_data = nullptr;
  probe::ConsumeCompilationCache(execution_context, source_code,
                                 &inspector_data);
  if (inspector_data) {
    v8::ScriptCompiler::Source source(code, origin, inspector_data);
    v8::MaybeLocal<v8::Script> script =
        v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source,
                                    v8::ScriptCompiler::kConsumeCodeCache);
    return script;
  }

  switch (compile_options) {
    case v8::ScriptCompiler::kNoCompileOptions:
    case v8::ScriptCompiler::kEagerCompile: {
      v8::ScriptCompiler::Source source(code, origin);
      return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source,
                                         compile_options, no_cache_reason);
    }

    case v8::ScriptCompiler::kConsumeCodeCache: {
      // Compile a script, and consume a V8 cache that was generated previously.
      SingleCachedMetadataHandler* cache_handler = source_code.CacheHandler();
      v8::ScriptCompiler::CachedData* cached_data =
          V8CodeCache::CreateCachedData(cache_handler);
      v8::ScriptCompiler::Source source(code, origin, cached_data);
      v8::MaybeLocal<v8::Script> script =
          v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source,
                                      v8::ScriptCompiler::kConsumeCodeCache);

      if (cached_data->rejected) {
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kClearPersistentStorage);
      } else if (InDiscardExperiment()) {
        // Experimentally free code cache from memory after first use. See
        // http://crbug.com/1045052.
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kDiscardLocally);
      }
      if (cache_result) {
        cache_result->consume_result = base::make_optional(
            inspector_compile_script_event::V8CacheResult::ConsumeResult(
                v8::ScriptCompiler::kConsumeCodeCache, cached_data->length,
                cached_data->rejected));
      }
      return script;
    }
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED();
  return v8::MaybeLocal<v8::Script>();
}

int GetMicrotasksScopeDepth(v8::Isolate* isolate,
                            v8::MicrotaskQueue* microtask_queue) {
  if (microtask_queue)
    return microtask_queue->GetMicrotasksScopeDepth();
  return v8::MicrotasksScope::GetCurrentDepth(isolate);
}

}  // namespace

v8::MaybeLocal<v8::Script> V8ScriptRunner::CompileScript(
    ScriptState* script_state,
    const ScriptSourceCode& source,
    SanitizeScriptErrors sanitize_script_errors,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    const ReferrerScriptInfo& referrer_info) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (source.Source().length() >= v8::String::kMaxLength) {
    V8ThrowException::ThrowError(isolate, "Source file too large.");
    return v8::Local<v8::Script>();
  }

  const String& file_name = source.Url();
  const TextPosition& script_start_position = source.StartPosition();

  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                     file_name.Utf8());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  probe::V8Compile probe(execution_context, file_name,
                         script_start_position.line_.ZeroBasedInt(),
                         script_start_position.column_.ZeroBasedInt());

  // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
  // 1, whereas v8 starts at 0.
  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, script_start_position.line_.ZeroBasedInt()),
      v8::Integer::New(isolate, script_start_position.column_.ZeroBasedInt()),
      v8::Boolean::New(isolate, sanitize_script_errors ==
                                    SanitizeScriptErrors::kDoNotSanitize),
      v8::Local<v8::Integer>(), V8String(isolate, source.SourceMapUrl()),
      v8::Boolean::New(
          isolate, sanitize_script_errors == SanitizeScriptErrors::kSanitize),
      v8::False(isolate),  // is_wasm
      v8::False(isolate),  // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));

  if (!*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kTraceEventCategoryGroup)) {
    return CompileScriptInternal(isolate, execution_context, source, origin,
                                 compile_options, no_cache_reason, nullptr);
  }

  inspector_compile_script_event::V8CacheResult cache_result;
  v8::MaybeLocal<v8::Script> script =
      CompileScriptInternal(isolate, execution_context, source, origin,
                            compile_options, no_cache_reason, &cache_result);
  TRACE_EVENT_END1(kTraceEventCategoryGroup, "v8.compile", "data",
                   inspector_compile_script_event::Data(
                       file_name, script_start_position, cache_result,
                       source.Streamer(), source.NotStreamingReason()));
  return script;
}

v8::MaybeLocal<v8::Module> V8ScriptRunner::CompileModule(
    v8::Isolate* isolate,
    const String& source_text,
    SingleCachedMetadataHandler* cache_handler,
    const String& file_name,
    const TextPosition& start_position,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    const ReferrerScriptInfo& referrer_info) {
  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compileModule", "fileName",
                     file_name.Utf8());

  // |resource_is_shared_cross_origin| is always true and |resource_is_opaque|
  // is always false because CORS is enforced to module scripts.
  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, start_position.line_.ZeroBasedInt()),
      v8::Integer::New(isolate, start_position.column_.ZeroBasedInt()),
      v8::Boolean::New(isolate, true),   // resource_is_shared_cross_origin
      v8::Local<v8::Integer>(),          // script id
      v8::String::Empty(isolate),        // source_map_url
      v8::Boolean::New(isolate, false),  // resource_is_opaque
      v8::False(isolate),                // is_wasm
      v8::True(isolate),                 // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));

  v8::Local<v8::String> code = V8String(isolate, source_text);

  v8::MaybeLocal<v8::Module> script;
  inspector_compile_script_event::V8CacheResult cache_result;

  switch (compile_options) {
    case v8::ScriptCompiler::kNoCompileOptions:
    case v8::ScriptCompiler::kEagerCompile: {
      v8::ScriptCompiler::Source source(code, origin);
      script = v8::ScriptCompiler::CompileModule(
          isolate, &source, compile_options, no_cache_reason);
      break;
    }

    case v8::ScriptCompiler::kConsumeCodeCache: {
      // Compile a script, and consume a V8 cache that was generated previously.
      DCHECK(cache_handler);
      v8::ScriptCompiler::CachedData* cached_data =
          V8CodeCache::CreateCachedData(cache_handler);
      v8::ScriptCompiler::Source source(code, origin, cached_data);
      script = v8::ScriptCompiler::CompileModule(
          isolate, &source, compile_options, no_cache_reason);
      if (cached_data->rejected) {
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kClearPersistentStorage);
      } else if (InDiscardExperiment()) {
        // Experimentally free code cache from memory after first use. See
        // http://crbug.com/1045052.
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kDiscardLocally);
      }
      cache_result.consume_result = base::make_optional(
          inspector_compile_script_event::V8CacheResult::ConsumeResult(
              compile_options, cached_data->length, cached_data->rejected));
      break;
    }
  }

  TRACE_EVENT_END1(kTraceEventCategoryGroup, "v8.compileModule", "data",
                   inspector_compile_script_event::Data(
                       file_name, start_position, cache_result, false,
                       ScriptStreamer::NotStreamingReason::kModuleScript));

  return script;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::RunCompiledScript(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script,
    ExecutionContext* context) {
  DCHECK(!script.IsEmpty());
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  ScopedFrameBlamer frame_blamer(window ? window->GetFrame() : nullptr);

  v8::Local<v8::Value> script_name =
      script->GetUnboundScript()->GetScriptName();
  TRACE_EVENT1("v8", "v8.run", "fileName",
               TRACE_STR_COPY(*v8::String::Utf8Value(isolate, script_name)));
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  v8::MicrotaskQueue* microtask_queue = ToMicrotaskQueue(context);
  if (GetMicrotasksScopeDepth(isolate, microtask_queue) > kMaxRecursionDepth)
    return ThrowStackOverflowExceptionIfNeeded(isolate, microtask_queue);

  CHECK(!context->ContextLifecycleObserverSet().IsIteratingOverObservers());

  // Run the script and keep track of the current recursion depth.
  v8::MaybeLocal<v8::Value> result;
  {
    if (ScriptForbiddenScope::IsScriptForbidden()) {
      ThrowScriptForbiddenException(isolate);
      return v8::MaybeLocal<v8::Value>();
    }

    v8::Isolate::SafeForTerminationScope safe_for_termination(isolate);
    v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::Local<v8::String> script_url;
    if (!script_name->ToString(isolate->GetCurrentContext())
             .ToLocal(&script_url))
      return result;

    // ToCoreString here should be zero copy due to externalized string
    // unpacked.
    probe::ExecuteScript probe(context, ToCoreString(script_url),
                               script->GetUnboundScript()->GetId());
    result = script->Run(isolate->GetCurrentContext());
  }

  CHECK(!isolate->IsDead());
  return result;
}

ScriptEvaluationResult V8ScriptRunner::CompileAndRunScript(
    v8::Isolate* isolate,
    ScriptState* script_state,
    ExecutionContext* execution_context,
    const ScriptSourceCode& source,
    const KURL& base_url,
    SanitizeScriptErrors sanitize_script_errors,
    const ScriptFetchOptions& fetch_options,
    mojom::blink::V8CacheOptions v8_cache_options,
    RethrowErrorsOption rethrow_errors) {
  DCHECK_EQ(isolate, script_state->GetIsolate());

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;
  TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data",
               inspector_evaluate_script_event::Data(
                   frame, source.Url().GetString(), source.StartPosition()));

  // Scope for |v8::TryCatch|.
  {
    v8::TryCatch try_catch(isolate);
    // Step 8.3. Otherwise, rethrow errors is false. Perform the following
    // steps: [spec text]
    // Step 8.3.1. Report the exception given by evaluationStatus.[[Value]]
    // for script. [spec text]
    //
    // This will be done inside V8 by setting TryCatch::SetVerbose(true) here.
    if (!rethrow_errors.ShouldRethrow()) {
      try_catch.SetVerbose(true);
    }

    // Omit storing base URL if it is same as source URL.
    // Note: This improves chance of getting into a fast path in
    //       ReferrerScriptInfo::ToV8HostDefinedOptions.
    KURL stored_base_url = (base_url == source.Url()) ? KURL() : base_url;

    // TODO(hiroshige): Remove this code and related use counters once the
    // measurement is done.
    ReferrerScriptInfo::BaseUrlSource base_url_source =
        ReferrerScriptInfo::BaseUrlSource::kOther;
    if (source.SourceLocationType() ==
            ScriptSourceLocationType::kExternalFile &&
        !base_url.IsNull()) {
      switch (sanitize_script_errors) {
        case SanitizeScriptErrors::kDoNotSanitize:
          base_url_source =
              ReferrerScriptInfo::BaseUrlSource::kClassicScriptCORSSameOrigin;
          break;
        case SanitizeScriptErrors::kSanitize:
          base_url_source =
              ReferrerScriptInfo::BaseUrlSource::kClassicScriptCORSCrossOrigin;
          break;
      }
    }
    const ReferrerScriptInfo referrer_info(stored_base_url, fetch_options,
                                           base_url_source);

    v8::Local<v8::Script> script;

    v8::ScriptCompiler::CompileOptions compile_options;
    V8CodeCache::ProduceCacheOptions produce_cache_options;
    v8::ScriptCompiler::NoCacheReason no_cache_reason;
    std::tie(compile_options, produce_cache_options, no_cache_reason) =
        V8CodeCache::GetCompileOptions(v8_cache_options, source);

    v8::MaybeLocal<v8::Value> maybe_result;
    if (V8ScriptRunner::CompileScript(script_state, source,
                                      sanitize_script_errors, compile_options,
                                      no_cache_reason, referrer_info)
            .ToLocal(&script)) {
      maybe_result =
          V8ScriptRunner::RunCompiledScript(isolate, script, execution_context);
      probe::ProduceCompilationCache(probe::ToCoreProbeSink(execution_context),
                                     source, script);
      V8CodeCache::ProduceCache(isolate, script, source, produce_cache_options);
    }

    // TODO(crbug/1114601): Investigate whether to check CanContinue() in other
    // script evaluation code paths.
    if (!try_catch.CanContinue()) {
      return ScriptEvaluationResult::FromClassicAborted();
    }

    if (!try_catch.HasCaught()) {
      // Step 10. If evaluationStatus is a normal completion, then return
      // evaluationStatus. [spec text]
      v8::Local<v8::Value> result;
      bool success = maybe_result.ToLocal(&result);
      DCHECK(success);
      return ScriptEvaluationResult::FromClassicSuccess(result);
    }

    DCHECK(maybe_result.IsEmpty());

    if (rethrow_errors.ShouldRethrow() &&
        sanitize_script_errors == SanitizeScriptErrors::kDoNotSanitize) {
      // Step 8.1. If rethrow errors is true and script's muted errors is
      // false, then: [spec text]
      //
      // Step 8.1.2. Rethrow evaluationStatus.[[Value]]. [spec text]
      //
      // We rethrow exceptions reported from importScripts() here. The
      // original filename/lineno/colno information (which points inside of
      // imported scripts) is kept through ReThrow(), and will be eventually
      // reported to WorkerGlobalScope.onerror via `TryCatch::SetVerbose(true)`
      // called at top-level worker script evaluation.
      try_catch.ReThrow();
      return ScriptEvaluationResult::FromClassicException();
    }
  }
  // |v8::TryCatch| is (and should be) exited, before ThrowException() below.

  if (rethrow_errors.ShouldRethrow()) {
    // kDoNotSanitize case is processed and early-exited above.
    DCHECK_EQ(sanitize_script_errors, SanitizeScriptErrors::kSanitize);

    // Step 8.2. If rethrow errors is true and script's muted errors is
    // true, then: [spec text]
    //
    // Step 8.2.2. Throw a "NetworkError" DOMException. [spec text]
    //
    // We don't supply any message here to avoid leaking details of muted
    // errors.
    V8ThrowException::ThrowException(
        isolate, V8ThrowDOMException::CreateOrEmpty(
                     isolate, DOMExceptionCode::kNetworkError,
                     rethrow_errors.Message()));
    return ScriptEvaluationResult::FromClassicException();
  }

  // #report-the-error for rethrow errors == true is already handled via
  // |TryCatch::SetVerbose(true)| above.
  return ScriptEvaluationResult::FromClassicException();
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CompileAndRunInternalScript(
    v8::Isolate* isolate,
    ScriptState* script_state,
    const ScriptSourceCode& source_code) {
  DCHECK_EQ(isolate, script_state->GetIsolate());

  v8::ScriptCompiler::CompileOptions compile_options;
  V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8CodeCache::GetCompileOptions(mojom::blink::V8CacheOptions::kDefault,
                                     source_code);
  // Currently internal scripts don't have cache handlers. So we should not
  // produce cache for them.
  DCHECK_EQ(produce_cache_options,
            V8CodeCache::ProduceCacheOptions::kNoProduceCache);
  v8::Local<v8::Script> script;
  // Use default ScriptReferrerInfo here:
  // - nonce: empty for internal script, and
  // - parser_state: always "not parser inserted" for internal scripts.
  if (!V8ScriptRunner::CompileScript(
           script_state, source_code, SanitizeScriptErrors::kDoNotSanitize,
           compile_options, no_cache_reason, ReferrerScriptInfo())
           .ToLocal(&script))
    return v8::MaybeLocal<v8::Value>();

  TRACE_EVENT0("v8", "v8.run");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
  v8::Isolate::SafeForTerminationScope safe_for_termination(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::MaybeLocal<v8::Value> result = script->Run(isolate->GetCurrentContext());
  CHECK(!isolate->IsDead());
  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallAsConstructor(
    v8::Isolate* isolate,
    v8::Local<v8::Object> constructor,
    ExecutionContext* context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  TRACE_EVENT0("v8", "v8.callAsConstructor");
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  v8::MicrotaskQueue* microtask_queue = ToMicrotaskQueue(context);
  int depth = GetMicrotasksScopeDepth(isolate, microtask_queue);
  if (depth >= kMaxRecursionDepth)
    return ThrowStackOverflowExceptionIfNeeded(isolate, microtask_queue);

  CHECK(!context->ContextLifecycleObserverSet().IsIteratingOverObservers());

  if (ScriptForbiddenScope::IsScriptForbidden()) {
    ThrowScriptForbiddenException(isolate);
    return v8::MaybeLocal<v8::Value>();
  }

  // TODO(dominicc): When inspector supports tracing object
  // invocation, change this to use v8::Object instead of
  // v8::Function. All callers use functions because
  // CustomElementRegistry#define's IDL signature is Function.
  CHECK(constructor->IsFunction());
  v8::Local<v8::Function> function = constructor.As<v8::Function>();

  v8::Isolate::SafeForTerminationScope safe_for_termination(isolate);
  v8::MicrotasksScope microtasks_scope(isolate, ToMicrotaskQueue(context),
                                       v8::MicrotasksScope::kRunMicrotasks);
  probe::CallFunction probe(context, function, depth);

  if (!depth) {
    TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data",
                       inspector_function_call_event::Data(context, function));
  }

  v8::MaybeLocal<v8::Value> result =
      constructor->CallAsConstructor(isolate->GetCurrentContext(), argc, argv);
  CHECK(!isolate->IsDead());

  if (!depth)
    TRACE_EVENT_END0("devtools.timeline", "FunctionCall");

  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallFunction(
    v8::Local<v8::Function> function,
    ExecutionContext* context,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> args[],
    v8::Isolate* isolate) {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;
  ScopedFrameBlamer frame_blamer(frame);
  TRACE_EVENT0("v8", "v8.callFunction");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  v8::MicrotaskQueue* microtask_queue = ToMicrotaskQueue(context);
  int depth = GetMicrotasksScopeDepth(isolate, microtask_queue);
  if (depth >= kMaxRecursionDepth)
    return ThrowStackOverflowExceptionIfNeeded(isolate, microtask_queue);

  CHECK(!context->ContextLifecycleObserverSet().IsIteratingOverObservers());

  if (ScriptForbiddenScope::IsScriptForbidden()) {
    ThrowScriptForbiddenException(isolate);
    return v8::MaybeLocal<v8::Value>();
  }

  DCHECK(!frame || BindingSecurity::ShouldAllowAccessToFrame(
                       ToLocalDOMWindow(function->CreationContext()), frame,
                       BindingSecurity::ErrorReportOption::kDoNotReport));
  v8::Isolate::SafeForTerminationScope safe_for_termination(isolate);
  v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                       v8::MicrotasksScope::kRunMicrotasks);
  if (!depth) {
    TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data",
                       inspector_function_call_event::Data(context, function));
  }

  probe::CallFunction probe(context, function, depth);
  v8::MaybeLocal<v8::Value> result =
      function->Call(isolate->GetCurrentContext(), receiver, argc, args);
  CHECK(!isolate->IsDead());

  if (!depth)
    TRACE_EVENT_END0("devtools.timeline", "FunctionCall");

  return result;
}

class ModuleEvaluationRejectionCallback final : public ScriptFunction {
 public:
  explicit ModuleEvaluationRejectionCallback(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  static v8::Local<v8::Function> CreateFunction(ScriptState* script_state) {
    ModuleEvaluationRejectionCallback* self =
        MakeGarbageCollected<ModuleEvaluationRejectionCallback>(script_state);
    return self->BindToV8Function();
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    ModuleRecord::ReportException(GetScriptState(), value.V8Value());
    return ScriptValue();
  }
};

// <specdef href="https://html.spec.whatwg.org/C/#run-a-module-script">
// Spec with TLA: https://github.com/whatwg/html/pull/4352
ScriptEvaluationResult V8ScriptRunner::EvaluateModule(
    ModuleScript* module_script,
    RethrowErrorsOption rethrow_errors) {
  // <spec step="1">If rethrow errors is not given, let it be false.</spec>

  // <spec step="2">Let settings be the settings object of script.</spec>
  //
  // The settings object is |module_script->SettingsObject()|.
  ScriptState* script_state = module_script->SettingsObject()->GetScriptState();
  DCHECK_EQ(Modulator::From(script_state), module_script->SettingsObject());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  // <spec step="3">Check if we can run script with settings. If this returns
  // "do not run" then return NormalCompletion(empty).</spec>
  if (!execution_context->CanExecuteScripts(kAboutToExecuteScript)) {
    return ScriptEvaluationResult::FromModuleNotRun();
  }

  // <spec step="4">Prepare to run script given settings.</spec>
  //
  // These are placed here to also cover ModuleRecord::ReportException().
  v8::MicrotasksScope microtasks_scope(isolate,
                                       ToMicrotaskQueue(execution_context),
                                       v8::MicrotasksScope::kRunMicrotasks);
  ScriptState::EscapableScope scope(script_state);

  // Without TLA: <spec step="5">Let evaluationStatus be null.</spec>
  ScriptEvaluationResult result = ScriptEvaluationResult::FromModuleNotRun();

  // <spec step="6">If script's error to rethrow is not null, ...</spec>
  if (module_script->HasErrorToRethrow()) {
    // Without TLA: <spec step="6">... then set evaluationStatus to Completion
    //     { [[Type]]: throw, [[Value]]: script's error to rethrow,
    //       [[Target]]: empty }.</spec>
    // With TLA:    <spec step="5">If script's error to rethrow is not null,
    //     then let valuationPromise be a promise rejected with script's error
    //     to rethrow.</spec>
    result = ScriptEvaluationResult::FromModuleException(
        module_script->CreateErrorToRethrow().V8Value());
  } else {
    // <spec step="7">Otherwise:</spec>

    // <spec step="7.1">Let record be script's record.</spec>
    v8::Local<v8::Module> record = module_script->V8Module();
    CHECK(!record.IsEmpty());

    // <spec step="7.2">Set evaluationStatus to record.Evaluate(). ...</spec>

    // Isolate exceptions that occur when executing the code. These exceptions
    // should not interfere with javascript code we might evaluate from C++
    // when returning from here.
    v8::TryCatch try_catch(isolate);

    // Script IDs are not available on errored modules or on non-source text
    // modules, so we give them a default value.
    probe::ExecuteScript probe(execution_context, module_script->SourceURL(),
                               record->GetStatus() != v8::Module::kErrored &&
                                       record->IsSourceTextModule()
                                   ? record->ScriptId()
                                   : v8::UnboundScript::kNoScriptId);

    TRACE_EVENT0("v8,devtools.timeline", "v8.evaluateModule");
    RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
    v8::Isolate::SafeForTerminationScope safe_for_termination(isolate);

    // Do not perform a microtask checkpoint here. A checkpoint is performed
    // only after module error handling to ensure proper timing with and
    // without top-level await.
    v8::Local<v8::Value> v8_result;
    if (!record->Evaluate(script_state->GetContext()).ToLocal(&v8_result)) {
      result =
          ScriptEvaluationResult::FromModuleException(try_catch.Exception());
    } else {
      result = ScriptEvaluationResult::FromModuleSuccess(v8_result);
    }

    // <spec step="7.2">... If Evaluate fails to complete as a result of the
    // user agent aborting the running script, then set evaluationStatus to
    // Completion { [[Type]]: throw, [[Value]]: a new "QuotaExceededError"
    // DOMException, [[Target]]: empty }.</spec>
  }

  // [not specced] Store V8 code cache on successful evaluation.
  if (result.GetResultType() == ScriptEvaluationResult::ResultType::kSuccess) {
    execution_context->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&Modulator::ProduceCacheModuleTreeTopLevel,
                             WrapWeakPersistent(Modulator::From(script_state)),
                             WrapWeakPersistent(module_script)));
  }

  if (!rethrow_errors.ShouldRethrow()) {
    if (base::FeatureList::IsEnabled(features::kTopLevelAwait)) {
      // <spec step="7"> If report errors is true, then upon rejection of
      // evaluationPromise with reason, report the exception given by reason
      // for script.</spec>
      v8::Local<v8::Function> callback_failure =
          ModuleEvaluationRejectionCallback::CreateFunction(script_state);
      // Add a rejection handler to report back errors once the result
      // promise is rejected.
      result.GetPromise(script_state)
          .Then(v8::Local<v8::Function>(), callback_failure);
    } else {
      // <spec step="8">If evaluationStatus is an abrupt completion,
      // then:</spec>
      if (result.GetResultType() ==
          ScriptEvaluationResult::ResultType::kException) {
        // <spec step="8.2">Otherwise, report the exception given by
        // evaluationStatus.[[Value]] for script.</spec>
        ModuleRecord::ReportException(script_state,
                                      result.GetExceptionForModule());
      }
    }
  }

  // <spec step="8">Clean up after running script with settings.</spec>
  // - Partially implement in MicrotaskScope destructor and the
  // - ScriptState::EscapableScope destructor.
  return result.Escape(&scope);
}

void V8ScriptRunner::ReportException(v8::Isolate* isolate,
                                     v8::Local<v8::Value> exception) {
  DCHECK(!exception.IsEmpty());

  // https://html.spec.whatwg.org/C/#report-the-error
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (IsMainThread())
    V8Initializer::MessageHandlerInMainThread(message, exception);
  else
    V8Initializer::MessageHandlerInWorker(message, exception);
}

}  // namespace blink
