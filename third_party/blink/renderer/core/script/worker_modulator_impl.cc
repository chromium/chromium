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
    ModuleScriptCustomFetchType custom_fetch_type,
    util::PassKey<ModuleScriptLoader> pass_key) {
  auto* global_scope = To<WorkerGlobalScope>(GetExecutionContext());
  switch (custom_fetch_type) {
    case ModuleScriptCustomFetchType::kNone:
      return MakeGarbageCollected<DocumentModuleScriptFetcher>(pass_key);
    case ModuleScriptCustomFetchType::kWorkerConstructor:
      return MakeGarbageCollected<WorkerModuleScriptFetcher>(global_scope,
                                                             pass_key);
    case ModuleScriptCustomFetchType::kWorkletAddModule:
      break;
    case ModuleScriptCustomFetchType::kInstalledServiceWorker:
      return MakeGarbageCollected<InstalledServiceWorkerModuleScriptFetcher>(
          global_scope, pass_key);
  }
  NOTREACHED();
  return nullptr;
}

bool WorkerModulatorImpl::IsDynamicImportForbidden(String* reason) {
  if (GetExecutionContext()->IsDedicatedWorkerGlobalScope())
    return false;

  // TODO(https://crbug.com/824646): Remove this flag check once module loading
  // for SharedWorker is enabled by default.
  if (GetExecutionContext()->IsSharedWorkerGlobalScope() &&
      RuntimeEnabledFeatures::ModuleSharedWorkerEnabled()) {
    return false;
  }

  // TODO(https://crbug.com/824647): Support module loading for Service Worker.
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
