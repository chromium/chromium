/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"

#include <tuple>

#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/single_sample_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_page_popup_controller_binding.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/document_name_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "v8/include/v8.h"

namespace blink {

void LocalWindowProxy::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  WindowProxy::Trace(visitor);
}

void LocalWindowProxy::DisposeContext(Lifecycle next_status,
                                      FrameReuseStatus frame_reuse_status) {
  DCHECK(next_status == Lifecycle::kV8MemoryIsForciblyPurged ||
         next_status == Lifecycle::kGlobalObjectIsDetached ||
         next_status == Lifecycle::kFrameIsDetached ||
         next_status == Lifecycle::kFrameIsDetachedAndV8MemoryIsPurged);

  // If the current lifecycle is kV8MemoryIsForciblyPurged, next status should
  // be either kFrameIsDetachedAndV8MemoryIsPurged, or kGlobalObjectIsDetached.
  // If the former, |global_proxy_| should become weak, and if the latter, the
  // necessary operations are already done so can return here.
  if (lifecycle_ == Lifecycle::kV8MemoryIsForciblyPurged) {
    DCHECK(next_status == Lifecycle::kGlobalObjectIsDetached ||
           next_status == Lifecycle::kFrameIsDetachedAndV8MemoryIsPurged);
    lifecycle_ = next_status;
    return;
  }

  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Context> context = script_state_->GetContext();
  // The embedder could run arbitrary code in response to the
  // willReleaseScriptContext callback, so all disposing should happen after
  // it returns.
  GetFrame()->Client()->WillReleaseScriptContext(context, world_->GetWorldId());
  CHECK_EQ(GetIsolate(), script_state_->GetIsolate());
  MainThreadDebugger::Instance(GetIsolate())
      ->ContextWillBeDestroyed(script_state_);
  if (next_status == Lifecycle::kV8MemoryIsForciblyPurged ||
      next_status == Lifecycle::kGlobalObjectIsDetached) {
    // Clean up state on the global proxy, which will be reused.
    v8::Local<v8::Object> global = context->Global();
    if (!global_proxy_.IsEmpty()) {
      CHECK(global_proxy_ == global);
      CHECK_EQ(ToScriptWrappable<DOMWindow>(GetIsolate(), global),
               ToScriptWrappable<DOMWindow>(
                   GetIsolate(), global->GetPrototype().As<v8::Object>()));
    }
    auto* window = GetFrame()->DomWindow();
    V8DOMWrapper::ClearNativeInfo(GetIsolate(), global,
                                  V8Window::GetWrapperTypeInfo());
    script_state_->World().DomDataStore().ClearIfEqualTo(window, global);
#if DCHECK_IS_ON()
    HeapVector<Member<DOMWrapperWorld>> all_worlds;
    DOMWrapperWorld::AllWorldsInIsolate(script_state_->GetIsolate(),
                                        all_worlds);
    for (auto& world : all_worlds) {
      DCHECK(!world->DomDataStore().EqualTo(window, global));
    }
#endif  // DCHECK_IS_ON()
    script_state_->DetachGlobalObject();
#if DCHECK_IS_ON()
    DidDetachGlobalObject();
#endif
  }

  script_state_->DisposePerContextData();
  // It's likely that disposing the context has created a lot of
  // garbage. Notify V8 about this so it'll have a chance of cleaning
  // it up when idle.
  V8GCForContextDispose::Instance().NotifyContextDisposed(
      script_state_->GetIsolate(), GetFrame()->IsMainFrame(),
      frame_reuse_status);

  DCHECK_EQ(lifecycle_, Lifecycle::kContextIsInitialized);
  lifecycle_ = next_status;
}

