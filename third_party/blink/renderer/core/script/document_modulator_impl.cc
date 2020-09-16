// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_modulator_impl.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

DocumentModulatorImpl::DocumentModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {}

ModuleScriptFetcher* DocumentModulatorImpl::CreateModuleScriptFetcher(
    ModuleScriptCustomFetchType custom_fetch_type,
    util::PassKey<ModuleScriptLoader> pass_key) {
  DCHECK_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
  return MakeGarbageCollected<DocumentModuleScriptFetcher>(pass_key);
}

bool DocumentModulatorImpl::IsDynamicImportForbidden(String* reason) {
  return false;
}

V8CacheOptions DocumentModulatorImpl::GetV8CacheOptions() const {
  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  const Settings* settings = window->GetFrame()->GetSettings();
  if (settings)
    return settings->GetV8CacheOptions();
  return kV8CacheOptionsDefault;
}

}  // namespace blink
