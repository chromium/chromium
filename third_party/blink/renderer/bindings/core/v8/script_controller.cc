/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {
bool IsTrivialScript(const String& script) {
  if (script.length() > 25) {
    return false;
  }

  DEFINE_STATIC_LOCAL(Vector<String>, trivial_scripts,
                      ({"void(0)",
                        "void0",
                        "void(false)",
                        "void(null)",
                        "void(-1)",
                        "false",
                        "true",
                        "",
                        "''",
                        "\"\"",
                        "undefined",
                        "0",
                        "1",
                        "'1'",
                        "print()",
                        "window.print()",
                        "close()",
                        "window.close()",
                        "history.back()",
                        "window.history.back()",
                        "history.go(-1)",
                        "window.history.go(-1)"}));
  String processed_script = script.StripWhiteSpace().Replace(";", "");
  return trivial_scripts.Contains(processed_script);
}

}  // namespace

void ScriptController::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(window_proxy_manager_);
}

LocalWindowProxy* ScriptController::WindowProxy(DOMWrapperWorld& world) {
  return window_proxy_manager_->WindowProxy(world);
}

void ScriptController::UpdateSecurityOrigin(
    const SecurityOrigin* security_origin) {
  window_proxy_manager_->UpdateSecurityOrigin(security_origin);
}

TextPosition ScriptController::EventHandlerPosition() const {
  ScriptableDocumentParser* parser =
      window_->document()->GetScriptableDocumentParser();
  if (parser)
    return parser->GetTextPosition();
  return TextPosition::MinimumPosition();
}

void ScriptController::DisableEval(const String& error_message) {
  SetEvalForWorld(DOMWrapperWorld::MainWorld(GetIsolate()),
                  false /* allow_eval */, error_message);
}

void ScriptController::SetWasmEvalErrorMessage(const String& error_message) {
  SetWasmEvalErrorMessageForWorld(DOMWrapperWorld::MainWorld(GetIsolate()),
                                  /*allow_eval=*/false, error_message);
}

void ScriptController::DisableEvalForIsolatedWorld(
    int32_t world_id,
    const String& error_message) {
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));
  DOMWrapperWorld* world =
      DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), world_id);
  SetEvalForWorld(*world, false /* allow_eval */, error_message);
}

void ScriptController::SetWasmEvalErrorMessageForIsolatedWorld(
    int32_t world_id,
    const String& error_message) {
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));
  DOMWrapperWorld* world =
      DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), world_id);
  SetWasmEvalErrorMessageForWorld(*world, /*allow_eval=*/false, error_message);
}

void ScriptController::SetEvalForWorld(DOMWrapperWorld& world,
                                       bool allow_eval,
                                       const String& error_message) {
  v8::HandleScope handle_scope(GetIsolate());
  LocalWindowProxy* proxy =
      world.IsMainWorld()
          ? window_proxy_manager_->MainWorldProxyMaybeUninitialized()
          : WindowProxy(world);

  v8::Local<v8::Context> v8_context = proxy->ContextIfInitialized();
  if (v8_context.IsEmpty())
    return;

  v8_context->AllowCodeGenerationFromStrings(allow_eval);
  if (allow_eval)
    return;

  v8_context->SetErrorMessageForCodeGenerationFromStrings(
      V8String(GetIsolate(), error_message));
}

void ScriptController::SetWasmEvalErrorMessageForWorld(
    DOMWrapperWorld& world,
    bool allow_eval,
    const String& error_message) {
  // For now we have nothing to do in case we want to enable wasm-eval.
  if (allow_eval)
    return;

  v8::HandleScope handle_scope(GetIsolate());
  LocalWindowProxy* proxy =
      world.IsMainWorld()
          ? window_proxy_manager_->MainWorldProxyMaybeUninitialized()
          : WindowProxy(world);

  v8::Local<v8::Context> v8_context = proxy->ContextIfInitialized();
  if (v8_context.IsEmpty())
    return;

  v8_context->SetErrorMessageForWasmCodeGeneration(
      V8String(GetIsolate(), error_message));
}

namespace {

Vector<const char*>& RegisteredExtensionNames() {
  DEFINE_STATIC_LOCAL(Vector<const char*>, extension_names, ());
  return extension_names;
}

}  // namespace

void ScriptController::RegisterExtensionIfNeeded(
    std::unique_ptr<v8::Extension> extension) {
  for (const auto* extension_name : RegisteredExtensionNames()) {
    if (!strcmp(extension_name, extension->name()))
      return;
  }
  RegisteredExtensionNames().push_back(extension->name());
  v8::RegisterExtension(std::move(extension));
}

v8::ExtensionConfiguration ScriptController::ExtensionsFor(
    const ExecutionContext* context) {
  if (context->ShouldInstallV8Extensions()) {
    return v8::ExtensionConfiguration(RegisteredExtensionNames().size(),
                                      RegisteredExtensionNames().data());
  }
  return v8::ExtensionConfiguration();
}

void ScriptController::UpdateDocument() {
  window_proxy_manager_->UpdateDocument();
}

void ScriptController::DiscardFrame() {
  DCHECK(window_->GetFrame());
  auto* previous_document_loader =
      window_->GetFrame()->Loader().GetDocumentLoader();
  DCHECK(previous_document_loader);
  auto params =
      previous_document_loader->CreateWebNavigationParamsToCloneDocument();
  WebNavigationParams::FillStaticResponse(params.get(), "text/html", "UTF-8",
                                          base::span<const char>());
  params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  window_->GetFrame()->Loader().CommitNavigation(std::move(params), nullptr,
                                                 CommitReason::kDiscard);
}

