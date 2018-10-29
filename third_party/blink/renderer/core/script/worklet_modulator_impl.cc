// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/worklet_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/worklet_module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"

namespace blink {

ModulatorImplBase* WorkletModulatorImpl::Create(ScriptState* script_state) {
  return new WorkletModulatorImpl(script_state);
}

WorkletModulatorImpl::WorkletModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* WorkletModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type) {
  DCHECK_EQ(ModuleScriptCustomFetchType::kWorkletAddModule, custom_fetch_type);
  WorkletGlobalScope* global_scope =
      To<WorkletGlobalScope>(GetExecutionContext());
  return new WorkletModuleScriptFetcher(global_scope->EnsureFetcher(),
                                        global_scope->GetModuleResponsesMap());
}

bool WorkletModulatorImpl::IsDynamicImportForbidden(String* reason) {
  *reason = "import() is disallowed on WorkletGlobalScope.";
  return true;
}

}  // namespace blink
