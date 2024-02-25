// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos_extensions.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"
#include "third_party/blink/renderer/extensions/chromeos/event_interface_chromeos_names.h"
#include "third_party/blink/renderer/extensions/chromeos/event_target_chromeos_names.h"
#include "third_party/blink/renderer/extensions/chromeos/event_type_chromeos_names.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/extensions_registry.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
void ChromeOSDataPropertyGetCallback(
    v8::Local<v8::Name> v8_property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  bindings::V8SetReturnValue(info, MakeGarbageCollected<ChromeOS>(),
                             info.Holder()->GetCreationContextChecked());
}

// Whether we should install ChromeOS extensions in `execution_context`.
bool IsSupportedExecutionContext(ExecutionContext* execution_context) {
  if (!execution_context) {
    return false;
  }
  return execution_context->IsWindow() ||
         execution_context->IsServiceWorkerGlobalScope();
}

void InstallChromeOSExtensions(ScriptState* script_state) {
  auto* execution_context = ExecutionContext::From(script_state);
  if (!IsSupportedExecutionContext(execution_context) ||
      !RuntimeEnabledFeatures::BlinkExtensionChromeOSEnabled(
          execution_context)) {
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

  // Static strings need to be initialized here, before
  // CoreInitializer::Initialize().
  const unsigned kChromeOSStaticStringsCount =
      event_target_names::kChromeOSNamesCount +
      event_type_names::kChromeOSNamesCount +
      event_interface_names::kChromeOSNamesCount;
  StringImpl::ReserveStaticStringsCapacityForSize(
      kChromeOSStaticStringsCount + StringImpl::AllStaticStrings().size());

  event_target_names::InitChromeOS();
  event_type_names::InitChromeOS();
  event_interface_names::InitChromeOS();
}

void ChromeOSExtensions::InitServiceWorkerGlobalScope(
    ServiceWorkerGlobalScope& worker_global_scope) {
  if (!RuntimeEnabledFeatures::BlinkExtensionChromeOSEnabled())
    return;
}

}  // namespace blink
