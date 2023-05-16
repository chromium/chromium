// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/detect_javascript_frameworks.h"

#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8-container.h"
#include "v8-local-handle.h"
#include "v8-object.h"
#include "v8-primitive.h"
#include "v8-regexp.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr char kGatsbyId[] = "___gatsby";
constexpr char kNextjsId[] = "__next";
constexpr char kNextjsData[] = "__NEXT_DATA__";
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
                                  int& loading_behavior_flag,
                                  AtomicString& detected_ng_version) {
  DEFINE_STATIC_LOCAL(QualifiedName, ng_version,
                      (g_null_atom, "ng-version", g_null_atom));
  DEFINE_STATIC_LOCAL(QualifiedName, data_reactroot,
                      (g_null_atom, "data-reactroot", g_null_atom));
  static constexpr char kSvelte[] = "svelte-";
  if (element.FastHasAttribute(ng_version)) {
    loading_behavior_flag |= kLoadingBehaviorAngularFrameworkUsed;
    detected_ng_version = element.FastGetAttribute(ng_version);
  }
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
  static constexpr char kVue3Data[] = "__VUE__";
  static constexpr char kReactData[] = "React";
  if (has_nextjs_id && IsFrameworkVariableUsed(context, kNextjsData))
    loading_behavior_flag |= kLoadingBehaviorNextJSFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kNuxtjsData))
    loading_behavior_flag |= kLoadingBehaviorNuxtJSFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kSapperData))
    loading_behavior_flag |= kLoadingBehaviorSapperFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kVuepressData))
    loading_behavior_flag |= kLoadingBehaviorVuePressFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kVueData) ||
      IsFrameworkVariableUsed(context, kVue3Data)) {
    loading_behavior_flag |= kLoadingBehaviorVueFrameworkUsed;
  }
  // TODO(npm): Add check for window.React.Component, not just window.React.
  if (IsFrameworkVariableUsed(context, kReactData))
    loading_behavior_flag |= kLoadingBehaviorReactFrameworkUsed;
  if (IsFrameworkVariableUsed(context, kShopify)) {
    loading_behavior_flag |= kLoadingBehaviorShopifyCMSUsed;
  }
  if (IsFrameworkVariableUsed(context, kSquarespace)) {
    loading_behavior_flag |= kLoadingBehaviorSquarespaceCMSUsed;
  }
}

void DidObserveLoadingBehaviors(Document& document, int loading_behavior_flag) {
  // TODO(npm): ideally we'd be able to surface multiple loading behaviors to
  // the document loader at once.
  static constexpr LoadingBehaviorFlag flags[] = {
      kLoadingBehaviorAngularFrameworkUsed,
      kLoadingBehaviorGatsbyFrameworkUsed,
      kLoadingBehaviorNextJSFrameworkUsed,
      kLoadingBehaviorNextJSFrameworkUsed,
      kLoadingBehaviorNuxtJSFrameworkUsed,
      kLoadingBehaviorPreactFrameworkUsed,
      kLoadingBehaviorReactFrameworkUsed,
      kLoadingBehaviorSapperFrameworkUsed,
      kLoadingBehaviorSvelteFrameworkUsed,
      kLoadingBehaviorVueFrameworkUsed,
      kLoadingBehaviorVuePressFrameworkUsed,
      kLoadingBehaviorDrupalCMSUsed,
      kLoadingBehaviorJoomlaCMSUsed,
      kLoadingBehaviorShopifyCMSUsed,
      kLoadingBehaviorSquarespaceCMSUsed,
      kLoadingBehaviorWixCMSUsed,
      kLoadingBehaviorWordPressCMSUsed,
  };
  for (LoadingBehaviorFlag flag : flags) {
    if (loading_behavior_flag & flag) {
      document.Loader()->DidObserveLoadingBehavior(flag);
    }
  }
}

absl::optional<int64_t> ExtractVersion(v8::Local<v8::RegExp> regexp,
                                       v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> version) {
  v8::Local<v8::Object> groups;
  v8::Local<v8::Value> major;
  v8::Local<v8::Value> minor;
  bool success =
      regexp->Exec(context, version.As<v8::String>()).ToLocal(&groups);
  if (!success || !groups->IsArray()) {
    return absl::nullopt;
  }
  v8::Local<v8::Array> groups_array = groups.As<v8::Array>();
  if (!groups_array->Get(context, 1).ToLocal(&major) ||
      !groups_array->Get(context, 2).ToLocal(&minor) || !major->IsString() ||
      !minor->IsString()) {
    return absl::nullopt;
  }

  v8::Local<v8::Value> major_number;
  v8::Local<v8::Value> minor_number;
  if (!major->ToNumber(context).ToLocal(&major_number) ||
      !minor->ToNumber(context).ToLocal(&minor_number)) {
    return absl::nullopt;
  }

  // Major & minor versions are clamped to 8bits to avoid using this as a
  // vector to identify users.
  return ((major_number->IntegerValue(context).FromMaybe(0) & 0xff) << 8) |
         (minor_number->IntegerValue(context).FromMaybe(0) & 0xff);
}