void LocalWindowProxy::Initialize() {
  TRACE_EVENT2("v8", "LocalWindowProxy::Initialize", "IsMainFrame",
               GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());
  CHECK(!GetFrame()->IsProvisional());
  base::ElapsedTimer timer;

  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::HandleScope handle_scope(GetIsolate());

  CreateContext();

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Context> context = script_state_->GetContext();
  if (global_proxy_.IsEmpty()) {
    global_proxy_.Reset(GetIsolate(), context->Global());
    CHECK(!global_proxy_.IsEmpty());
  }

  SetupWindowPrototypeChain();

  // Setup handling for eval checks for the context. Isolated worlds which don't
  // specify their own CSPs are exempt from eval checks currently.
  // TODO(crbug.com/982388): For other CSP checks, we use the main world CSP
  // when an isolated world doesn't specify its own CSP. We should do the same
  // here.
  const bool evaluate_csp_for_eval =
      world_->IsMainWorld() ||
      (world_->IsIsolatedWorld() &&
       IsolatedWorldCSP::Get().HasContentSecurityPolicy(world_->GetWorldId()));
  if (evaluate_csp_for_eval) {
    ContentSecurityPolicy* csp =
        GetFrame()->DomWindow()->GetContentSecurityPolicyForCurrentWorld();
    context->AllowCodeGenerationFromStrings(!csp->ShouldCheckEval());
    context->SetErrorMessageForCodeGenerationFromStrings(
        V8String(GetIsolate(), csp->EvalDisabledErrorMessage()));
    context->SetErrorMessageForWasmCodeGeneration(
        V8String(GetIsolate(), csp->WasmEvalDisabledErrorMessage()));
  }

  scoped_refptr<const SecurityOrigin> origin;
  if (world_->IsMainWorld()) {
    // This also updates the ActivityLogger for the main world.
    UpdateDocumentForMainWorld();
    origin = GetFrame()->DomWindow()->GetSecurityOrigin();
  } else {
    UpdateActivityLogger();
    origin = world_->IsolatedWorldSecurityOrigin(
        GetFrame()->DomWindow()->GetAgentClusterID());
    SetSecurityToken(origin.get());
  }

  {
    TRACE_EVENT2("v8", "ContextCreatedNotification", "IsMainFrame",
                 GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
                 GetFrame()->IsOutermostMainFrame());
    MainThreadDebugger::Instance(script_state_->GetIsolate())
        ->ContextCreated(script_state_, GetFrame(), origin.get());
    GetFrame()->Client()->DidCreateScriptContext(context, world_->GetWorldId());
  }

  InstallConditionalFeatures();

  if (World().IsMainWorld()) {
    probe::DidCreateMainWorldContext(GetFrame());
    GetFrame()->Loader().DispatchDidClearWindowObjectInMainWorld();
  }
  base::UmaHistogramTimes("V8.LocalWindowProxy.InitializeTime",
                          timer.Elapsed());
}

void LocalWindowProxy::CreateContext() {
  TRACE_EVENT2("v8", "LocalWindowProxy::CreateContext", "IsMainFrame",
               GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());
  base::ElapsedTimer timer;

  v8::ExtensionConfiguration extension_configuration =
      ScriptController::ExtensionsFor(GetFrame()->DomWindow());

  DCHECK(GetFrame()->DomWindow());
  v8::Local<v8::Context> context;
  {
    v8::Isolate* isolate = GetIsolate();
    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate));
    Document* document = GetFrame()->GetDocument();

    v8::Local<v8::Object> global_proxy = global_proxy_.Get(isolate);
    context = V8ContextSnapshot::CreateContextFromSnapshot(
        isolate, World(), &extension_configuration, global_proxy, document);
    context_was_created_from_snapshot_ = !context.IsEmpty();

    // Even if we enable V8 context snapshot feature, we may hit this branch
    // in some cases, e.g. loading XML files.
    if (context.IsEmpty()) {
      v8::Local<v8::ObjectTemplate> global_template =
          V8Window::GetWrapperTypeInfo()
              ->GetV8ClassTemplate(isolate, World())
              .As<v8::FunctionTemplate>()
              ->InstanceTemplate();
      CHECK(!global_template.IsEmpty());
      context = v8::Context::New(isolate, &extension_configuration,
                                 global_template, global_proxy,
                                 v8::DeserializeInternalFieldsCallback(),
                                 GetFrame()->DomWindow()->GetMicrotaskQueue());
      VLOG(1) << "A context is created NOT from snapshot";
    }
  }
  CHECK(!context.IsEmpty());

#if DCHECK_IS_ON()
  DidAttachGlobalObject();
#endif

  script_state_ = ScriptState::Create(context, world_, GetFrame()->DomWindow());

  DCHECK(lifecycle_ == Lifecycle::kContextIsUninitialized ||
         lifecycle_ == Lifecycle::kGlobalObjectIsDetached);
  lifecycle_ = Lifecycle::kContextIsInitialized;
  DCHECK(script_state_->ContextIsValid());
  base::UmaHistogramTimes("V8.LocalWindowProxy.CreateContextTime",
                          timer.Elapsed());
}

