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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/shadow_realm_context.h"
#include "third_party/blink/renderer/bindings/core/v8/use_counter_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_embedder_graph_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_error_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_trustedscript.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/shadow_realm/shadow_realm_global_scope.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/public/v8_snapshot_file_type.h"
#endif

namespace blink {

#if BUILDFLAG(IS_WIN)
// Defined in v8_initializer_win.cc.
bool FilterETWSessionByURLCallback(v8::Local<v8::Context> context,
                                   const std::string& json_payload);
#endif  // BUILDFLAG(IS_WIN)

namespace {

String ExtractMessageForConsole(v8::Isolate* isolate,
                                v8::Local<v8::Value> data) {
  DOMException* exception = V8DOMException::ToWrappable(isolate, data);
  return exception ? exception->ToStringForConsole() : String();
}

mojom::ConsoleMessageLevel MessageLevelFromNonFatalErrorLevel(int error_level) {
  mojom::ConsoleMessageLevel level = mojom::ConsoleMessageLevel::kError;
  switch (error_level) {
    case v8::Isolate::kMessageDebug:
      level = mojom::ConsoleMessageLevel::kVerbose;
      break;
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
      level = mojom::ConsoleMessageLevel::kInfo;
      break;
    case v8::Isolate::kMessageWarning:
      level = mojom::ConsoleMessageLevel::kWarning;
      break;
    case v8::Isolate::kMessageError:
      level = mojom::ConsoleMessageLevel::kInfo;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return level;
}

// NOTE: when editing this, please also edit the error messages we throw when
// the size is exceeded (see uses of the constant), which use the human-friendly
// "8MB" text.
const size_t kWasmWireBytesLimit = 1 << 23;

}  // namespace

void V8Initializer::MessageHandlerInMainThread(v8::Local<v8::Message> message,
                                               v8::Local<v8::Value> data) {
  DCHECK(IsMainThread());
  v8::Isolate* isolate = message->GetIsolate();

  if (isolate->GetEnteredOrMicrotaskContext().IsEmpty())
    return;

  // If called during context initialization, there will be no entered context.
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  if (!script_state->ContextIsValid())
    return;

  ExecutionContext* context = ExecutionContext::From(script_state);

  UseCounter::Count(context, WebFeature::kUnhandledExceptionCountInMainThread);
  base::UmaHistogramBoolean("V8.UnhandledExceptionCountInMainThread", true);
  // TODO(b/338241225): Reenable the
  // ThirdPartyCookies.BreakageIndicator.UncaughtJSError event with logic that
  // caps the number of times the event can be sent per client.

  std::unique_ptr<SourceLocation> location =
      CaptureSourceLocation(isolate, message, context);

  if (message->ErrorLevel() != v8::Isolate::kMessageError) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(isolate, message->Get()),
        std::move(location)));
    return;
  }

  const auto sanitize_script_errors = message->IsSharedCrossOrigin()
                                          ? SanitizeScriptErrors::kDoNotSanitize
                                          : SanitizeScriptErrors::kSanitize;

  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(isolate, message->Get()), std::move(location),
      ScriptValue(isolate, data), &script_state->World());

  String message_for_console = ExtractMessageForConsole(isolate, data);
  if (!message_for_console.empty())
    event->SetUnsanitizedMessage(message_for_console);

  context->DispatchErrorEvent(event, sanitize_script_errors);
}

void V8Initializer::MessageHandlerInWorker(v8::Local<v8::Message> message,
                                           v8::Local<v8::Value> data) {
  v8::Isolate* isolate = message->GetIsolate();
  // During the frame teardown, there may not be a valid context.
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  if (!script_state->ContextIsValid())
    return;

  ExecutionContext* context = ExecutionContext::From(script_state);
  CHECK(context);

  UseCounter::Count(context, WebFeature::kUnhandledExceptionCountInWorker);
  base::UmaHistogramBoolean("V8.UnhandledExceptionCountInWorker", true);

  std::unique_ptr<SourceLocation> location =
      CaptureSourceLocation(isolate, message, context);

  if (message->ErrorLevel() != v8::Isolate::kMessageError) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(isolate, message->Get()),
        std::move(location)));
    return;
  }

  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(isolate, message->Get()), std::move(location),
      ScriptValue(isolate, data), &script_state->World());

  const auto sanitize_script_errors = message->IsSharedCrossOrigin()
                                          ? SanitizeScriptErrors::kDoNotSanitize
                                          : SanitizeScriptErrors::kSanitize;

  // If execution termination has been triggered as part of constructing
  // the error event from the v8::Message, quietly leave.
  if (!isolate->IsExecutionTerminating()) {
    ExecutionContext::From(script_state)
        ->DispatchErrorEvent(event, sanitize_script_errors);
  }
}

