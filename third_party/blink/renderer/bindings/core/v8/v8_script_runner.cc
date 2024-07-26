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
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/bindings/buildflags.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_cache_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_common.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

// Used to throw an exception before we exceed the C++ stack and crash.
// This limit was arrived at arbitrarily. crbug.com/449744
const int kMaxRecursionDepth = 44;

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
    ScriptState* script_state,
    const ClassicScript& classic_script,
    v8::ScriptOrigin origin,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    bool can_use_crowdsourced_compile_hints,
    std::optional<inspector_compile_script_event::V8ConsumeCacheResult>*
        cache_result) {
  v8::Local<v8::String> code = V8String(isolate, classic_script.SourceText());

  // TODO(kouhei): Plumb the ScriptState into this function and replace all
  // Isolate->GetCurrentContext in this function with ScriptState->GetContext.
  if (ScriptStreamer* streamer = classic_script.Streamer()) {
    if (v8::ScriptCompiler::StreamedSource* source =
            streamer->Source(v8::ScriptType::kClassic)) {
      // Final compile call for a streamed compilation.
      // Streaming compilation may involve use of code cache.
      // TODO(leszeks): Add compile timer to streaming compilation.
      return v8::ScriptCompiler::Compile(script_state->GetContext(), source,
                                         code, origin);
    }
  }

  // Allow inspector to use its own compilation cache store.
  v8::ScriptCompiler::CachedData* inspector_data = nullptr;
  // The probe below allows inspector to either inject the cached code
  // or override compile_options to force eager compilation of code
  // when producing the cache.
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  probe::ApplyCompilationModeOverride(execution_context, classic_script,
                                      &inspector_data, &compile_options);
  if (inspector_data) {
    v8::ScriptCompiler::Source source(code, origin, inspector_data);
    v8::MaybeLocal<v8::Script> script =
        v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                    v8::ScriptCompiler::kConsumeCodeCache);
    return script;
  }

  switch (static_cast<int>(compile_options)) {
    case v8::ScriptCompiler::kConsumeCompileHints:
    case v8::ScriptCompiler::kConsumeCompileHints |
        v8::ScriptCompiler::kFollowCompileHintsMagicComment: {
      // We can only consume local or crowdsourced compile hints, but
      // not both at the same time. If the page has crowdsourced compile hints,
      // we won't generate local compile hints, so won't ever have them.
      // We'd only have both local and crowdsourced compile hints available in
      // special cases, e.g., if crowdsourced compile hints were temporarily
      // unavailable, we generated local compile hints, and during the next page
      // load we have both available.

      // TODO(40286622): Enable using crowdsourced compile hints and augmenting
      // them with local compile hints. 1) Enable consuming compile hints and at
      // the same time, producing compile hints for functions which were still
      // lazy and 2) enable consuming both kind of compile hints at the same
      // time.
      if (can_use_crowdsourced_compile_hints) {
        base::UmaHistogramEnumeration(
            v8_compile_hints::kStatusHistogram,
            v8_compile_hints::Status::
                kConsumeCrowdsourcedCompileHintsClassicNonStreaming);

        // Based on how `can_use_crowdsourced_compile_hints` in CompileScript is
        // computed, we must get a non-null LocalDOMWindow and LocalFrame here.
        LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
        CHECK(window);
        LocalFrame* frame = window->GetFrame();
        CHECK(frame);
        Page* page = frame->GetPage();
        CHECK(page);
        // This ptr keeps the data alive during v8::ScriptCompiler::Compile.
        std::unique_ptr<v8_compile_hints::V8CrowdsourcedCompileHintsConsumer::
                            DataAndScriptNameHash>
            compile_hint_data =
                page->GetV8CrowdsourcedCompileHintsConsumer()
                    .GetDataWithScriptNameHash(v8_compile_hints::ScriptNameHash(
                        origin.ResourceName(), script_state->GetContext(),
                        isolate));
        v8::ScriptCompiler::Source source(
            code, origin,
            &v8_compile_hints::V8CrowdsourcedCompileHintsConsumer::
                CompileHintCallback,
            compile_hint_data.get());
        return v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                           compile_options, no_cache_reason);
      }
      // No crowdsourced compile hints; compile with local compile hints.
      CHECK(base::FeatureList::IsEnabled(features::kLocalCompileHints));
      base::UmaHistogramEnumeration(
          v8_compile_hints::kStatusHistogram,
          v8_compile_hints::Status::
              kConsumeLocalCompileHintsClassicNonStreaming);
      CachedMetadataHandler* cache_handler = classic_script.CacheHandler();
      CHECK(cache_handler);
      scoped_refptr<CachedMetadata> cached_metadata =
          V8CodeCache::GetCachedMetadataForCompileHints(cache_handler);
      v8_compile_hints::V8LocalCompileHintsConsumer
          v8_local_compile_hints_consumer(cached_metadata.get());
      if (v8_local_compile_hints_consumer.IsRejected()) {
        cache_handler->ClearCachedMetadata(
            ExecutionContext::GetCodeCacheHostFromContext(execution_context),
            CachedMetadataHandler::kClearPersistentStorage);
        // Compile without compile hints.
        compile_options = v8::ScriptCompiler::CompileOptions(
            compile_options & (~v8::ScriptCompiler::kConsumeCompileHints));
        v8::ScriptCompiler::Source source(code, origin);
        return v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                           compile_options, no_cache_reason);
      }
      v8::ScriptCompiler::Source source(
          code, origin,
          v8_compile_hints::V8LocalCompileHintsConsumer::GetCompileHint,
          &v8_local_compile_hints_consumer);
      return v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                         compile_options, no_cache_reason);
    }
    case v8::ScriptCompiler::kProduceCompileHints:
    case v8::ScriptCompiler::kProduceCompileHints |
        v8::ScriptCompiler::kFollowCompileHintsMagicComment: {
      base::UmaHistogramEnumeration(
          v8_compile_hints::kStatusHistogram,
          v8_compile_hints::Status::kProduceCompileHintsClassicNonStreaming);
      v8::ScriptCompiler::Source source(code, origin);
      return v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                         compile_options, no_cache_reason);
    }
    case v8::ScriptCompiler::kNoCompileOptions:
    case v8::ScriptCompiler::kEagerCompile:
    case v8::ScriptCompiler::kFollowCompileHintsMagicComment: {
      base::UmaHistogramEnumeration(
          v8_compile_hints::kStatusHistogram,
          v8_compile_hints::Status::kNoCompileHintsClassicNonStreaming);
      v8::ScriptCompiler::Source source(code, origin);
      return v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                         compile_options, no_cache_reason);
    }

    case v8::ScriptCompiler::kConsumeCodeCache: {
      base::UmaHistogramEnumeration(
          v8_compile_hints::kStatusHistogram,
          v8_compile_hints::Status::kConsumeCodeCacheClassicNonStreaming);
      // Compile a script, and consume a V8 cache that was generated previously.
      CachedMetadataHandler* cache_handler = classic_script.CacheHandler();
      ScriptCacheConsumer* cache_consumer = classic_script.CacheConsumer();
      scoped_refptr<CachedMetadata> cached_metadata =
          V8CodeCache::GetCachedMetadata(cache_handler);
      const bool full_code_cache = V8CodeCache::IsFull(cached_metadata.get());
      v8::ScriptCompiler::Source source(
          code, origin,
          V8CodeCache::CreateCachedData(cached_metadata).release(),
          cache_consumer
              ? cache_consumer->TakeV8ConsumeTask(cached_metadata.get())
              : nullptr);
      const v8::ScriptCompiler::CachedData* cached_data =
          source.GetCachedData();
      v8::MaybeLocal<v8::Script> script =
          v8::ScriptCompiler::Compile(script_state->GetContext(), &source,
                                      v8::ScriptCompiler::kConsumeCodeCache);
      cache_handler->DidUseCodeCache();
      // The ScriptState has an associated context. We expect the current
      // context to match the context associated with Script context when
      // compiling the script for main world. Hence it is safe to use the
      // CodeCacheHost corresponding to the script execution context. For
      // isolated world (for ex: extension scripts), the current context
      // may not match the script context. Though currently code caching is
      // disabled for extensions.
      if (cached_data->rejected) {
        cache_handler->ClearCachedMetadata(
            ExecutionContext::GetCodeCacheHostFromContext(
                ExecutionContext::From(script_state)),
            CachedMetadataHandler::kClearPersistentStorage);
      }
      if (cache_result) {
        *cache_result = std::make_optional(
            inspector_compile_script_event::V8ConsumeCacheResult(
                cached_data->length, cached_data->rejected, full_code_cache));
      }
      return script;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED_IN_MIGRATION();
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
    const ClassicScript& classic_script,
    v8::ScriptOrigin origin,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    bool can_use_crowdsourced_compile_hints) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (classic_script.SourceText().length() >= v8::String::kMaxLength) {
    V8ThrowException::ThrowError(isolate, "Source file too large.");
    return v8::Local<v8::Script>();
  }

  const String& file_name = classic_script.SourceUrl();
  const TextPosition& script_start_position = classic_script.StartPosition();

  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                     file_name.Utf8());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  probe::V8Compile probe(execution_context, file_name,
                         script_start_position.line_.ZeroBasedInt(),
                         script_start_position.column_.ZeroBasedInt());

  if (!*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kTraceEventCategoryGroup)) {
    return CompileScriptInternal(isolate, script_state, classic_script, origin,
                                 compile_options, no_cache_reason,
                                 can_use_crowdsourced_compile_hints, nullptr);
  }

  std::optional<inspector_compile_script_event::V8ConsumeCacheResult>
      cache_result;
  v8::MaybeLocal<v8::Script> script = CompileScriptInternal(
      isolate, script_state, classic_script, origin, compile_options,
      no_cache_reason, can_use_crowdsourced_compile_hints, &cache_result);
  TRACE_EVENT_END1(
      kTraceEventCategoryGroup, "v8.compile", "data",
      [&](perfetto::TracedValue context) {
        inspector_compile_script_event::Data(
            std::move(context), file_name, script_start_position, cache_result,
            compile_options == v8::ScriptCompiler::kEagerCompile,
            classic_script.Streamer(), classic_script.NotStreamingReason());
      });
  return script;
}