void LocalWindowProxy::InstallConditionalFeatures() {
  TRACE_EVENT2("v8", "InstallConditionalFeatures", "IsMainFrame",
               GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());

  if (context_was_created_from_snapshot_) {
    V8ContextSnapshot::InstallContextIndependentProps(script_state_);
  }

  V8PerContextData* per_context_data = script_state_->PerContextData();
  std::ignore =
      per_context_data->ConstructorForType(V8Window::GetWrapperTypeInfo());
  // Inform V8 that origin trial information is now connected with the context,
  // and V8 can extend the context with origin trial features.
  script_state_->GetIsolate()->InstallConditionalFeatures(
      script_state_->GetContext());
  ExtensionsRegistry::GetInstance().InstallExtensions(script_state_);
}

void LocalWindowProxy::SetupWindowPrototypeChain() {
  TRACE_EVENT2("v8", "LocalWindowProxy::SetupWindowPrototypeChain",
               "IsMainFrame", GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());

  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  DOMWindow* window = GetFrame()->DomWindow();
  const WrapperTypeInfo* wrapper_type_info = window->GetWrapperTypeInfo();
  v8::Local<v8::Context> context = script_state_->GetContext();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> global_proxy = context->Global();
  CHECK(global_proxy_ == global_proxy);
  // Use the global proxy as window wrapper object.
  V8DOMWrapper::SetNativeInfo(GetIsolate(), global_proxy, window);
  CHECK(global_proxy_ == window->AssociateWithWrapper(GetIsolate(), world_,
                                                      wrapper_type_info,
                                                      global_proxy));

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> window_wrapper =
      global_proxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::SetNativeInfo(GetIsolate(), window_wrapper, window);

  // The prototype object of Window interface.
  v8::Local<v8::Object> window_prototype =
      window_wrapper->GetPrototype().As<v8::Object>();
  CHECK(!window_prototype.IsEmpty());

  // The named properties object of Window interface.
  v8::Local<v8::Object> window_properties =
      window_prototype->GetPrototype().As<v8::Object>();
  CHECK(!window_properties.IsEmpty());
  V8DOMWrapper::SetNativeInfo(GetIsolate(), window_properties, window);

  // [CachedAccessor=kWindowProxy]
  V8PrivateProperty::GetCachedAccessor(
      GetIsolate(), V8PrivateProperty::CachedAccessor::kWindowProxy)
      .Set(window_wrapper, global_proxy);

  if (GetFrame()->GetPage()->GetChromeClient().IsPopup()) {
    // TODO(yukishiino): Remove installPagePopupController and implement
    // PagePopupController in another way.
    V8PagePopupControllerBinding::InstallPagePopupController(context,
                                                             window_wrapper);
  }
}

