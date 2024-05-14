// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/worker_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/installed_service_worker_module_script_fetcher.h"
#include "third_party/blink/renderer/core/loader/modulescript/worker_module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

WorkerModulatorImpl::WorkerModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* WorkerModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type,
    base::PassKey<ModuleScriptLoader> pass_key) {
  auto* global_scope = To<WorkerGlobalScope>(GetExecutionContext());
  switch (custom_fetch_type) {
    case ModuleScriptCustomFetchType::kNone:
      return MakeGarbageCollected<DocumentModuleScriptFetcher>(global_scope,
                                                               pass_key);
    case ModuleScriptCustomFetchType::kWorkerConstructor:
      return MakeGarbageCollected<WorkerModuleScriptFetcher>(global_scope,
                                                             pass_key);
    case ModuleScriptCustomFetchType::kWorkletAddModule:
      break;
    case ModuleScriptCustomFetchType::kInstalledServiceWorker:
      return MakeGarbageCollected<InstalledServiceWorkerModuleScriptFetcher>(
          global_scope, pass_key);
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool WorkerModulatorImpl::IsDynamicImportForbidden(String* reason) {
  if (GetExecutionContext()->IsDedicatedWorkerGlobalScope() ||
      GetExecutionContext()->IsSharedWorkerGlobalScope()) {
    return false;
  }

  // https://html.spec.whatwg.org/C/#hostimportmoduledynamically(referencingscriptormodule,-specifier,-promisecapability)
  DCHECK(GetExecutionContext()->IsServiceWorkerGlobalScope());
  *reason =
      "import() is disallowed on ServiceWorkerGlobalScope by the HTML "
      "specification. See https://github.com/w3c/ServiceWorker/issues/1356.";
  return true;
}

}  // namespace blink