v8::MaybeLocal<v8::Module> V8ScriptRunner::CompileModule(
    v8::Isolate* isolate,
    const ModuleScriptCreationParams& params,
    const TextPosition& start_position,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    const ReferrerScriptInfo& referrer_info) {
  const String file_name = params.SourceURL();
  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compileModule", "fileName",
                     file_name.Utf8());

  // |resource_is_shared_cross_origin| is always true and |resource_is_opaque|
  // is always false because CORS is enforced to module scripts.
  v8::ScriptOrigin origin(
      V8String(isolate, file_name), start_position.line_.ZeroBasedInt(),
      start_position.column_.ZeroBasedInt(),
      true,                        // resource_is_shared_cross_origin
      -1,                          // script id
      v8::String::Empty(isolate),  // source_map_url
      false,                       // resource_is_opaque
      false,                       // is_wasm
      true,                        // is_module
      referrer_info.ToV8HostDefinedOptions(isolate, params.SourceURL()));

  v8::Local<v8::String> code = V8String(isolate, params.GetSourceText());
  std::optional<inspector_compile_script_event::V8ConsumeCacheResult>
      cache_result;
  v8::MaybeLocal<v8::Module> script;
  ScriptStreamer* streamer = params.GetScriptStreamer();
  if (streamer) {
    // Final compile call for a streamed compilation.
    // Streaming compilation may involve use of code cache.
    // TODO(leszeks): Add compile timer to streaming compilation.
    script = v8::ScriptCompiler::CompileModule(
        isolate->GetCurrentContext(), streamer->Source(v8::ScriptType::kModule),
        code, origin);
  } else {
    switch (compile_options) {
      // TODO(40286622): Compile hints for modules.
      case v8::ScriptCompiler::kProduceCompileHints:
      case v8::ScriptCompiler::kConsumeCompileHints:
        compile_options = v8::ScriptCompiler::kNoCompileOptions;
        ABSL_FALLTHROUGH_INTENDED;
      case v8::ScriptCompiler::kNoCompileOptions:
      case v8::ScriptCompiler::kEagerCompile: {
        base::UmaHistogramEnumeration(
            v8_compile_hints::kStatusHistogram,
            v8_compile_hints::Status::kNoCompileHintsModuleNonStreaming);
        v8::ScriptCompiler::Source source(code, origin);
        script = v8::ScriptCompiler::CompileModule(
            isolate, &source, compile_options, no_cache_reason);
        break;
      }

      case v8::ScriptCompiler::kConsumeCodeCache: {
        base::UmaHistogramEnumeration(
            v8_compile_hints::kStatusHistogram,
            v8_compile_hints::Status::kConsumeCodeCacheModuleNonStreaming);
        // Compile a script, and consume a V8 cache that was generated
        // previously.
        CachedMetadataHandler* cache_handler = params.CacheHandler();
        DCHECK(cache_handler);
        cache_handler->DidUseCodeCache();
        const scoped_refptr<CachedMetadata> cached_metadata =
            V8CodeCache::GetCachedMetadata(cache_handler);
        const bool full_code_cache = V8CodeCache::IsFull(cached_metadata.get());
        // TODO(leszeks): Add support for passing in ScriptCacheConsumer.
        v8::ScriptCompiler::Source source(
            code, origin,
            V8CodeCache::CreateCachedData(cache_handler).release());
        const v8::ScriptCompiler::CachedData* cached_data =
            source.GetCachedData();
        script = v8::ScriptCompiler::CompileModule(
            isolate, &source, compile_options, no_cache_reason);
        // The ScriptState also has an associated context. We expect the current
        // context to match the context associated with Script context when
        // compiling the module. Hence it is safe to use the CodeCacheHost
        // corresponding to the current execution context.
        ExecutionContext* execution_context =
            ExecutionContext::From(isolate->GetCurrentContext());
        if (cached_data->rejected) {
          cache_handler->ClearCachedMetadata(
              ExecutionContext::GetCodeCacheHostFromContext(execution_context),
              CachedMetadataHandler::kClearPersistentStorage);
        }
        cache_result = std::make_optional(
            inspector_compile_script_event::V8ConsumeCacheResult(
                cached_data->length, cached_data->rejected, full_code_cache));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  TRACE_EVENT_END1(kTraceEventCategoryGroup, "v8.compileModule", "data",
                   [&](perfetto::TracedValue context) {
                     inspector_compile_script_event::Data(
                         std::move(context), file_name, start_position,
                         cache_result,
                         compile_options == v8::ScriptCompiler::kEagerCompile,
                         streamer, params.NotStreamingReason());
                   });
  return script;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::RunCompiledScript(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script,
    v8::Local<v8::Data> host_defined_options,
    ExecutionContext* context) {
  DCHECK(!script.IsEmpty());

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
    if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
      CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    } else {
      DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
    }

    v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                         v8::MicrotasksScope::kRunMicrotasks);
    v8::Local<v8::String> script_url;
    if (!script_name->ToString(isolate->GetCurrentContext())
             .ToLocal(&script_url))
      return result;

    // ToCoreString here should be zero copy due to externalized string
    // unpacked.
    String url = ToCoreString(isolate, script_url);
    probe::ExecuteScript probe(context, isolate->GetCurrentContext(), url,
                               script->GetUnboundScript()->GetId());
    result = script->Run(isolate->GetCurrentContext(), host_defined_options);
  }

  CHECK(!isolate->IsDead());
  return result;
}

