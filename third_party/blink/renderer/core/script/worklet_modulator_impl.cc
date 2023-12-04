// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/worklet_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"

namespace blink {

WorkletModulatorImpl::WorkletModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* WorkletModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type,
    base::PassKey<ModuleScriptLoader> pass_key) {
  DCHECK_EQ(ModuleScriptCustomFetchType::kWorkletAddModule, custom_fetch_type);
  WorkletGlobalScope* global_scope =
      To<WorkletGlobalScope>(GetExecutionContext());
  return MakeGarbageCollected<WorkletModuleScriptFetcher>(global_scope,
                                                          pass_key);
}

bool WorkletModulatorImpl::IsDynamicImportForbidden(String* reason) {
  *reason = "import() is disallowed on WorkletGlobalScope.";
  return true;
}

}  // namespace blink
