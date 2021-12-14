// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos_extensions.h"

#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_chrome_os.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"
#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
void ChromeOSDataPropertyGetCallback(
    v8::Local<v8::Name> v8_property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creation_context =
      info.Holder()->GetCreationContextChecked();
  bindings::V8SetReturnValue(
      info,
      MakeGarbageCollected<ChromeOS>(ExecutionContext::From(creation_context)),
      creation_context);
}

void InstallChromeOSExtensions(ScriptState* script_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context ||
      !ExecutionContext::From(script_state)->IsServiceWorkerGlobalScope() ||
      !RuntimeEnabledFeatures::BlinkExtensionChromeOSEnabled()) {
    return;
  }

  auto global_proxy = script_state->GetContext()->Global();

  global_proxy
      ->SetLazyDataProperty(script_state->GetContext(),
                            V8String(script_state->GetIsolate(), "chromeos"),
                            ChromeOSDataPropertyGetCallback,
                            v8::Local<v8::Value>(), v8::DontEnum,
                            v8::SideEffectType::kHasNoSideEffect)
      .ToChecked();
}

}  // namespace

void ChromeOSExtensions::Initialize() {
  ExtensionsRegistry::GetInstance().RegisterBlinkExtensionInstallCallback(
      &InstallChromeOSExtensions);
}

}  // namespace blink