static void PromiseRejectHandler(v8::PromiseRejectMessage data,
                                 RejectedPromises& rejected_promises,
                                 ScriptState* script_state) {
  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    rejected_promises.HandlerAdded(data);
    return;
  } else if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
             data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    // Ignore reject/resolve after resolved.
    return;
  }

  DCHECK_EQ(v8::kPromiseRejectWithNoHandler, data.GetEvent());

  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* context = ExecutionContext::From(script_state);

  v8::Local<v8::Value> exception = data.GetValue();
  String error_message;
  SanitizeScriptErrors sanitize_script_errors = SanitizeScriptErrors::kSanitize;
  std::unique_ptr<SourceLocation> location;

  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (!message.IsEmpty()) {
    // message->Get() can be empty here. https://crbug.com/450330
    error_message = ToCoreStringWithNullCheck(isolate, message->Get());
    location = CaptureSourceLocation(isolate, message, context);
    if (message->IsSharedCrossOrigin())
      sanitize_script_errors = SanitizeScriptErrors::kDoNotSanitize;
  } else {
    location = std::make_unique<SourceLocation>(context->Url().GetString(),
                                                String(), 0, 0, nullptr);
  }

  String message_for_console =
      ExtractMessageForConsole(isolate, data.GetValue());
  if (!message_for_console.empty()) {
    error_message = std::move(message_for_console);
  }

  rejected_promises.RejectedWithNoHandler(script_state, data, error_message,
                                          std::move(location),
                                          sanitize_script_errors);
}

// static
void V8Initializer::PromiseRejectHandlerInMainThread(
    v8::PromiseRejectMessage data) {
  DCHECK(IsMainThread());

  v8::Local<v8::Promise> promise = data.GetPromise();

  v8::Isolate* isolate = promise->GetIsolate();

  // TODO(ikilpatrick): Remove this check, extensions tests that use
  // extensions::ModuleSystemTest incorrectly don't have a valid script state.
  LocalDOMWindow* window = CurrentDOMWindow(isolate);
  if (!window || !window->IsCurrentlyDisplayedInFrame())
    return;

  // Bail out if called during context initialization.
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  if (!script_state->ContextIsValid())
    return;

  RejectedPromises* rejected_promises =
      &window->GetAgent()->GetRejectedPromises();
  PromiseRejectHandler(data, *rejected_promises, script_state);
}

void V8Initializer::ExceptionPropagationCallback(
    v8::ExceptionPropagationMessage v8_message) {
  v8::Isolate* isolate = v8_message.GetIsolate();
  v8::Local<v8::Object> exception = v8_message.GetException();

  v8::ExceptionContext context_type = v8_message.GetExceptionContext();
  String class_name = ToCoreString(isolate, v8_message.GetInterfaceName());
  if (class_name == "global") {
    class_name = "Window";
  }
  String property_name = ToCoreString(isolate, v8_message.GetPropertyName());
  if ((context_type == v8::ExceptionContext::kAttributeGet &&
       property_name.StartsWith("get ")) ||
      (context_type == v8::ExceptionContext::kAttributeSet &&
       property_name.StartsWith("set "))) {
    property_name = property_name.Substring(4);
  }
  if (property_name == "[Symbol.toPrimitive]") {
    property_name = String();
  }
  if (context_type == v8::ExceptionContext::kConstructor) {
    // Constructors are reported by v8 as the property name, but
    // our plumbing expects it as the class name.
    class_name = property_name;
  }
  DCHECK(class_name.Is8Bit());

  ApplyContextToException(
      isolate, isolate->GetCurrentContext(), exception,
      ExceptionContext(context_type, class_name.Utf8().data(), property_name));
}

