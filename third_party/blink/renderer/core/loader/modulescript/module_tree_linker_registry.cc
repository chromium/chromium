// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

void ModuleTreeLinkerRegistry::Fetch(
    const KURL& url,
    const ModuleType& module_type,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::blink::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    const ScriptFetchOptions& options,
    Modulator* modulator,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client,
    String referrer) {
  ModuleTreeLinker* linker = MakeGarbageCollected<ModuleTreeLinker>(
      fetch_client_settings_object_fetcher, context_type, destination,
      modulator, custom_fetch_type, this, client,
      base::PassKey<ModuleTreeLinkerRegistry>());
  AddLinker(linker);
  linker->FetchRoot(url, module_type, options,
                    base::PassKey<ModuleTreeLinkerRegistry>(), referrer);
  DCHECK(linker->IsFetching());
}

void ModuleTreeLinkerRegistry::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    mojom::blink::RequestContextType context_type,
    network::mojom::RequestDestination destination,
    Modulator* modulator,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client) {
  ModuleTreeLinker* linker = MakeGarbageCollected<ModuleTreeLinker>(
      fetch_client_settings_object_fetcher, context_type, destination,
      modulator, custom_fetch_type, this, client,
      base::PassKey<ModuleTreeLinkerRegistry>());
  AddLinker(linker);
  linker->FetchRootInline(module_script,
                          base::PassKey<ModuleTreeLinkerRegistry>());
  DCHECK(linker->IsFetching());
}

void ModuleTreeLinkerRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(active_tree_linkers_);
}

void ModuleTreeLinkerRegistry::AddLinker(ModuleTreeLinker* linker) {
  DCHECK(!active_tree_linkers_.Contains(linker));
  active_tree_linkers_.insert(linker);
}

void ModuleTreeLinkerRegistry::ReleaseFinishedLinker(ModuleTreeLinker* linker) {
  DCHECK(linker->HasFinished());

  auto it = active_tree_linkers_.find(linker);
  CHECK_NE(it, active_tree_linkers_.end(), base::NotFatalUntil::M130);
  active_tree_linkers_.erase(it);
}

}  // namespace blink
