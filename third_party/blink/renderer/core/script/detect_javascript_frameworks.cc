// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/detect_javascript_frameworks.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/javascript_framework_detection.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr char kGatsbyId[] = "___gatsby";
constexpr char kNextjsData[] = "next";
constexpr char kNuxtjsData[] = "__NUXT__";
constexpr char kSapperData[] = "__SAPPER__";
constexpr char kVuepressData[] = "__VUEPRESS__";
constexpr char kShopify[] = "Shopify";
constexpr char kSquarespace[] = "Squarespace";

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
                           JavaScriptFrameworkDetectionResult& result) {
  DEFINE_STATIC_LOCAL(AtomicString, kReactId, ("react-root"));
  if (IsFrameworkIDUsed(document, AtomicString(kGatsbyId))) {
    result.detected_versions[JavaScriptFramework::kGatsby] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkIDUsed(document, kReactId)) {
    result.detected_versions[JavaScriptFramework::kReact] =
        kNoFrameworkVersionDetected;
  }
}

inline void CheckAttributeMatches(const Element& element,
                                  JavaScriptFrameworkDetectionResult& result,
                                  AtomicString& detected_ng_version) {
  DEFINE_STATIC_LOCAL(QualifiedName, ng_version, (AtomicString("ng-version")));
  DEFINE_STATIC_LOCAL(QualifiedName, data_reactroot,
                      (AtomicString("data-reactroot")));
  static constexpr char kSvelte[] = "svelte-";
  if (element.FastHasAttribute(data_reactroot)) {
    result.detected_versions[JavaScriptFramework::kReact] =
        kNoFrameworkVersionDetected;
  }
  if (element.GetClassAttribute().StartsWith(kSvelte)) {
    result.detected_versions[JavaScriptFramework::kSvelte] =
        kNoFrameworkVersionDetected;
  }
  if (element.FastHasAttribute(ng_version)) {
    result.detected_versions[JavaScriptFramework::kAngular] =
        kNoFrameworkVersionDetected;
    detected_ng_version = element.FastGetAttribute(ng_version);
  }
}

inline void CheckPropertyMatches(Element& element,
                                 DOMDataStore& dom_data_store,
                                 v8::Local<v8::Context> context,
                                 v8::Isolate* isolate,
                                 JavaScriptFrameworkDetectionResult& result) {
  v8::Local<v8::Object> v8_element;
  if (!dom_data_store.Get(isolate, &element).ToLocal(&v8_element)) {
    return;
  }
  v8::Local<v8::Array> property_names;
  if (!v8_element->GetOwnPropertyNames(context).ToLocal(&property_names)) {
    return;
  }

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
    AtomicString key_value = ToCoreAtomicString(isolate, key.As<v8::String>());
    if (key_value == vue_string || key_value == vue_app_string) {
      result.detected_versions[JavaScriptFramework::kVue] =
          kNoFrameworkVersionDetected;
    } else if (key_value == k_string) {
      result.detected_versions[JavaScriptFramework::kPreact] =
          kNoFrameworkVersionDetected;
    } else if (key_value == reactRootContainer_string) {
      result.detected_versions[JavaScriptFramework::kReact] =
          kNoFrameworkVersionDetected;
    } else if (key_value.StartsWith(reactListening_string) ||
               key_value.StartsWith(reactFiber_string)) {
      result.detected_versions[JavaScriptFramework::kReact] =
          kNoFrameworkVersionDetected;
    }
  }
}

inline void CheckGlobalPropertyMatches(
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    JavaScriptFrameworkDetectionResult& result) {
  static constexpr char kVueData[] = "Vue";
  static constexpr char kVue3Data[] = "__VUE__";
  static constexpr char kReactData[] = "React";
  if (IsFrameworkVariableUsed(context, kNextjsData)) {
    result.detected_versions[JavaScriptFramework::kNext] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kNuxtjsData)) {
    result.detected_versions[JavaScriptFramework::kNuxt] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kSapperData)) {
    result.detected_versions[JavaScriptFramework::kSapper] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kVuepressData)) {
    result.detected_versions[JavaScriptFramework::kVuePress] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kVueData) ||
      IsFrameworkVariableUsed(context, kVue3Data)) {
    result.detected_versions[JavaScriptFramework::kVue] =
        kNoFrameworkVersionDetected;
  }
  // TODO(npm): Add check for window.React.Component, not just window.React.
  if (IsFrameworkVariableUsed(context, kReactData)) {
    result.detected_versions[JavaScriptFramework::kReact] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kShopify)) {
    result.detected_versions[JavaScriptFramework::kShopify] =
        kNoFrameworkVersionDetected;
  }
  if (IsFrameworkVariableUsed(context, kSquarespace)) {
    result.detected_versions[JavaScriptFramework::kSquarespace] =
        kNoFrameworkVersionDetected;
  }
}