static void PromiseRejectHandlerInWorker(v8::PromiseRejectMessage data) {
  v8::Local<v8::Promise> promise = data.GetPromise();

  // Bail out if called during context initialization.
  v8::Isolate* isolate = promise->GetIsolate();
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  if (!script_state->ContextIsValid())
    return;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return;

  ExecutionContext* root_worker_context =
      execution_context->IsShadowRealmGlobalScope()
          ? To<ShadowRealmGlobalScope>(execution_context)
                ->GetRootInitiatorExecutionContext()
          : execution_context;
  DCHECK(root_worker_context->IsWorkerOrWorkletGlobalScope());

  auto* script_controller =
      To<WorkerOrWorkletGlobalScope>(root_worker_context)->ScriptController();
  DCHECK(script_controller);

  PromiseRejectHandler(data, *script_controller->GetRejectedPromises(),
                       script_state);
}

// static
void V8Initializer::FailedAccessCheckCallbackInMainThread(
    v8::Local<v8::Object> holder,
    v8::AccessType type,
    v8::Local<v8::Value> data) {
  // FIXME: This is the access check callback of last resort. We should modify
  // V8 to pass in more contextual information, so that we can build a full
  // ExceptionState.
  ExceptionState exception_state(
      holder->GetIsolate(), v8::ExceptionContext::kUnknown, nullptr, nullptr);
  BindingSecurity::FailedAccessCheckFor(holder->GetIsolate(),
                                        WrapperTypeInfo::Unwrap(data), holder,
                                        exception_state);
}

// Check whether Content Security Policy allows script execution.
static bool ContentSecurityPolicyCodeGenerationCheck(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> source) {
  if (ExecutionContext* execution_context = ToExecutionContext(context)) {
    // Note this callback is only triggered for contexts which have eval
    // disabled. Hence we don't need to handle the case of isolated world
    // contexts with no CSP specified. (They should be exempt from the page CSP.
    // See crbug.com/982388.)

    if (ContentSecurityPolicy* policy =
            execution_context->GetContentSecurityPolicyForCurrentWorld()) {
      v8::Context::Scope scope(context);
      v8::String::Value source_str(context->GetIsolate(), source);
      UChar snippet[ContentSecurityPolicy::kMaxSampleLength + 1];
      size_t len = std::min((sizeof(snippet) / sizeof(UChar)) - 1,
                            static_cast<size_t>(source_str.length()));
      memcpy(snippet, *source_str, len * sizeof(UChar));
      snippet[len] = 0;
      return policy->AllowEval(ReportingDisposition::kReport,
                               ContentSecurityPolicy::kWillThrowException,
                               snippet);
    }
  }
  return false;
}

static std::pair<bool, v8::MaybeLocal<v8::String>>
TrustedTypesCodeGenerationCheck(v8::Local<v8::Context> context,
                                v8::Local<v8::Value> source,
                                bool is_code_like) {
  v8::Isolate* isolate = context->GetIsolate();
  // If the input is not a string or TrustedScript, pass it through.
  if (!source->IsString() && !is_code_like &&
      !V8TrustedScript::HasInstance(isolate, source)) {
    return {true, v8::MaybeLocal<v8::String>()};
  }

  v8::TryCatch try_catch(isolate);
  V8UnionStringOrTrustedScript* string_or_trusted_script =
      NativeValueTraits<V8UnionStringOrTrustedScript>::NativeValue(
          isolate, source, PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    // The input was a string or TrustedScript but the conversion failed.
    // Block, just in case.
    return {false, v8::MaybeLocal<v8::String>()};
  }

  if (is_code_like && string_or_trusted_script->IsString()) {
    string_or_trusted_script->Set(MakeGarbageCollected<TrustedScript>(
        string_or_trusted_script->GetAsString()));
  }

  String stringified_source = TrustedTypesCheckForScript(
      string_or_trusted_script, ToExecutionContext(context), "eval", "",
      PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    return {false, v8::MaybeLocal<v8::String>()};
  }

  return {true, V8String(context->GetIsolate(), stringified_source)};
}

