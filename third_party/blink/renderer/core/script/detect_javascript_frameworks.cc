// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/detect_javascript_frameworks.h"

#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {

namespace {

bool IsFrameworkVariableUsed(v8::Local<v8::Context> context,
                             const String& framework_variable_name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> global = context->Global();
  v8::TryCatch try_catch(isolate);

  bool has_property;
  bool succeeded =
      global
          ->HasRealNamedProperty(
              context, V8AtomicString(isolate, framework_variable_name))
          .To(&has_property);

  DCHECK(succeeded && !try_catch.HasCaught());
  return has_property;
}

bool IsFrameworkIDUsed(Document& document, const AtomicString& framework_id) {
  if (document.getElementById(framework_id)) {
    return true;
  }
  return false;
}

void CheckForGatsby(Document& document, v8::Local<v8::Context> context) {
  if (IsFrameworkIDUsed(document, "___gatsby")) {
    document.Loader()->DidObserveLoadingBehavior(
        kLoadingBehaviorGatsbyFrameworkUsed);
  }
}

void CheckForNextJS(Document& document, v8::Local<v8::Context> context) {
  if (IsFrameworkIDUsed(document, "__next") &&
      IsFrameworkVariableUsed(context, "__NEXT_DATA__")) {
    document.Loader()->DidObserveLoadingBehavior(
        LoadingBehaviorFlag::kLoadingBehaviorNextJSFrameworkUsed);
  }
}

void CheckForNuxtJS(Document& document, v8::Local<v8::Context> context) {
  if (IsFrameworkVariableUsed(context, "__NUXT__")) {
    document.Loader()->DidObserveLoadingBehavior(
        kLoadingBehaviorNuxtJSFrameworkUsed);
  }
}

void CheckForSapper(Document& document, v8::Local<v8::Context> context) {
  if (IsFrameworkVariableUsed(context, "__SAPPER__")) {
    document.Loader()->DidObserveLoadingBehavior(
        kLoadingBehaviorSapperFrameworkUsed);
  }
}

void CheckForVuePress(Document& document, v8::Local<v8::Context> context) {
  if (IsFrameworkVariableUsed(context, "__VUEPRESS__")) {
    document.Loader()->DidObserveLoadingBehavior(
        kLoadingBehaviorVuePressFrameworkUsed);
  }
}

}  // namespace

void DetectJavascriptFrameworksOnLoad(Document& document) {
  // Only detect Javascript frameworks on the main frame and if URL and BaseURL
  // is HTTP. Note: Without these checks, ToScriptStateForMainWorld will
  // initialize WindowProxy and trigger a second DidClearWindowObject() earlier
  // than expected for Android WebView. The Gin Java Bridge has a race condition
  // that relies on a second DidClearWindowObject() firing immediately before
  // executing JavaScript. See the document that explains this in more detail:
  // https://docs.google.com/document/d/1R5170is5vY425OO2Ru-HJBEraEKu0HjQEakcYldcSzM/edit?usp=sharing
  if (!document.GetFrame() || !document.GetFrame()->IsMainFrame() ||
      !document.Url().ProtocolIsInHTTPFamily() ||
      !document.BaseURL().ProtocolIsInHTTPFamily()) {
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(document.GetFrame());

  if (!script_state || !script_state->ContextIsValid()) {
    return;
  }

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Context> context = script_state->GetContext();

  CheckForGatsby(document, context);
  CheckForNextJS(document, context);
  CheckForNuxtJS(document, context);
  CheckForSapper(document, context);
  CheckForVuePress(document, context);
}

}  // namespace blink
