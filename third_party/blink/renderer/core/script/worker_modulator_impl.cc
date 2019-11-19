// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/worker_modulator_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/worker_module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

WorkerModulatorImpl::WorkerModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* WorkerModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type) {
  auto* global_scope = To<WorkerGlobalScope>(GetExecutionContext());
  switch (custom_fetch_type) {
    case ModuleScriptCustomFetchType::kNone:
      return MakeGarbageCollected<DocumentModuleScriptFetcher>();
    case ModuleScriptCustomFetchType::kWorkerConstructor:
      return MakeGarbageCollected<WorkerModuleScriptFetcher>(global_scope);
    case ModuleScriptCustomFetchType::kWorkletAddModule:
      break;
    case ModuleScriptCustomFetchType::kInstalledServiceWorker:
      return MakeGarbageCollected<InstalledServiceWorkerModuleScriptFetcher>(
          global_scope);
  }
  NOTREACHED();
  return nullptr;
}

bool WorkerModulatorImpl::IsDynamicImportForbidden(String* reason) {
  // TODO(nhiroki): Remove this flag check once module loading for
  // DedicatedWorker is enabled by default (https://crbug.com/680046).
  if (GetExecutionContext()->IsDedicatedWorkerGlobalScope() &&
      RuntimeEnabledFeatures::ModuleDedicatedWorkerEnabled()) {
    return false;
  }

  // TODO(nhiroki): Support module loading for SharedWorker and Service Worker.
  // (https://crbug.com/680046)
  *reason =
      "Module scripts are not supported on WorkerGlobalScope yet (see "
      "https://crbug.com/680046).";
  return true;
}

V8CacheOptions WorkerModulatorImpl::GetV8CacheOptions() const {
  auto* scope = To<WorkerGlobalScope>(GetExecutionContext());
  return scope->GetV8CacheOptions();
}

}  // namespace blink