// static
v8::ModifyCodeGenerationFromStringsResult
V8Initializer::CodeGenerationCheckCallbackInMainThread(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> source,
    bool is_code_like) {
  // The TC39 "Dynamic Code Brand Check" feature is currently behind a flag.
  if (!RuntimeEnabledFeatures::TrustedTypesUseCodeLikeEnabled())
    is_code_like = false;

  // With Trusted Types, we always run the TT check first because of reporting,
  // and because a default policy might want to stringify or modify the original
  // source. When TT enforcement is disabled, codegen is always allowed, and we
  // just use the check to stringify any trusted type source.
  bool codegen_allowed_by_tt = false;
  v8::MaybeLocal<v8::String> stringified_source;
  std::tie(codegen_allowed_by_tt, stringified_source) =
      TrustedTypesCodeGenerationCheck(context, source, is_code_like);

  if (!codegen_allowed_by_tt) {
    return {false, v8::MaybeLocal<v8::String>()};
  }

  if (stringified_source.IsEmpty()) {
    return {true, v8::MaybeLocal<v8::String>()};
  }

  if (!ContentSecurityPolicyCodeGenerationCheck(
          context, stringified_source.ToLocalChecked())) {
    return {false, v8::MaybeLocal<v8::String>()};
  }

  return {true, std::move(stringified_source)};
}

bool V8Initializer::WasmCodeGenerationCheckCallbackInMainThread(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> source) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context) {
    return false;
  }
  ContentSecurityPolicy* policy = execution_context->GetContentSecurityPolicy();
  if (!policy) {
    return false;
  }
  v8::String::Value source_str(context->GetIsolate(), source);
  UChar snippet[ContentSecurityPolicy::kMaxSampleLength + 1];
  size_t len = std::min((sizeof(snippet) / sizeof(UChar)) - 1,
                        static_cast<size_t>(source_str.length()));
  memcpy(snippet, *source_str, len * sizeof(UChar));
  snippet[len] = 0;
  if (!policy->AllowWasmCodeGeneration(
          ReportingDisposition::kReport,
          ContentSecurityPolicy::kWillThrowException, snippet)) {
    return false;
  }

  // Set a crash key so we know if a crash report could have been caused by
  // Wasm.
  static crash_reporter::CrashKeyString<1> has_wasm_key("has-wasm");
  has_wasm_key.Set("1");
  return true;
}

void V8Initializer::WasmAsyncResolvePromiseCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Promise::Resolver> resolver,
    v8::Local<v8::Value> compilation_result,
    v8::WasmAsyncSuccess success) {
  ScriptState* script_state = ScriptState::MaybeFrom(isolate, context);
  if (!script_state ||
      !IsInParallelAlgorithmRunnable(ExecutionContext::From(script_state),
                                     script_state)) {
    return;
  }
  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  if (success == v8::WasmAsyncSuccess::kSuccess) {
    CHECK(resolver->Resolve(context, compilation_result).FromJust());
  } else {
    CHECK(resolver->Reject(context, compilation_result).FromJust());
  }
}

namespace {
bool SharedArrayBufferConstructorEnabledCallback(
    v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context)
    return false;
  return execution_context->SharedArrayBufferTransferAllowed();
}

v8::Local<v8::Value> NewRangeException(v8::Isolate* isolate,
                                       const char* message) {
  return v8::Exception::RangeError(
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>(message),
                                 v8::NewStringType::kNormal)
          .ToLocalChecked());
}

void ThrowRangeException(v8::Isolate* isolate, const char* message) {
  isolate->ThrowException(NewRangeException(isolate, message));
}

BASE_FEATURE(kWebAssemblyUnlimitedSyncCompilation,
             "WebAssemblyUnlimitedSyncCompilation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool WasmModuleOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Return false if we want the base behavior to proceed.
  if (!WTF::IsMainThread() || args.Length() < 1 ||
      base::FeatureList::IsEnabled(kWebAssemblyUnlimitedSyncCompilation)) {
    return false;
  }
  v8::Local<v8::Value> source = args[0];
  if ((source->IsArrayBuffer() &&
       v8::Local<v8::ArrayBuffer>::Cast(source)->ByteLength() >
           kWasmWireBytesLimit) ||
      (source->IsArrayBufferView() &&
       v8::Local<v8::ArrayBufferView>::Cast(source)->ByteLength() >
           kWasmWireBytesLimit)) {
    ThrowRangeException(
        args.GetIsolate(),
        "WebAssembly.Compile is disallowed on the main thread, "
        "if the buffer size is larger than 8MB. Use "
        "WebAssembly.compile, compile on a worker thread, or use the flag "
        "`--enable-features=WebAssemblyUnlimitedSyncCompilation`.");
    // Return true because we injected new behavior and we do not
    // want the default behavior.
    return true;
  }
  return false;
}

