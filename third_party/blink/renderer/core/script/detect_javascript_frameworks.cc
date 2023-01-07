// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/detect_javascript_frameworks.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr char kGatsbyId[] = "___gatsby";
constexpr char kNextjsId[] = "__next";
constexpr char kNextjsData[] = "__NEXT_DATA__";
constexpr char kNuxtjsData[] = "__NUXT__";
constexpr char kSapperData[] = "__SAPPER__";
constexpr char kVuepressData[] = "__VUEPRESS__";

bool IsFrameworkVariableUsed(v8::Local<v8::Context> context,
                             const String& framework_variable_name) {
  v8::Isolate* isolate = context->GetIsolate();
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

inline void CheckIdMatches(Document& document,
                           int& loading_behavior_flag,
                           bool& has_nextjs_id) {
  DEFINE_STATIC_LOCAL(AtomicString, kReactId, ("react-root"));
  if (IsFrameworkIDUsed(document, kGatsbyId))
    loading_behavior_flag |= kLoadingBehaviorGatsbyFrameworkUsed;
  if (IsFrameworkIDUsed(document, kNextjsId))
    has_nextjs_id = true;
  if (IsFrameworkIDUsed(document, kReactId))
    loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
}

inline void CheckAttributeMatches(const Element& element,
                                  int& loading_behavior_flag) {
  DEFINE_STATIC_LOCAL(QualifiedName, ng_version,
                      (g_null_atom, "ng-version", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, data_reactroot,
                      (g_null_atom, "data-reactroot", g_null_atom));
  static constexpr char kSvelte[] = "svelte-";
  if (element.FastHasAttribute(ng_version))
    loading_behavior_flag |= kLoadingBehaviorAngularFrameworkUsed;
  if (element.FastHasAttribute(data_reactroot))
    loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
  if (element.GetClassAttribute().StartsWith(kSvelte))
    loading_behavior_flag |= kLoadingBehaviorSvelteFrameworkUsed;
}

inline void CheckPropertyMatches(Element& element,
                                 DOMDataStore& dom_data_store,
                                 v8::Local<v8::Context> context,
                                 v8::Isolate* isolate,
                                 int& loading_behavior_flag) {
  v8::Local<v8::Object> v8_element = dom_data_store.Get(&element, isolate);
  if (v8_element.IsEmpty())
    return;
  v8::Local<v8::Array> property_names;
  if (!v8_element->GetOwnPropertyNames(context).ToLocal(&property_names))
    return;

  DEFINE_STATIC_LOCAL(AtomicString, vue_string, ("__vue__"));
  DEFINE_STATIC_LOCAL(AtomicString, vue_app_string, ("__vue_app__"));
  DEFINE_STATIC_LOCAL(AtomicString, k_string, ("__k"));
  DEFINE_STATIC_LOCAL(AtomicString, reactRootContainer_string,
                      ("_reactRootContainer"));
  DEFINE_STATIC_LOCAL(AtomicString, reactListening_string, ("_reactListening"));
  DEFINE_STATIC_LOCAL(AtomicString, reactFiber_string, ("__reactFiber"));
  for (uint32_t i = 0; i < property_names->Length(); ++i) {
    v8::Local<v8::Value> key;
    if (!property_names->Get(context, i).ToLocal(&key) || !key->IsString()) {
      continue;
    }
    AtomicString key_value = ToCoreAtomicString(key.As<v8::String>());
    if (key_value == vue_string || key_value == vue_app_string) {
      loading_behavior_flag |= kLoadingBehaviorVueFrameworkUsed;
    } else if (key_value == k_string) {
      loading_behavior_flag |= kLoadingBehaviorPreactFrameworkUsed;
    } else if (key_value == reactRootContainer_string) {
      loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
    } else if (key_value.StartsWith(reactListening_string) ||
               key_value.StartsWith(reactFiber_string)) {
      loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
    }
  }
}

inline void CheckGlobalPropertyMatches(v8::Local<v8::Context> context,
                                       v8::Isolate* isolate,
                                       int& loading_behavior_flag,
                                       bool& has_nextjs_id) {
  static constexpr char kVueData[] = "Vue";
  static constexpr char kReactData[] = "React";
  if (has_nextjs_id && IsFrameworkVariableUsed(context, kNextjsData))
    loading_behavior_flag |= kLoadingBehaviorNextJSFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kNuxtjsData))
    loading_behavior_flag |= kLoadingBehaviorNuxtJSFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kSapperData))
    loading_behavior_flag |= kLoadingBehaviorSapperFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kVuepressData))
    loading_behavior_flag |= kLoadingBehaviorVuePressFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kVueData))
    loading_behavior_flag |= kLoadingBehaviorVueFrameworkUsed;
  // TODO(npm): Add check for window.React.Component, not just window.React.
  if (IsFrameworkVariableUsed(context, kReactData))
    loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
}

void DidObserveLoadingBehaviors(Document& document, int loading_behavior_flag) {
  // TODO(npm): ideally we'd be able to surface multiple loading behaviors to
  // the document loader at once.
  static constexpr LoadingBehaviorFlag flags[] = {
      kLoadingBehaviorAngularFrameworkUsed, kLoadingBehaviorGatsbyFrameworkUsed,
      kLoadingBehaviorNextJSFrameworkUsed,  kLoadingBehaviorNextJSFrameworkUsed,
      kLoadingBehaviorNuxtJSFrameworkUsed,  kLoadingBehaviorPreactFrameworkUsed,
      kLoadingBehaviorReactFrameworkUsed,   kLoadingBehaviorSapperFrameworkUsed,
      kLoadingBehaviorSvelteFrameworkUsed,  kLoadingBehaviorVueFrameworkUsed,
      kLoadingBehaviorVuePressFrameworkUsed};
  for (LoadingBehaviorFlag flag : flags) {
    if (loading_behavior_flag & flag) {
      document.Loader()->DidObserveLoadingBehavior(flag);
    }
  }
}

void TraverseTreeForFrameworks(Document& document,
                               v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  int loading_behavior_flag = kLoadingBehaviorNone;
  bool has_nextjs_id = false;
  if (!document.documentElement())
    return;
  DOMDataStore& dom_data_store = DOMWrapperWorld::MainWorld().DomDataStore();
  for (Element& element :
       ElementTraversal::InclusiveDescendantsOf(*document.documentElement())) {
    CheckAttributeMatches(element, loading_behavior_flag);
    CheckPropertyMatches(element, dom_data_store, context, isolate,
                         loading_behavior_flag);
  }
  CheckIdMatches(document, loading_behavior_flag, has_nextjs_id);
  CheckGlobalPropertyMatches(context, isolate, loading_behavior_flag,
                             has_nextjs_id);
  DCHECK(!try_catch.HasCaught());
  DidObserveLoadingBehaviors(document, loading_behavior_flag);
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
      document.GetFrame()->IsInFencedFrameTree() ||
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
  TraverseTreeForFrameworks(document, context);
}

}  // namespace blink