void ScriptController::ExecuteJavaScriptURL(
    const KURL& url,
    network::mojom::CSPDisposition csp_disposition,
    const DOMWrapperWorld* world_for_csp) {
  DCHECK(url.ProtocolIsJavaScript());

  if (!window_->GetFrame())
    return;

  bool had_navigation_before =
      window_->GetFrame()->Loader().HasProvisionalNavigation();

  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#javascript-protocol
  // Step 6. "Let baseURL be settings's API base URL." [spec text]
  const KURL base_url = window_->BaseURL();

  String script_source = window_->CheckAndGetJavascriptUrl(
      world_for_csp, url, nullptr /* element */, csp_disposition);

  // Step 7. "Let script be the result of creating a classic script given
  // scriptSource, settings, baseURL, and the default classic script fetch
  // options." [spec text]
  //
  // We pass |SanitizeScriptErrors::kDoNotSanitize| because |muted errors| is
  // false by default.
  ClassicScript* script = ClassicScript::Create(
      script_source, KURL(), base_url, ScriptFetchOptions(),
      ScriptSourceLocationType::kJavascriptUrl,
      SanitizeScriptErrors::kDoNotSanitize);

  DCHECK_EQ(&window_->GetScriptController(), this);
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> v8_result =
      script->RunScriptAndReturnValue(window_).GetSuccessValueOrEmpty();
  UseCounter::Count(window_.Get(), WebFeature::kExecutedJavaScriptURL);

  // CSPDisposition::CHECK indicate that the JS URL comes from a site (and not
  // from bookmarks or extensions). Empty v8_result indicate that the script
  // had a failure at the time of execution.
  if (csp_disposition == network::mojom::CSPDisposition::CHECK &&
      !v8_result.IsEmpty()) {
    if (!IsTrivialScript(script_source)) {
      UseCounter::Count(window_.Get(),
                        WebFeature::kExecutedNonTrivialJavaScriptURL);
    }
  }

  // If executing script caused this frame to be removed from the page, we
  // don't want to try to replace its document!
  if (!window_->GetFrame())
    return;
  // If a navigation begins during the javascript: url's execution, ignore
  // the return value of the script. Otherwise, replacing the document with a
  // string result would cancel the navigation.
  // TODO(crbug.com/1085514): Consider making HasProvisionalNavigation return
  // true when a form submission is pending instead of having a separate check
  // for form submissions here.
  if (!had_navigation_before &&
      (window_->GetFrame()->Loader().HasProvisionalNavigation() ||
       window_->GetFrame()->IsFormSubmissionPending())) {
    return;
  }
  if (v8_result.IsEmpty() || !v8_result->IsString())
    return;

  UseCounter::Count(window_.Get(),
                    WebFeature::kReplaceDocumentViaJavaScriptURL);

  auto* previous_document_loader =
      window_->GetFrame()->Loader().GetDocumentLoader();
  DCHECK(previous_document_loader);
  auto params =
      previous_document_loader->CreateWebNavigationParamsToCloneDocument();
  String result = ToCoreString(isolate, v8::Local<v8::String>::Cast(v8_result));
  WebNavigationParams::FillStaticResponse(
      params.get(), "text/html", "UTF-8",
      StringUTF8Adaptor(
          result, kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD));
  params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  window_->GetFrame()->Loader().CommitNavigation(std::move(params), nullptr,
                                                 CommitReason::kJavascriptUrl);
}

v8::Local<v8::Value> ScriptController::EvaluateMethodInMainWorld(
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> argv[]) {
  if (!CanExecuteScript(
          ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled)) {
    return v8::Local<v8::Value>();
  }

  // |script_state->GetContext()| should be initialized already due to the
  // WindowProxy() call inside ToScriptStateForMainWorld().
  ScriptState* script_state = ToScriptStateForMainWorld(window_->GetFrame());
  if (!script_state) {
    return v8::Local<v8::Value>();
  }
  DCHECK_EQ(script_state->GetIsolate(), GetIsolate());

  v8::Context::Scope scope(script_state->GetContext());
  v8::EscapableHandleScope handle_scope(GetIsolate());

  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);

  ExecutionContext* executionContext = ExecutionContext::From(script_state);

  v8::MaybeLocal<v8::Value> resultObj = V8ScriptRunner::CallFunction(
      function, executionContext, receiver, argc,
      static_cast<v8::Local<v8::Value>*>(argv), ToIsolate(window_->GetFrame()));

  if (resultObj.IsEmpty())
    return v8::Local<v8::Value>();

  return handle_scope.Escape(resultObj.ToLocalChecked());
}

bool ScriptController::CanExecuteScript(ExecuteScriptPolicy policy) {
  if (policy == ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled &&
      !window_->CanExecuteScripts(kAboutToExecuteScript))
    return false;

  if (window_->document()->IsInitialEmptyDocument())
    window_->GetFrame()->Loader().DidAccessInitialDocument();

  return true;
}

v8::Isolate* ScriptController::GetIsolate() const {
  return window_proxy_manager_->GetIsolate();
}

DOMWrapperWorld* ScriptController::CreateNewInspectorIsolatedWorld(
    const String& world_name) {
  DOMWrapperWorld* world = DOMWrapperWorld::Create(
      GetIsolate(), DOMWrapperWorld::WorldType::kInspectorIsolated);
  // Bail out if we could not create an isolated world.
  if (!world)
    return nullptr;
  if (!world_name.empty()) {
    DOMWrapperWorld::SetNonMainWorldHumanReadableName(world->GetWorldId(),
                                                      world_name);
  }
  // Make sure the execution context exists.
  WindowProxy(*world);
  return world;
}

}  // namespace blink