bool WasmInstanceOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Return false if we want the base behavior to proceed.
  if (!WTF::IsMainThread() || args.Length() < 1 ||
      base::FeatureList::IsEnabled(kWebAssemblyUnlimitedSyncCompilation)) {
    return false;
  }
  v8::Local<v8::Value> source = args[0];
  if (!source->IsWasmModuleObject())
    return false;

  v8::CompiledWasmModule compiled_module =
      v8::Local<v8::WasmModuleObject>::Cast(source)->GetCompiledModule();
  if (compiled_module.GetWireBytesRef().size() > kWasmWireBytesLimit) {
    ThrowRangeException(
        args.GetIsolate(),
        "WebAssembly.Instance is disallowed on the main thread, "
        "if the buffer size is larger than 8MB. Use "
        "WebAssembly.instantiate, or use the flag "
        "`--enable-features=WebAssemblyUnlimitedSyncCompilation`.");
    return true;
  }
  return false;
}

bool WasmJSStringBuiltinsEnabledCallback(v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context) {
    return false;
  }
  return RuntimeEnabledFeatures::WebAssemblyJSStringBuiltinsEnabled(
      execution_context);
}

bool WasmJSPromiseIntegrationEnabledCallback(v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context) {
    return false;
  }
  return RuntimeEnabledFeatures::WebAssemblyJSPromiseIntegrationEnabled(
      execution_context);
}

v8::MaybeLocal<v8::Promise> HostImportModuleDynamically(
    v8::Local<v8::Context> context,
    v8::Local<v8::Data> v8_host_defined_options,
    v8::Local<v8::Value> v8_referrer_resource_url,
    v8::Local<v8::String> v8_specifier,
    v8::Local<v8::FixedArray> v8_import_attributes) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);

  Modulator* modulator = Modulator::From(script_state);
  if (!modulator) {
    // Inactive browsing context (detached frames) doesn't have a modulator.
    // We chose to return a rejected promise (which may never get to catch(),
    // since MicrotaskQueue for a detached frame is never consumed).
    //
    // This is a hack to satisfy V8 API expectation, which are:
    // - return non-empty v8::Promise value
    //   (can either be fulfilled/rejected), or
    // - throw exception && return Empty value
    // See crbug.com/972960 .
    //
    // We use the v8 promise API directly here.
    // We can't use ScriptPromiseResolverBase here since it assumes a valid
    // ScriptState.
    v8::Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(script_state->GetContext())
             .ToLocal(&resolver)) {
      // Note: V8 should have thrown an exception in this case,
      //       so we return Empty.
      return v8::MaybeLocal<v8::Promise>();
    }

    v8::Local<v8::Promise> promise = resolver->GetPromise();
    v8::Local<v8::Value> error = V8ThrowException::CreateError(
        script_state->GetIsolate(),
        "Cannot import module from an inactive browsing context.");
    resolver->Reject(script_state->GetContext(), error).ToChecked();
    return promise;
  }

  String specifier =
      ToCoreStringWithNullCheck(script_state->GetIsolate(), v8_specifier);
  KURL referrer_resource_url;
  if (v8_referrer_resource_url->IsString()) {
    String referrer_resource_url_str =
        ToCoreString(script_state->GetIsolate(),
                     v8::Local<v8::String>::Cast(v8_referrer_resource_url));
    if (!referrer_resource_url_str.empty())
      referrer_resource_url = KURL(NullURL(), referrer_resource_url_str);
  }

  ModuleRequest module_request(
      specifier, TextPosition::MinimumPosition(),
      ModuleRecord::ToBlinkImportAttributes(
          script_state->GetContext(), v8::Local<v8::Module>(),
          v8_import_attributes, /*v8_import_attributes_has_positions=*/false));

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(
      script_state,
      ExceptionContext(v8::ExceptionContext::kUnknown, "", "import"));

  String invalid_attribute_key;
  if (module_request.HasInvalidImportAttributeKey(&invalid_attribute_key)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "Invalid attribute key \"" + invalid_attribute_key + "\"."));
  } else {
    ReferrerScriptInfo referrer_info =
        ReferrerScriptInfo::FromV8HostDefinedOptions(
            context, v8_host_defined_options, referrer_resource_url);

    modulator->ResolveDynamically(module_request, referrer_info, resolver);
  }

  return resolver->Promise().V8Promise();
}