void LocalWindowProxy::UpdateDocumentProperty() {
  DCHECK(world_->IsMainWorld());
  TRACE_EVENT2("v8", "LocalWindowProxy::UpdateDocumentProperty", "IsMainFrame",
               GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Context> context = script_state_->GetContext();
  v8::Local<v8::Value> document_wrapper =
      ToV8Traits<Document>::ToV8(script_state_, GetFrame()->GetDocument());
  DCHECK(document_wrapper->IsObject());

  // Update the cached accessor for window.document.
  CHECK(V8PrivateProperty::GetWindowDocumentCachedAccessor(GetIsolate())
            .Set(context->Global(), document_wrapper));
}

void LocalWindowProxy::UpdateActivityLogger() {
  script_state_->PerContextData()->SetActivityLogger(
      V8DOMActivityLogger::ActivityLogger(
          world_->GetWorldId(), GetFrame()->GetDocument()
                                    ? GetFrame()->GetDocument()->baseURI()
                                    : KURL()));
}

void LocalWindowProxy::SetSecurityToken(const SecurityOrigin* origin) {
  // The security token is a fast path optimization for cross-context v8 checks.
  // If two contexts have the same token, then the SecurityOrigins can access
  // each other. Otherwise, v8 will fall back to a full CanAccess() check.
  String token;
  // The default v8 security token is to the global object itself. By
  // definition, the global object is unique and using it as the security token
  // will always trigger a full CanAccess() check from any other context.
  //
  // Using the default security token to force a callback to CanAccess() is
  // required for three things:
  // 1. When a new window is opened, the browser displays the pending URL rather
  //    than about:blank. However, if the Document is accessed, it is no longer
  //    safe to show the pending URL, as the initial empty Document may have
  //    been modified. Forcing a CanAccess() call allows Blink to notify the
  //    browser if the initial empty Document is accessed.
  // 2. If document.domain is set, a full CanAccess() check is required as two
  //    Documents are only same-origin if document.domain is set to the same
  //    value. Checking this can currently only be done in Blink, so require a
  //    full CanAccess() check.
  bool use_default_security_token =
      world_->IsMainWorld() &&
      (GetFrame()->GetDocument()->IsInitialEmptyDocument() ||
       origin->DomainWasSetInDOM());
  if (origin && !use_default_security_token)
    token = origin->ToTokenForFastCheck();

  // 3. The ToTokenForFastCheck method on SecurityOrigin returns null string for
  //    empty security origins and for security origins that should only allow
  //    access to themselves (i.e. opaque origins). Using the default security
  //    token serves for two purposes: it allows fast-path security checks for
  //    accesses inside the same context, and forces a full CanAccess() check
  //    for contexts that don't inherit the same origin.
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = script_state_->GetContext();
  if (token.IsNull()) {
    context->UseDefaultSecurityToken();
    return;
  }

  if (world_->IsIsolatedWorld()) {
    const SecurityOrigin* frame_security_origin =
        GetFrame()->DomWindow()->GetSecurityOrigin();
    String frame_security_token = frame_security_origin->ToTokenForFastCheck();
    // We need to check the return value of domainWasSetInDOM() on the
    // frame's SecurityOrigin because, if that's the case, only
    // SecurityOrigin::domain_ would have been modified.
    // domain_ is not used by SecurityOrigin::toString(), so we would end
    // up generating the same token that was already set.
    if (frame_security_origin->DomainWasSetInDOM() ||
        frame_security_token.IsNull()) {
      context->UseDefaultSecurityToken();
      return;
    }
    token = frame_security_token + token;
  }

  // NOTE: V8 does identity comparison in fast path, must use a symbol
  // as the security token.
  context->SetSecurityToken(V8AtomicString(GetIsolate(), token));
}

void LocalWindowProxy::UpdateDocument() {
  // For an uninitialized main window proxy, there's nothing we need
  // to update. The update is done when the window proxy gets initialized later.
  if (lifecycle_ == Lifecycle::kContextIsUninitialized)
    return;

  // For a navigated-away window proxy, reinitialize it as a new window with new
  // context and document.
  if (lifecycle_ == Lifecycle::kGlobalObjectIsDetached) {
    Initialize();
    DCHECK_EQ(Lifecycle::kContextIsInitialized, lifecycle_);
    // Initialization internally updates the document properties, so just
    // return afterwards.
    return;
  }

  if (!world_->IsMainWorld())
    return;

  UpdateDocumentForMainWorld();
}

void LocalWindowProxy::UpdateDocumentForMainWorld() {
  DCHECK(world_->IsMainWorld());
  UpdateActivityLogger();
  UpdateDocumentProperty();
  UpdateSecurityOrigin(GetFrame()->DomWindow()->GetSecurityOrigin());
}

namespace {

// GetNamedProperty(), Getter(), NamedItemAdded(), and NamedItemRemoved()
// optimize property access performance for Document.
//
// Document interface has [LegacyOverrideBuiltIns] and a named getter. If we
// implemented the named getter as a standard IDL-mapped code, we would call a
// Blink function before any of Document property access, and it would be
// performance overhead even for builtin properties. Our implementation updates
// V8 accessors for a Document wrapper when a named object is added or removed,
// and avoid to check existence of names objects on accessing any properties.
//
// See crbug.com/614559 for how this affected benchmarks.

v8::Local<v8::Value> GetNamedProperty(HTMLDocument* html_document,
                                      const AtomicString& key,
                                      v8::Local<v8::Object> creation_context,
                                      v8::Isolate* isolate) {
  if (!html_document->HasNamedItem(key))
    return v8::Local<v8::Value>();

  DocumentNameCollection* items = html_document->DocumentNamedItems(key);
  if (items->IsEmpty())
    return v8::Local<v8::Value>();

  if (items->HasExactlyOneItem()) {
    HTMLElement* element = items->Item(0);
    DCHECK(element);
    if (auto* iframe = DynamicTo<HTMLIFrameElement>(*element)) {
      if (Frame* frame = iframe->ContentFrame()) {
        return frame->DomWindow()->ToV8(isolate, creation_context);
      }
    }
    return element->ToV8(isolate, creation_context);
  }
  return items->ToV8(isolate, creation_context);
}

void Getter(v8::Local<v8::Name> property,
            const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (!property->IsString())
    return;
  // FIXME: Consider passing StringImpl directly.
  v8::Isolate* isolate = info.GetIsolate();
  AtomicString name = ToCoreAtomicString(isolate, property.As<v8::String>());
  HTMLDocument* html_document =
      V8HTMLDocument::ToWrappableUnsafe(isolate, info.Holder());
  DCHECK(html_document);
  v8::Local<v8::Value> namedPropertyValue =
      GetNamedProperty(html_document, name, info.Holder(), isolate);
  bool hasNamedProperty = !namedPropertyValue.IsEmpty();

  v8::Local<v8::Value> prototypeChainValue;
  bool hasPropertyInPrototypeChain =
      info.Holder()
          ->GetRealNamedPropertyInPrototypeChain(isolate->GetCurrentContext(),
                                                 property.As<v8::String>())
          .ToLocal(&prototypeChainValue);

  if (hasNamedProperty) {
    bindings::V8SetReturnValue(info, namedPropertyValue);
    UseCounter::Count(
        html_document,
        hasPropertyInPrototypeChain
            ? WebFeature::kDOMClobberedShadowedDocumentPropertyAccessed
            : WebFeature::kDOMClobberedNotShadowedDocumentPropertyAccessed);

    return;
  }
  if (hasPropertyInPrototypeChain) {
    bindings::V8SetReturnValue(info, prototypeChainValue);
  }
}

void EmptySetter(v8::Local<v8::Name> name,
                 v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<void>& info) {
  // Empty setter is required to keep the native data property in "accessor"
  // state even in case the value is updated by user code.
}

}  // namespace

void LocalWindowProxy::NamedItemAdded(HTMLDocument* document,
                                      const AtomicString& name) {
  DCHECK(world_->IsMainWorld());

  // Currently only contexts in attached frames can change the named items.
  // TODO(yukishiino): Support detached frame's case, too, since the spec is not
  // saying that the document needs to be attached to the DOM.
  // https://html.spec.whatwg.org/C/dom.html#dom-document-nameditem
  DCHECK(lifecycle_ == Lifecycle::kContextIsInitialized);
  // TODO(yukishiino): Remove the following if-clause due to the above DCHECK.
  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Object> document_wrapper =
      world_->DomDataStore().Get(GetIsolate(), document).ToLocalChecked();
  // When a non-configurable own property (e.g. unforgeable attribute) already
  // exists, `SetNativeDataProperty` fails and throws. Ignore the exception
  // because own properties have priority over named properties.
  // https://webidl.spec.whatwg.org/#dfn-named-property-visibility
  v8::TryCatch try_block(GetIsolate());
  std::ignore = document_wrapper->SetNativeDataProperty(
      GetIsolate()->GetCurrentContext(), V8String(GetIsolate(), name), Getter,
      EmptySetter);
}

void LocalWindowProxy::NamedItemRemoved(HTMLDocument* document,
                                        const AtomicString& name) {
  DCHECK(world_->IsMainWorld());

  // Currently only contexts in attached frames can change the named items.
  // TODO(yukishiino): Support detached frame's case, too, since the spec is not
  // saying that the document needs to be attached to the DOM.
  // https://html.spec.whatwg.org/C/dom.html#dom-document-nameditem
  DCHECK(lifecycle_ == Lifecycle::kContextIsInitialized);
  // TODO(yukishiino): Remove the following if-clause due to the above DCHECK.
  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  if (document->HasNamedItem(name))
    return;
  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Object> document_wrapper =
      world_->DomDataStore().Get(GetIsolate(), document).ToLocalChecked();
  document_wrapper
      ->Delete(GetIsolate()->GetCurrentContext(), V8String(GetIsolate(), name))
      .ToChecked();
}

void LocalWindowProxy::UpdateSecurityOrigin(const SecurityOrigin* origin) {
  // For an uninitialized window proxy, there's nothing we need to update. The
  // update is done when the window proxy gets initialized later.
  if (lifecycle_ == Lifecycle::kContextIsUninitialized ||
      lifecycle_ == Lifecycle::kGlobalObjectIsDetached)
    return;

  SetSecurityToken(origin);
}

void LocalWindowProxy::SetAbortScriptExecution(
    v8::Context::AbortScriptExecutionCallback callback) {
  InitializeIfNeeded();
  script_state_->GetContext()->SetAbortScriptExecution(callback);
}

LocalWindowProxy::LocalWindowProxy(v8::Isolate* isolate,
                                   LocalFrame& frame,
                                   DOMWrapperWorld* world)
    : WindowProxy(isolate, frame, world) {}

}  // namespace blink
