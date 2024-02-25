// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_modulator_impl.h"

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"

namespace blink {

DocumentModulatorImpl::DocumentModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* DocumentModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type,
    base::PassKey<ModuleScriptLoader> pass_key) {
  DCHECK_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
  return MakeGarbageCollected<DocumentModuleScriptFetcher>(
      GetExecutionContext(), pass_key);
}

bool DocumentModulatorImpl::IsDynamicImportForbidden(String* reason) {
  return false;
}

}  // namespace blink