// https://html.spec.whatwg.org/C/#hostgetimportmetaproperties
void HostGetImportMetaProperties(v8::Local<v8::Context> context,
                                 v8::Local<v8::Module> module,
                                 v8::Local<v8::Object> meta) {
  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  v8::HandleScope handle_scope(isolate);

  Modulator* modulator = Modulator::From(script_state);
  if (!modulator)
    return;

  ModuleImportMeta host_meta = modulator->HostGetImportMetaProperties(module);

  // 6. Return « Record { [[Key]]: "url", [[Value]]: urlString }, Record {
  // [[Key]]: "resolve", [[Value]]: resolveFunction } ». [spec text]
  v8::Local<v8::String> url_key = V8String(isolate, "url");
  v8::Local<v8::String> url_value = V8String(isolate, host_meta.Url());

  v8::Local<v8::String> resolve_key = V8String(isolate, "resolve");
  v8::Local<v8::Function> resolve_value =
      host_meta.MakeResolveV8Function(modulator);
  resolve_value->SetName(resolve_key);

  meta->CreateDataProperty(context, url_key, url_value).ToChecked();
  meta->CreateDataProperty(context, resolve_key, resolve_value).ToChecked();
}

struct PrintV8OOM {
  const char* location;
  const v8::OOMDetails& details;
};

std::ostream& operator<<(std::ostream& os, const PrintV8OOM& oom_details) {
  const auto [location, details] = oom_details;
  os << "V8 " << (details.is_heap_oom ? "javascript" : "process") << " OOM ("
     << location;
  if (details.detail) {
    os << "; detail: " << details.detail;
  }
  os << ").";
  return os;
}

}  // namespace

// static
void V8Initializer::InitializeV8Common(v8::Isolate* isolate) {
  // Set up garbage collection before setting up anything else as V8 may trigger
  // GCs during Blink setup.
  V8PerIsolateData::From(isolate)->SetGCCallbacks(
      isolate, V8GCController::GcPrologue, V8GCController::GcEpilogue);
  ThreadState::Current()->AttachToIsolate(
      isolate, EmbedderGraphBuilder::BuildEmbedderGraphCallback);
  V8PerIsolateData::From(isolate)->SetActiveScriptWrappableManager(
      MakeGarbageCollected<ActiveScriptWrappableManager>());

  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
  isolate->SetUseCounterCallback(&UseCounterCallback);
  isolate->SetWasmModuleCallback(WasmModuleOverride);
  isolate->SetWasmInstanceCallback(WasmInstanceOverride);
  isolate->SetWasmImportedStringsEnabledCallback(
      WasmJSStringBuiltinsEnabledCallback);
  isolate->SetWasmJSPIEnabledCallback(WasmJSPromiseIntegrationEnabledCallback);
  isolate->SetSharedArrayBufferConstructorEnabledCallback(
      SharedArrayBufferConstructorEnabledCallback);
  isolate->SetHostImportModuleDynamicallyCallback(HostImportModuleDynamically);
  isolate->SetHostInitializeImportMetaObjectCallback(
      HostGetImportMetaProperties);
  isolate->SetMetricsRecorder(std::make_shared<V8MetricsRecorder>(isolate));

#if BUILDFLAG(IS_WIN)
  isolate->SetFilterETWSessionByURLCallback(FilterETWSessionByURLCallback);
#endif  // BUILDFLAG(IS_WIN)

  V8ContextSnapshot::EnsureInterfaceTemplates(isolate);

  WasmResponseExtensions::Initialize(isolate);

  if (v8::HeapProfiler* profiler = isolate->GetHeapProfiler()) {
    profiler->SetGetDetachednessCallback(
        V8GCController::DetachednessFromWrapper, nullptr);
  }
}