namespace {
void DelayedProduceCodeCacheTask(ScriptState* script_state,
                                 v8::Global<v8::Script> script,
                                 CachedMetadataHandler* cache_handler,
                                 size_t source_text_length,
                                 KURL source_url,
                                 TextPosition source_start_position) {
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  V8CodeCache::ProduceCache(
      isolate, ExecutionContext::GetCodeCacheHostFromContext(execution_context),
      script.Get(isolate), cache_handler, source_text_length, source_url,
      source_start_position,
      V8CodeCache::ProduceCacheOptions::kProduceCodeCache);
}
}  // namespace

ScriptEvaluationResult V8ScriptRunner::CompileAndRunScript(
    ScriptState* script_state,
    ClassicScript* classic_script,
    ExecuteScriptPolicy policy,
    RethrowErrorsOption rethrow_errors) {
  if (!script_state)
    return ScriptEvaluationResult::FromClassicNotRun();

  // |script_state->GetContext()| must be initialized here already, typically
  // due to a WindowProxy() call inside ToScriptState*() that is used to get the
  // ScriptState.

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context->IsContextThread());

  if (policy == ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled &&
      !execution_context->CanExecuteScripts(kAboutToExecuteScript)) {
    return ScriptEvaluationResult::FromClassicNotRun();
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  const SanitizeScriptErrors sanitize_script_errors =
      classic_script->GetSanitizeScriptErrors();

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  WorkerOrWorkletGlobalScope* worker_or_worklet_global_scope =
      DynamicTo<WorkerOrWorkletGlobalScope>(execution_context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;

  if (window && window->document()->IsInitialEmptyDocument()) {
    window->GetFrame()->Loader().DidAccessInitialDocument();
  } else if (worker_or_worklet_global_scope) {
    DCHECK_EQ(
        script_state,
        worker_or_worklet_global_scope->ScriptController()->GetScriptState());
    DCHECK(worker_or_worklet_global_scope->ScriptController()
               ->IsContextInitialized());
    DCHECK(worker_or_worklet_global_scope->ScriptController()
               ->IsReadyToEvaluate());
  }

  v8::Context::Scope scope(script_state->GetContext());

  DEVTOOLS_TIMELINE_TRACE_EVENT(
      "EvaluateScript", inspector_evaluate_script_event::Data, isolate, frame,
      classic_script->SourceUrl().GetString(), classic_script->StartPosition());

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

    v8::Local<v8::Script> script;

    CachedMetadataHandler* cache_handler = classic_script->CacheHandler();
    if (cache_handler) {
      cache_handler->Check(
          ExecutionContext::GetCodeCacheHostFromContext(execution_context),
          classic_script->SourceText());
    }
    v8::ScriptCompiler::CompileOptions compile_options;
    V8CodeCache::ProduceCacheOptions produce_cache_options;
    v8::ScriptCompiler::NoCacheReason no_cache_reason;
    Page* page = frame != nullptr ? frame->GetPage() : nullptr;
    const bool is_http = classic_script->SourceUrl().ProtocolIsInHTTPFamily();
    const bool might_generate_crowdsourced_compile_hints =
        is_http && page != nullptr &&
        page->GetV8CrowdsourcedCompileHintsProducer().MightGenerateData();
    const bool can_use_crowdsourced_compile_hints =
        is_http && page != nullptr && page->MainFrame() == frame &&
        page->GetV8CrowdsourcedCompileHintsConsumer().HasData();
    const bool v8_compile_hints_magic_comment_runtime_enabled =
        RuntimeEnabledFeatures::JavaScriptCompileHintsMagicRuntimeEnabled(
            execution_context);

    std::tie(compile_options, produce_cache_options, no_cache_reason) =
        V8CodeCache::GetCompileOptions(
            execution_context->GetV8CacheOptions(), *classic_script,
            might_generate_crowdsourced_compile_hints,
            can_use_crowdsourced_compile_hints,
            v8_compile_hints_magic_comment_runtime_enabled);

    v8::ScriptOrigin origin = classic_script->CreateScriptOrigin(isolate);
    v8::MaybeLocal<v8::Value> maybe_result;
    if (V8ScriptRunner::CompileScript(script_state, *classic_script, origin,
                                      compile_options, no_cache_reason,
                                      can_use_crowdsourced_compile_hints)
            .ToLocal(&script)) {
      DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
          TRACE_DISABLED_BY_DEFAULT("devtools.target-rundown"),
          "ScriptCompiled", inspector_target_rundown_event::Data,
          execution_context, isolate, script_state,
          script->GetUnboundScript()->GetId());
      maybe_result = V8ScriptRunner::RunCompiledScript(
          isolate, script, origin.GetHostDefinedOptions(), execution_context);
      probe::DidProduceCompilationCache(
          probe::ToCoreProbeSink(execution_context), *classic_script, script);

      // The ScriptState has an associated context. We expect the current
      // context to match the context associated with Script context when
      // compiling the script in the main world. Hence it is safe to use the
      // CodeCacheHost corresponding to the script execution context. For
      // isolated world the contexts may not match. Though code caching is
      // disabled for extensions so it is OK to use execution_context here.

      if (produce_cache_options ==
              V8CodeCache::ProduceCacheOptions::kProduceCodeCache &&
          cache_handler) {
        cache_handler->WillProduceCodeCache();
      }
      if (produce_cache_options ==
              V8CodeCache::ProduceCacheOptions::kProduceCodeCache &&
          base::FeatureList::IsEnabled(features::kCacheCodeOnIdle) &&
          (features::kCacheCodeOnIdleDelayServiceWorkerOnlyParam.Get()
               ? execution_context->IsServiceWorkerGlobalScope()
               : true)) {
        auto delay =
            base::Milliseconds(features::kCacheCodeOnIdleDelayParam.Get());
        // Workers don't have a concept of idle tasks, so use a default task for
        // these.
        TaskType task_type =
            frame ? TaskType::kIdleTask : TaskType::kInternalDefault;
        execution_context->GetTaskRunner(task_type)->PostDelayedTask(
            FROM_HERE,
            WTF::BindOnce(&DelayedProduceCodeCacheTask,
                          // TODO(leszeks): Consider passing the
                          // script state as a weak persistent.
                          WrapPersistent(script_state),
                          v8::Global<v8::Script>(isolate, script),
                          WrapPersistent(cache_handler),
                          classic_script->SourceText().length(),
                          classic_script->SourceUrl(),
                          classic_script->StartPosition()),
            delay);
      } else {
        V8CodeCache::ProduceCache(
            isolate,
            ExecutionContext::GetCodeCacheHostFromContext(execution_context),
            script, cache_handler, classic_script->SourceText().length(),
            classic_script->SourceUrl(), classic_script->StartPosition(),
            produce_cache_options);
      }

      // `SharedStorageWorkletGlobalScope` has a out-of-process worklet
      // architecture that does not have a `page` associated.
      // TODO(crbug.com/340920456): Figure out what should be done here.
      if (compile_options == v8::ScriptCompiler::kProduceCompileHints &&
          !execution_context->IsSharedStorageWorkletGlobalScope()) {
        CHECK(page);
        CHECK(frame);
        // We can produce both crowdsourced and local compile hints at the
        // same time.
#if BUILDFLAG(PRODUCE_V8_COMPILE_HINTS)
        // TODO(40286622): Add a compile hints solution for workers.
        // TODO(40286622): Add a compile hints solution for fenced frames.
        // TODO(40286622): Add a compile hints solution for out-of-process
        // iframes.
        page->GetV8CrowdsourcedCompileHintsProducer().RecordScript(
            frame, execution_context, script, script_state);
#endif  // BUILDFLAG(ENABLE_V8_COMPILE_HINTS)
        frame->GetV8LocalCompileHintsProducer().RecordScript(
            execution_context, script, classic_script);
      }
    }

    // TODO(crbug/1114601): Investigate whether to check CanContinue() in other
    // script evaluation code paths.
    if (!try_catch.CanContinue()) {
      if (worker_or_worklet_global_scope)
        worker_or_worklet_global_scope->ScriptController()->ForbidExecution();
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
      return ScriptEvaluationResult::FromClassicExceptionRethrown();
    }

    // Step 8.1.3. Otherwise, rethrow errors is false. Perform the following
    // steps: [spec text]
    if (!rethrow_errors.ShouldRethrow()) {
      // #report-the-error for rethrow errors == true is already handled via
      // `TryCatch::SetVerbose(true)` above.
      return ScriptEvaluationResult::FromClassicException(
          try_catch.Exception());
    }
  }
  // |v8::TryCatch| is (and should be) exited, before ThrowException() below.

  // kDoNotSanitize case is processed and early-exited above.
  DCHECK(rethrow_errors.ShouldRethrow());
  DCHECK_EQ(sanitize_script_errors, SanitizeScriptErrors::kSanitize);

  // Step 8.2. If rethrow errors is true and script's muted errors is true,
  // then: [spec text]
  //
  // Step 8.2.2. Throw a "NetworkError" DOMException. [spec text]
  //
  // We don't supply any message here to avoid leaking details of muted errors.
  V8ThrowException::ThrowException(
      isolate,
      V8ThrowDOMException::CreateOrEmpty(
          isolate, DOMExceptionCode::kNetworkError, rethrow_errors.Message()));
  return ScriptEvaluationResult::FromClassicExceptionRethrown();
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
  if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
    CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  } else {
    DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  }

  // TODO(dominicc): When inspector supports tracing object
  // invocation, change this to use v8::Object instead of
  // v8::Function. All callers use functions because
  // CustomElementRegistry#define's IDL signature is Function.
  CHECK(constructor->IsFunction());
  v8::Local<v8::Function> function = constructor.As<v8::Function>();

  v8::MicrotasksScope microtasks_scope(isolate, ToMicrotaskQueue(context),
                                       v8::MicrotasksScope::kRunMicrotasks);
  probe::CallFunction probe(context, isolate->GetCurrentContext(), function,
                            depth);

  if (!depth) {
    TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data",
                       [&](perfetto::TracedValue ctx) {
                         inspector_function_call_event::Data(std::move(ctx),
                                                             context, function);
                       });
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
    v8::Local<v8::Value> argv[],
    v8::Isolate* isolate) {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
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
  if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
    CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  } else {
    DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  }

  DCHECK(!window || !window->GetFrame() ||
         BindingSecurity::ShouldAllowAccessTo(
             ToLocalDOMWindow(function->GetCreationContextChecked()), window));
  v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                       v8::MicrotasksScope::kRunMicrotasks);
  if (!depth) {
    TRACE_EVENT_BEGIN1("devtools.timeline", "FunctionCall", "data",
                       [&](perfetto::TracedValue trace_context) {
                         inspector_function_call_event::Data(
                             std::move(trace_context), context, function);
                       });
  }

  probe::CallFunction probe(context, isolate->GetCurrentContext(), function,
                            depth);
  v8::MaybeLocal<v8::Value> result = function->Call(
      isolate, isolate->GetCurrentContext(), receiver, argc, argv);
  CHECK(!isolate->IsDead());

  if (!depth)
    TRACE_EVENT_END0("devtools.timeline", "FunctionCall");

  return result;
}