int64_t ExtractVersion(v8::Local<v8::RegExp> regexp,
                       v8::Local<v8::Context> context,
                       v8::Local<v8::Value> version) {
  v8::Local<v8::Object> groups;
  v8::Local<v8::Value> major;
  v8::Local<v8::Value> minor;
  bool success =
      regexp->Exec(context, version.As<v8::String>()).ToLocal(&groups);
  if (!success || !groups->IsArray()) {
    return kNoFrameworkVersionDetected;
  }
  v8::Local<v8::Array> groups_array = groups.As<v8::Array>();
  if (!groups_array->Get(context, 1).ToLocal(&major) ||
      !groups_array->Get(context, 2).ToLocal(&minor) || !major->IsString() ||
      !minor->IsString()) {
    return kNoFrameworkVersionDetected;
  }

  v8::Local<v8::Value> major_number;
  v8::Local<v8::Value> minor_number;
  if (!major->ToNumber(context).ToLocal(&major_number) ||
      !minor->ToNumber(context).ToLocal(&minor_number)) {
    return kNoFrameworkVersionDetected;
  }

  // Major & minor versions are clamped to 8bits to avoid using this as a
  // vector to identify users.
  return ((major_number->IntegerValue(context).FromMaybe(0) & 0xff) << 8) |
         (minor_number->IntegerValue(context).FromMaybe(0) & 0xff);
}

void DetectFrameworkVersions(Document& document,
                             v8::Local<v8::Context> context,
                             v8::Isolate* isolate,
                             JavaScriptFrameworkDetectionResult& result,
                             const AtomicString& detected_ng_version) {
  v8::Local<v8::Object> global = context->Global();
  static constexpr char kVersionPattern[] = "([0-9]+)\\.([0-9]+)";
  v8::Local<v8::RegExp> version_regexp;

  if (!v8::RegExp::New(context, V8AtomicString(isolate, kVersionPattern),
                       v8::RegExp::kNone)
           .ToLocal(&version_regexp)) {
    return;
  }

  auto SafeGetProperty = [&](v8::Local<v8::Value> object,
                             const char* prop_name) -> v8::Local<v8::Value> {
    if (object.IsEmpty() || !object->IsObject()) {
      return v8::Undefined(isolate);
    }

    v8::Local<v8::Value> value;
    if (!object.As<v8::Object>()
             ->GetRealNamedProperty(context, V8AtomicString(isolate, prop_name))
             .ToLocal(&value)) {
      return v8::Undefined(isolate);
    }

    return value;
  };

  if (result.detected_versions.contains(JavaScriptFramework::kNext)) {
    static constexpr char kNext[] = "next";
    static constexpr char kVersion[] = "version";
    int64_t version = kNoFrameworkVersionDetected;
    v8::Local<v8::Value> version_string =
        SafeGetProperty(SafeGetProperty(global, kNext), kVersion);
    if (!version_string.IsEmpty() && version_string->IsString()) {
      version = ExtractVersion(version_regexp, context, version_string);
    }

    result.detected_versions[JavaScriptFramework::kNext] = version;
  }

  if (!detected_ng_version.IsNull()) {
    result.detected_versions[JavaScriptFramework::kAngular] = ExtractVersion(
        version_regexp, context,
        v8::String::NewFromUtf8(isolate,
                                detected_ng_version.GetString().Utf8().c_str())
            .FromMaybe(v8::String::Empty(isolate)));
  }

  if (result.detected_versions.contains(JavaScriptFramework::kVue)) {
    static constexpr char kVue2[] = "Vue";
    static constexpr char kVersion[] = "version";
    if (global->HasRealNamedProperty(context, V8AtomicString(isolate, kVue2))
            .FromMaybe(false)) {
      v8::Local<v8::Value> version_string =
          SafeGetProperty(SafeGetProperty(global, kVue2), kVersion);
      if (!version_string.IsEmpty() && version_string->IsString()) {
        result.detected_versions[JavaScriptFramework::kVue] =
            ExtractVersion(version_regexp, context, version_string);
      }
    } else {
      static constexpr char kVue3[] = "__VUE__";
      bool vue3 = false;
      if (global->HasRealNamedProperty(context, V8AtomicString(isolate, kVue3))
              .To(&vue3) &&
          vue3) {
        result.detected_versions[JavaScriptFramework::kVue] = 0x300;
      }
    }
  }

  HTMLMetaElement* generator_meta = nullptr;

  if (document.head()) {
    for (HTMLMetaElement& meta_element :
         Traversal<HTMLMetaElement>::DescendantsOf(*document.head())) {
      if (EqualIgnoringASCIICase(meta_element.GetName(), "generator")) {
        generator_meta = &meta_element;
        break;
      }
    }
  }

  if (generator_meta) {
    const AtomicString& content = generator_meta->Content();
    if (!content.empty()) {
      if (content.StartsWith("Wix")) {
        result.detected_versions[JavaScriptFramework::kWix] =
            kNoFrameworkVersionDetected;
      } else if (content.StartsWith("Joomla")) {
        result.detected_versions[JavaScriptFramework::kJoomla] =
            kNoFrameworkVersionDetected;
      } else {
        constexpr char wordpress_prefix[] = "WordPress ";
        constexpr size_t wordpress_prefix_length =
            std::char_traits<char>::length(wordpress_prefix);

        if (content.StartsWith(wordpress_prefix)) {
          String version_string =
              String(content).Substring(wordpress_prefix_length);
          result.detected_versions[JavaScriptFramework::kWordPress] =
              ExtractVersion(version_regexp, context,
                             V8String(isolate, version_string));
        }

        constexpr char drupal_prefix[] = "Drupal ";
        constexpr size_t drupal_prefix_length =
            std::char_traits<char>::length(drupal_prefix);

        if (content.StartsWith(drupal_prefix)) {
          String version_string =
              String(content).Substring(drupal_prefix_length);
          String trimmed =
              version_string.Substring(0, version_string.Find(" "));
          bool ok = true;
          int version = trimmed.ToInt(&ok);
          result.detected_versions[JavaScriptFramework::kDrupal] =
              ok ? ((version & 0xff) << 8) : kNoFrameworkVersionDetected;
        }
      }
    }
  }
}

