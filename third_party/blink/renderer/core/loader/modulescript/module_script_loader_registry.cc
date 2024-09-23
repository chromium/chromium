// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader_registry.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_loader.h"

namespace blink {

void ModuleScriptLoaderRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(active_loaders_);
}

void ModuleScriptLoaderRegistry::AddLoader(ModuleScriptLoader* loader) {
  DCHECK(loader->IsInitialState());
  DCHECK(!active_loaders_.Contains(loader));
  active_loaders_.insert(loader);
}

void ModuleScriptLoaderRegistry::ReleaseFinishedLoader(
    ModuleScriptLoader* loader) {
  DCHECK(loader->HasFinished());

  auto it = active_loaders_.find(loader);
  CHECK_NE(it, active_loaders_.end(), base::NotFatalUntil::M130);
  active_loaders_.erase(it);
}

}  // namespace blink