class ModuleEvaluationRejectionCallback final
    : public ScriptFunction::Callable {
 public:
  ModuleEvaluationRejectionCallback() = default;

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    ModuleRecord::ReportException(script_state, value.V8Value());
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

  // TODO(crbug.com/1151165): Ideally v8::Context should be entered before
  // CanExecuteScripts().
  v8::Context::Scope scope(script_state->GetContext());

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
    probe::ExecuteScript probe(execution_context, script_state->GetContext(),
                               module_script->SourceUrl(),
                               record->GetStatus() != v8::Module::kErrored &&
                                       record->IsSourceTextModule()
                                   ? record->ScriptId()
                                   : v8::UnboundScript::kNoScriptId);

    TRACE_EVENT0("v8,devtools.timeline", "v8.evaluateModule");
    RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

    // Do not perform a microtask checkpoint here. A checkpoint is performed
    // only after module error handling to ensure proper timing with and
    // without top-level await.

    v8::MaybeLocal<v8::Value> maybe_result =
        record->Evaluate(script_state->GetContext());

    if (!try_catch.CanContinue())
      return ScriptEvaluationResult::FromModuleAborted();

    DCHECK(!try_catch.HasCaught());
    result = ScriptEvaluationResult::FromModuleSuccess(
        maybe_result.ToLocalChecked());

    // <spec step="7.2">... If Evaluate fails to complete as a result of the
    // user agent aborting the running script, then set evaluationStatus to
    // Completion { [[Type]]: throw, [[Value]]: a new "QuotaExceededError"
    // DOMException, [[Target]]: empty }.</spec>
  }

  // [not specced] Store V8 code cache on successful evaluation.
  if (result.GetResultType() == ScriptEvaluationResult::ResultType::kSuccess) {
    DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
        TRACE_DISABLED_BY_DEFAULT("devtools.target-rundown"), "ModuleEvaluated",
        inspector_target_rundown_event::Data, execution_context, isolate,
        script_state, module_script->V8Module()->ScriptId());
    execution_context->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&Modulator::ProduceCacheModuleTreeTopLevel,
                          WrapWeakPersistent(Modulator::From(script_state)),
                          WrapWeakPersistent(module_script)));
  }

  if (!rethrow_errors.ShouldRethrow()) {
    // <spec step="7"> If report errors is true, then upon rejection of
    // evaluationPromise with reason, report the exception given by reason
    // for script.</spec>
    auto* callback_failure = MakeGarbageCollected<ScriptFunction>(
        script_state,
        MakeGarbageCollected<ModuleEvaluationRejectionCallback>());
    // Add a rejection handler to report back errors once the result
    // promise is rejected.
    result.GetPromise(script_state).Then(nullptr, callback_failure);
  }

  // <spec step="8">Clean up after running script with settings.</spec>
  // Partially implemented in MicrotaskScope destructor and the
  // v8::Context::Scope destructor.
  return result;
}

void V8ScriptRunner::ReportException(v8::Isolate* isolate,
                                     v8::Local<v8::Value> exception) {
  CHECK(!exception.IsEmpty());

  // https://html.spec.whatwg.org/C/#report-the-error
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (IsMainThread())
    V8Initializer::MessageHandlerInMainThread(message, exception);
  else
    V8Initializer::MessageHandlerInWorker(message, exception);
}

}  // namespace blink
