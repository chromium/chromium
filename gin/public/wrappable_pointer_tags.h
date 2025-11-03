// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
#define GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_

#include <cstdint>

#include "v8-sandbox.h"

namespace gin {

// References from V8 JavaScript objects to C++ objects are stored with a type
// tag, and dereferencing a C++ object is only possible when the same type tag
// is used. E.g. a reference to an ArrayBuffer object can only be dereferenced
// using the ArrayBuffer type tag. This enum defines type tags for subclasses of
// `gin::Wrappable`, so that the JavaScript wrapper objects of these subclasses
// can only be unwrapped with the correct type tag.
enum WrappablePointerTag : uint16_t {
  // The type tags for gin::Wrappable start at the end of the value range to
  // avoid overlaps with the type tags of blink::ScriptWrappable.
  kFirstPointerTag = 1601,
  kAccessibilityControllerBindings,  // content::AccessibilityControllerBindings
  kAPIBindingBridge,                 // extensions::APIBindingBridge
  kAPIBindingJSUtil,                 // extensions::APIBindingJSUtil
  kAutomationPosition,               // ui::AutomationPosition
  kChromePluginPlaceholder,          // ChromePluginPlaceholder
  kChromeSetting,                    // extensions::ChromeSetting
  kContentSetting,                   // extensions::ContentSetting
  kDeclarativeEvent,                 // extensions::DeclarativeEvent
  kDomAutomationController,          // content::DomAutomationController
  kEventEmitter,                     // extensions::EventEmitter
  kEventSenderBindings,              // content::EventSenderBindings
  kGamepadControllerBindings,        // content::GameControllerBindings
  kGCController,                     // content::GCController
  kGinJavaBridgeObject,              // content::GinJavaBridgeObject
  kGinPort,                          // extensions::GinPort
  kGpuBenchmarking,                  // content::GpuBenchmarking
  kJsBinding,                        // js_injection::JsBinding
  kJsMessageEvent,                   // android_webview::JsMessageEvent
  kJsSandboxMessagePort,             // android_webview::JsSandboxMessagePort
  kJSHookInterface,                  // extensions::JSHookInterface
  kLastErrorObject,                  // extensions::LastErrorObject
  kLocalStorageArea,                 // extensions::LocalStorageArea
  kManagedStorageArea,               // extensions::ManagedStorageArea
  kMojo,                             // ax::Mojo
  kMojoHandle,                       // ax::MojoHandle
  kMojoWatcher,                      // ax::MojoWatcher
  kMyInterceptor,                    // gin::MyInterceptor
  kNetErrorPageController,           // NetErrorPageController
  kNewTabPageBindings,               // NewTabPageBindings
  kPDFPluginPlaceholder,             // PDFPluginPlaceholder
  kPluginPlaceholder,                // plugins::PluginPlaceholder
  kPostMessageReceiver,              // chrome_pdf::PostMessageReceiver
  kPostMessageScriptableObject,  // extensions::(anonymous)::ScriptableObject
  kReadAnythingAppController,    // ReadAnythingAppController
  kRemoteObject,                 // blink::RemoteObject
  kSearchBoxBindings,            // SearchBoxBindings
  kSecurityInterstitialPageController,  // SecurityInterstitialPageController
  kSessionStorageArea,                  // extensions::SessionStorageArea
  kSharedStorageMethod,                 // auction_worklet::SharedStorageMethod
  kSkiaBenchmarking,                    // content::SkiaBenchmarking
  kStatsCollectionController,           // content::StatsCollectionController
  kSupervisedUserErrorPageController,   // SupervisedUserErrorPageController
  kSyncStorageArea,                     // extensions::SyncStorageArea
  kTestGinWrappable,                    // GinWrappable
  kTestObject,                          // gin::TestGinObject
  kTestObject2,                         // gin::MyObject2
  kTestPluginScriptableObject,   // content::(anonymous)::ScriptableObject
  kTestRunnerBindings,           // content::TestRunnerBindings
  kTextDecoder,                  // ax::TextDecoder
  kTextEncoder,                  // ax::TextEncoder
  kTextInputControllerBindings,  // content::TextInputControllerBindings
  kWebAXObjectProxy,             // content::WebAXObjectProxy
  kWrappedExceptionHandler,      // extensions::WrappedExceptionHandler
  kLastPointerTag = kWrappedExceptionHandler,
};

static_assert(kLastPointerTag <
                  static_cast<uint16_t>(v8::CppHeapPointerTag::kZappedEntryTag),
              "The defined type tags exceed the range of allowed tags. Adjust "
              "the start value of this enum such that all values fit.");

}  // namespace gin

#endif  // GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