void DetectFrameworkVersions(Document& document,
                             v8::Local<v8::Context> context,
                             v8::Isolate* isolate,
                             int& loading_behavior_flags,
                             const AtomicString& detected_ng_version) {
  if (!document.UkmRecorder() ||
      document.UkmSourceID() == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::Blink_JavaScriptFramework_Versions builder(
      document.UkmSourceID());
  v8::Local<v8::Object> global = context->Global();
  static constexpr char kVersionPattern[] = "([0-9]+)\\.([0-9]+)";
  v8::Local<v8::RegExp> version_regexp =
      v8::RegExp::New(context, V8AtomicString(isolate, kVersionPattern),
                      v8::RegExp::kNone)
          .ToLocalChecked();
  bool detected = false;

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

  if (loading_behavior_flags & kLoadingBehaviorNextJSFrameworkUsed) {
    static constexpr char kNext[] = "next";
    static constexpr char kVersion[] = "version";
    v8::Local<v8::Value> version_string =
        SafeGetProperty(SafeGetProperty(global, kNext), kVersion);
    if (!version_string.IsEmpty() && version_string->IsString()) {
      absl::optional<int64_t> version =
          ExtractVersion(version_regexp, context, version_string);
      if (version.has_value()) {
        detected = true;
        builder.SetNextJSVersion(version.value());
      }
    }
  }

  if (!detected_ng_version.IsNull()) {
    absl::optional<int64_t> version = ExtractVersion(
        version_regexp, context,
        v8::String::NewFromUtf8(isolate,
                                detected_ng_version.GetString().Utf8().c_str())
            .FromMaybe(v8::String::Empty(isolate)));
    if (version.has_value()) {
      detected = true;
      builder.SetAngularVersion(version.value());
    }
  }

  if (loading_behavior_flags & kLoadingBehaviorVueFrameworkUsed) {
    static constexpr char kVue2[] = "Vue";
    static constexpr char kVersion[] = "version";
    if (global->HasRealNamedProperty(context, V8AtomicString(isolate, kVue2))
            .FromMaybe(false)) {
      v8::Local<v8::Value> version_string =
          SafeGetProperty(SafeGetProperty(global, kVue2), kVersion);
      if (!version_string.IsEmpty() && version_string->IsString()) {
        absl::optional<int64_t> version =
            ExtractVersion(version_regexp, context, version_string);
        if (version.has_value()) {
          detected = true;
          builder.SetVueVersion(version.value());
        }
      }
    } else {
      static constexpr char kVue3[] = "__VUE__";
      bool vue3 = false;
      if (global->HasRealNamedProperty(context, V8AtomicString(isolate, kVue3))
              .To(&vue3) &&
          vue3) {
        detected = true;
        // Vue3.x doesn't provide a detectable minor version number.
        builder.SetVueVersion(0x300);
      }
    }
  }

  HTMLMetaElement* generator_meta = nullptr;

  for (HTMLMetaElement& meta_element :
       Traversal<HTMLMetaElement>::DescendantsOf(*document.head())) {
    if (EqualIgnoringASCIICase(meta_element.GetName(), "generator")) {
      generator_meta = &meta_element;
      break;
    }
  }

  if (generator_meta) {
    const AtomicString& content = generator_meta->Content();
    if (!content.empty()) {
      if (content.StartsWith("Wix")) {
        loading_behavior_flags |= kLoadingBehaviorWixCMSUsed;
      } else if (content.StartsWith("Joomla")) {
        loading_behavior_flags |= kLoadingBehaviorJoomlaCMSUsed;
      } else {
        constexpr char wordpress_prefix[] = "WordPress ";
        constexpr size_t wordpress_prefix_length =
            std::char_traits<char>::length(wordpress_prefix);

        if (content.StartsWith(wordpress_prefix)) {
          String version_string =
              String(content).Substring(wordpress_prefix_length);
          absl::optional<int64_t> version = ExtractVersion(
              version_regexp, context, V8String(isolate, version_string));
          if (version) {
            detected = true;
            loading_behavior_flags |= kLoadingBehaviorWordPressCMSUsed;
            builder.SetWordPressVersion(version.value());
          }
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
          if (ok) {
            detected = true;
            loading_behavior_flags |= kLoadingBehaviorDrupalCMSUsed;
            builder.SetDrupalVersion((version & 0xff) << 8);
          }
        }
      }
    }
  }

  if (detected) {
    builder.Record(document.UkmRecorder());
  }
}

void TraverseTreeForFrameworks(Document& document,
                               v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  int loading_behavior_flag = kLoadingBehaviorNone;
  AtomicString detected_ng_version;
  bool has_nextjs_id = false;
  if (!document.documentElement())
    return;
  DOMDataStore& dom_data_store = DOMWrapperWorld::MainWorld().DomDataStore();
  for (Element& element :
       ElementTraversal::InclusiveDescendantsOf(*document.documentElement())) {
    CheckAttributeMatches(element, loading_behavior_flag, detected_ng_version);
    CheckPropertyMatches(element, dom_data_store, context, isolate,
                         loading_behavior_flag);
  }
  CheckIdMatches(document, loading_behavior_flag, has_nextjs_id);
  CheckGlobalPropertyMatches(context, isolate, loading_behavior_flag,
                             has_nextjs_id);
  DCHECK(!try_catch.HasCaught());
  DetectFrameworkVersions(document, context, isolate, loading_behavior_flag,
                          detected_ng_version);
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