// Callback functions called when V8 encounters a fatal or OOM error.
// Keep them outside the anonymous namespace such that ChromeCrash recognizes
// them.
void ReportV8FatalError(const char* location, const char* message) {
  LOG(FATAL) << "V8 error: " << message << " (" << location << ").";
}

void ReportV8OOMError(const char* location, const v8::OOMDetails& details) {
  if (location) {
    static crash_reporter::CrashKeyString<64> location_key("v8-oom-location");
    location_key.Set(location);
  }

  if (details.detail) {
    static crash_reporter::CrashKeyString<128> detail_key("v8-oom-detail");
    detail_key.Set(details.detail);
  }

  LOG(ERROR) << PrintV8OOM{location, details};
  OOM_CRASH(0);
}

namespace {
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  ArrayBufferAllocator() : total_allocation_(0) {
    // size_t may be equivalent to uint32_t or uint64_t, cast all values to
    // uint64_t to compare.
    uint64_t virtual_size = base::SysInfo::AmountOfVirtualMemory();
    uint64_t size_t_max = std::numeric_limits<std::size_t>::max();
    DCHECK(virtual_size < size_t_max);
    // If AmountOfVirtualMemory() returns 0, there is no limit on virtual
    // memory, do not limit the total allocation. Otherwise, Limit the total
    // allocation to reserve up to 2 GiB virtual memory space for other
    // components.
    uint64_t memory_reserve = 2ull * 1024 * 1024 * 1024;  // 2 GiB
    if (virtual_size > memory_reserve * 2) {
      max_allocation_ = static_cast<size_t>(virtual_size - memory_reserve);
    } else {
      max_allocation_ = static_cast<size_t>(virtual_size / 2);
    }
  }

  // Allocate() methods return null to signal allocation failure to V8, which
  // should respond by throwing a RangeError, per
  // http://www.ecma-international.org/ecma-262/6.0/#sec-createbytedatablock.
  void* Allocate(size_t size) override {
    if (max_allocation_ != 0 &&
        std::atomic_load(&total_allocation_) > max_allocation_ - size)
      return nullptr;
    void* result = ArrayBufferContents::AllocateMemoryOrNull(
        size, ArrayBufferContents::kZeroInitialize);
    if (max_allocation_ != 0 && result)
      total_allocation_.fetch_add(size, std::memory_order_relaxed);
    return result;
  }

  void* AllocateUninitialized(size_t size) override {
    if (max_allocation_ != 0 &&
        std::atomic_load(&total_allocation_) > max_allocation_ - size)
      return nullptr;
    void* result = ArrayBufferContents::AllocateMemoryOrNull(
        size, ArrayBufferContents::kDontInitialize);
    if (max_allocation_ != 0 && result)
      total_allocation_.fetch_add(size, std::memory_order_relaxed);
    return result;
  }

  void Free(void* data, size_t size) override {
    if (max_allocation_ != 0 && data)
      total_allocation_.fetch_sub(size, std::memory_order_relaxed);
    ArrayBufferContents::FreeMemory(data);
  }

 private:
  // Total memory allocated in bytes.
  std::atomic_size_t total_allocation_;
  // If |max_allocation_| is 0, skip these atomic operations on
  // |total_allocation_|.
  size_t max_allocation_;
};

V8PerIsolateData::V8ContextSnapshotMode GetV8ContextSnapshotMode() {
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  if (Platform::Current()->IsTakingV8ContextSnapshot())
    return V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot;
  if (gin::GetLoadedSnapshotFileType() ==
      gin::V8SnapshotFileType::kWithAdditionalContext) {
    return V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot;
  }
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  return V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot;
}

}  // namespace

void V8Initializer::InitializeIsolateHolder(
    const intptr_t* reference_table,
    const std::string& js_command_line_flags) {
  DEFINE_STATIC_LOCAL(ArrayBufferAllocator, array_buffer_allocator, ());
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 &array_buffer_allocator, reference_table,
                                 js_command_line_flags, ReportV8FatalError,
                                 ReportV8OOMError);
}

