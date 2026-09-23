// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
#define GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_

#include <cstdint>

#include "v8-sandbox.h"

namespace gin {

// The range of CppHeapPointerTags that are used for gin::Wrappable objects.
// See v8::CppHeapPointerTagRange for details.
constexpr v8::CppHeapPointerTagRange kGinWrappableTagRange(
    v8::CppHeapPointerTag::kFirstEmbedderWrappableTag,
    static_cast<v8::CppHeapPointerTag>(0x00ff));

// The range of CppHeapPointerTags that are used for v8::Object::Wrappable
// objects and are within the blink namespace.
// See v8::CppHeapPointerTagRange for details.
constexpr v8::CppHeapPointerTagRange kBlinkWrappableTagRange(
    static_cast<v8::CppHeapPointerTag>(0x0100),
    v8::CppHeapPointerTag::kLastEmbedderWrappableTag);

static_assert(v8::kEmbedderWrappableTagRange.Contains(kGinWrappableTagRange));
static_assert(v8::kEmbedderWrappableTagRange.Contains(kBlinkWrappableTagRange));
static_assert(static_cast<uint16_t>(kGinWrappableTagRange.last) <
              static_cast<uint16_t>(kBlinkWrappableTagRange.first));

// The range of CppHeapPointerTags that are used for non-v8::Object::Wrappable
// objects and are within the gin namespace.
// See v8::CppHeapPointerTagRange for details.
constexpr v8::CppHeapPointerTagRange kGinNonWrappableTagRange(
    v8::CppHeapPointerTag::kFirstEmbedderNonWrappableTag,
    static_cast<v8::CppHeapPointerTag>(0x71ff));

// The range of CppHeapPointerTags that are used for non-v8::Object::Wrappable
// objects and are within the blink namespace.
// See v8::CppHeapPointerTagRange for details.
constexpr v8::CppHeapPointerTagRange kBlinkNonWrappableTagRange(
    static_cast<v8::CppHeapPointerTag>(0x7200),
    v8::CppHeapPointerTag::kLastEmbedderNonWrappableTag);

static_assert(
    v8::kEmbedderNonWrappableTagRange.Contains(kGinNonWrappableTagRange));
static_assert(
    v8::kEmbedderNonWrappableTagRange.Contains(kBlinkNonWrappableTagRange));
static_assert(static_cast<uint16_t>(kGinNonWrappableTagRange.last) <
              static_cast<uint16_t>(kBlinkNonWrappableTagRange.first));

// References from V8 JavaScript objects to C++ objects are stored with a type
// tag, and dereferencing a C++ object is only possible when the same type tag
// is used. E.g. a reference to an ArrayBuffer object can only be dereferenced
// using the ArrayBuffer type tag. This enum defines type tags for subclasses of
// `gin::Wrappable`, so that the JavaScript wrapper objects of these subclasses
// can only be unwrapped with the correct type tag.
enum WrappablePointerTag : uint16_t {
  kFirstPointerTag = static_cast<uint16_t>(kGinWrappableTagRange.first),
  // keep-sorted start case=no
  kAccessibilityControllerBindings =
      kFirstPointerTag,          // content::AccessibilityControllerBindings
  kAPIBindingBridge,             // extensions::APIBindingBridge
  kAPIBindingJSUtil,             // extensions::APIBindingJSUtil
  kAutomationPosition,           // ui::AutomationPosition
  kBenchmarkingBindings,         // BenchmarkingBindings
  kCallbackHolderBase,           // gin::internal::CallbackHolderBase
  kChromePluginPlaceholder,      // ChromePluginPlaceholder
  kChromeSetting,                // extensions::ChromeSetting
  kContentSetting,               // extensions::ContentSetting
  kDeclarativeEvent,             // extensions::DeclarativeEvent
  kDomAutomationController,      // content::DomAutomationController
  kEventEmitter,                 // extensions::EventEmitter
  kEventSenderBindings,          // content::EventSenderBindings
  kGamepadControllerBindings,    // content::GameControllerBindings
  kGCController,                 // content::GCController
  kGinJavaBridgeObject,          // content::GinJavaBridgeObject
  kGinPort,                      // extensions::GinPort
  kGpuBenchmarking,              // content::GpuBenchmarking
  kIndigoContext,                // indigo::IndigoContext
  kIndigoOnboarding,             // indigo::OnboardingContext
  kJsBinding,                    // js_injection::JsBinding
  kJSHookInterface,              // extensions::JSHookInterface
  kJsMessageEvent,               // android_webview::JsMessageEvent
  kJsSandboxMessagePort,         // android_webview::JsSandboxMessagePort
  kLastErrorObject,              // extensions::LastErrorObject
  kLoadTimesBindings,            // LoadTimesBindings
  kLocalStorageArea,             // extensions::LocalStorageArea
  kManagedStorageArea,           // extensions::ManagedStorageArea
  kMyInterceptor,                // gin::MyInterceptor
  kNetErrorPageController,       // NetErrorPageController
  kNewTabPageBindings,           // NewTabPageBindings
  kPDFPluginPlaceholder,         // PDFPluginPlaceholder
  kPluginPlaceholder,            // plugins::PluginPlaceholder
  kPostMessageReceiver,          // chrome_pdf::PostMessageReceiver
  kPostMessageScriptableObject,  // extensions::(anonymous)::ScriptableObject
  kReadAnythingAppController,    // ReadAnythingAppController
  kRemoteObject,                 // blink::RemoteObject
  kSearchBoxBindings,            // SearchBoxBindings
  kSecurityInterstitialPageController,  // SecurityInterstitialPageController
  kSessionStorageArea,                  // extensions::SessionStorageArea
  kSkiaBenchmarking,                    // content::SkiaBenchmarking
  kStatsCollectionController,           // content::StatsCollectionController
  kSupervisedUserErrorPageController,   // SupervisedUserErrorPageController
  kSyncStorageArea,                     // extensions::SyncStorageArea
  kTestGinWrappable,                    // GinWrappable
  kTestObject,                          // gin::TestGinObject
  kTestObject2,                         // gin::MyObject2
  kTestPluginScriptableObject,   // content::(anonymous)::ScriptableObject
  kTestRunnerBindings,           // content::TestRunnerBindings
  kTextInputControllerBindings,  // content::TextInputControllerBindings
  kWebAXObjectProxy,             // content::WebAXObjectProxy
  kWrappedExceptionHandler,      // extensions::WrappedExceptionHandler
  kWrappedHandlerFunction,       // extensions::WrappedHandlerFunction
  // keep-sorted end
  kLastPointerTag,
};

static_assert(kGinWrappableTagRange.Contains(
                  static_cast<v8::CppHeapPointerTag>(kLastPointerTag)),
              "The defined WrappablePointerTags exceed kGinWrappableTagRange.");

// Pointer tags for non-gin::Wrappable classes.
enum NonWrappablePointerTag : uint16_t {
  kFirstNonWrappablePointerTag =
      static_cast<uint16_t>(kGinNonWrappableTagRange.first),
  // keep-sorted start case=no
  kGinPerContextData = kFirstNonWrappablePointerTag,  // gin::PerContextData
  // keep-sorted end
  kLastNonWrappablePointerTag = kGinPerContextData,
};

static_assert(
    kGinNonWrappableTagRange.Contains(
        static_cast<v8::CppHeapPointerTag>(kLastNonWrappablePointerTag)),
    "The defined NonWrappablePointerTags exceed kGinNonWrappableTagRange.");

}  // namespace gin

#endif  // GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