void TraverseTreeForFrameworks(Document& document,
                               v8::Isolate* isolate,
                               v8::Local<v8::Context> context) {
  v8::TryCatch try_catch(isolate);
  JavaScriptFrameworkDetectionResult result;
  AtomicString detected_ng_version;
  if (!document.documentElement())
    return;
  DOMDataStore& dom_data_store =
      DOMWrapperWorld::MainWorld(isolate).DomDataStore();
  for (Element& element :
       ElementTraversal::InclusiveDescendantsOf(*document.documentElement())) {
    CheckAttributeMatches(element, result, detected_ng_version);
    CheckPropertyMatches(element, dom_data_store, context, isolate, result);
  }
  CheckIdMatches(document, result);
  CheckGlobalPropertyMatches(context, isolate, result);
  DetectFrameworkVersions(document, context, isolate, result,
                          detected_ng_version);
  DCHECK(!try_catch.HasCaught());
  document.Loader()->DidObserveJavaScriptFrameworks(result);
}

}  // namespace

void DetectJavascriptFrameworksOnLoad(Document& document) {
  LocalFrame* const frame = document.GetFrame();
  if (!frame || !frame->IsOutermostMainFrame() ||
      !document.Url().ProtocolIsInHTTPFamily() ||
      !document.BaseURL().ProtocolIsInHTTPFamily()) {
    return;
  }

  v8::Isolate* const isolate = ToIsolate(frame);
  // It would be simpler to call `ToScriptStateForMainWorld()`; however, this
  // forces WindowProxy initialization, which is somewhat expensive.  If the
  // WindowProxy isn't already initialized, there are no JS frameworks by
  // definition. As a bonus, this also helps preserve a historical quirk for Gin
  // Java Bridge in Android WebView:
  // https://docs.google.com/document/d/1R5170is5vY425OO2Ru-HJBEraEKu0HjQEakcYldcSzM/edit?usp=sharing
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      ToV8ContextMaybeEmpty(frame, DOMWrapperWorld::MainWorld(isolate));
  if (context.IsEmpty()) {
    return;
  }

  ScriptState* script_state = ScriptState::From(isolate, context);
  DCHECK(script_state && script_state->ContextIsValid());

  ScriptState::Scope scope(script_state);
  TraverseTreeForFrameworks(document, isolate, context);
}

}  // namespace blink
