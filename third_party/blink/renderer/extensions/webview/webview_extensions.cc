// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/webview_extensions.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/webview/android.h"
#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void WebViewDataPropertyGetCallback(
    v8::Local<v8::Name> v8_property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  bindings::V8SetReturnValue(info, MakeGarbageCollected<Android>(),
                             info.Holder()->GetCreationContextChecked());
}

// Whether we should install WebView extensions in `execution_context`.
bool IsSupportedExecutionContext(const ExecutionContext* execution_context) {
  if (!execution_context) {
    return false;
  }
  return execution_context->IsWindow();
}

void InstallWebViewExtensions(ScriptState* script_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  if (!IsSupportedExecutionContext(execution_context)) {
    return;
  }
  if (!RuntimeEnabledFeatures::BlinkExtensionWebViewEnabled(
          execution_context)) {
    return;
  }

  auto global_proxy = script_state->GetContext()->Global();

  global_proxy
      ->SetLazyDataProperty(script_state->GetContext(),
                            V8String(script_state->GetIsolate(), "android"),
                            WebViewDataPropertyGetCallback,
                            v8::Local<v8::Value>(), v8::DontEnum,
                            v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
}

// static
void WebViewExtensions::Initialize() {
  ExtensionsRegistry::GetInstance().RegisterBlinkExtensionInstallCallback(
      &InstallWebViewExtensions);
}

}  // namespace blink