v8::Isolate* V8Initializer::InitializeMainThread() {
  DCHECK(IsMainThread());
  ThreadScheduler* scheduler = ThreadScheduler::Current();

  V8PerIsolateData::V8ContextSnapshotMode snapshot_mode =
      GetV8ContextSnapshotMode();
  v8::CreateHistogramCallback create_histogram_callback = nullptr;
  v8::AddHistogramSampleCallback add_histogram_sample_callback = nullptr;
  // We don't log histograms when taking a snapshot.
  if (snapshot_mode != V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot) {
    create_histogram_callback = CreateHistogram;
    add_histogram_sample_callback = AddHistogramSample;
  }
  v8::Isolate* isolate = V8PerIsolateData::Initialize(
      scheduler->V8TaskRunner(), scheduler->V8UserVisibleTaskRunner(),
      scheduler->V8BestEffortTaskRunner(), snapshot_mode,
      create_histogram_callback, add_histogram_sample_callback);
  scheduler->SetV8Isolate(isolate);

  // ThreadState::isolate_ needs to be set before setting the EmbedderHeapTracer
  // as setting the tracer indicates that a V8 garbage collection should trace
  // over to Blink.
  DCHECK(ThreadStateStorage::MainThreadStateStorage());

  InitializeV8Common(isolate);

  isolate->AddMessageListenerWithErrorLevel(
      MessageHandlerInMainThread,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);
  isolate->SetFailedAccessCheckCallbackFunction(
      V8Initializer::FailedAccessCheckCallbackInMainThread);
  isolate->SetModifyCodeGenerationFromStringsCallback(
      CodeGenerationCheckCallbackInMainThread);
  isolate->SetAllowWasmCodeGenerationCallback(
      WasmCodeGenerationCheckCallbackInMainThread);
  isolate->SetWasmAsyncResolvePromiseCallback(WasmAsyncResolvePromiseCallback);
  if (RuntimeEnabledFeatures::V8IdleTasksEnabled()) {
    V8PerIsolateData::EnableIdleTasks(
        isolate, std::make_unique<V8IdleTaskRunner>(scheduler));
  }

  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInMainThread);
  isolate->SetExceptionPropagationCallback(ExceptionPropagationCallback);

  V8PerIsolateData::From(isolate)->SetThreadDebugger(
      std::make_unique<MainThreadDebugger>(isolate));

  isolate->SetHostCreateShadowRealmContextCallback(
      OnCreateShadowRealmV8Context);

  if (Platform::Current()->IsolateStartsInBackground()) {
    // If we do not track widget visibility, then assume conservatively that
    // the isolate is in background. This reduces memory usage.
    isolate->SetPriority(v8::Isolate::Priority::kBestEffort);
  }

  return isolate;
}

// Stack size for workers is limited to 500KB because default stack size for
// secondary threads is 512KB on macOS. See GetDefaultThreadStackSize() in
// base/threading/platform_thread_apple.mm for details.
//
// For 32-bit Windows, the stack region always starts with an odd number of
// reserved pages, followed by two guard pages, followed by the committed
// memory for the stack, and the worker stack size need to be reduced
// (https://crbug.com/1412239).
#if defined(ARCH_CPU_32_BITS) && BUILDFLAG(IS_WIN)
static const int kWorkerMaxStackSize = 492 * 1024;
#else
static const int kWorkerMaxStackSize = 500 * 1024;
#endif

void V8Initializer::InitializeWorker(v8::Isolate* isolate) {
  InitializeV8Common(isolate);

  isolate->AddMessageListenerWithErrorLevel(
      MessageHandlerInWorker,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);

  isolate->SetStackLimit(WTF::GetCurrentStackPosition() - kWorkerMaxStackSize);
  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInWorker);
  isolate->SetExceptionPropagationCallback(ExceptionPropagationCallback);
  isolate->SetModifyCodeGenerationFromStringsCallback(
      CodeGenerationCheckCallbackInMainThread);
  isolate->SetAllowWasmCodeGenerationCallback(
      WasmCodeGenerationCheckCallbackInMainThread);
  isolate->SetWasmAsyncResolvePromiseCallback(WasmAsyncResolvePromiseCallback);
  isolate->SetHostCreateShadowRealmContextCallback(
      OnCreateShadowRealmV8Context);
}

}  // namespace blink
