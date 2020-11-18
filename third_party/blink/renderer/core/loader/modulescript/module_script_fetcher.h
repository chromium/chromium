// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_FETCHER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

class ConsoleMessage;
class ModuleScriptLoader;

// ModuleScriptFetcher is an abstract class to fetch module scripts. Derived
// classes are expected to fetch a module script for the given FetchParameters
// and return its client a fetched resource as ModuleScriptCreationParams.
class CORE_EXPORT ModuleScriptFetcher : public ResourceClient {
 public:
  // ModuleScriptFetcher should only be called from ModuleScriptLoader.
  explicit ModuleScriptFetcher(base::PassKey<ModuleScriptLoader>);

  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void NotifyFetchFinished(
        const base::Optional<ModuleScriptCreationParams>&,
        const HeapVector<Member<ConsoleMessage>>& error_messages) = 0;

    // These helpers are used only from WorkletModuleResponsesMap.
    // TODO(nhiroki): Move these helpers to WorkletModuleResponsesMap.
    void OnFetched(const base::Optional<ModuleScriptCreationParams>&);
    void OnFailed();
  };

  // Fetch() must be called right after ModuleScriptFetcher is constructed.
  // Fetch() must not be called more than once.
  //
  // Takes a non-const reference to FetchParameters because
  // ScriptResource::Fetch() requires it.
  virtual void Fetch(FetchParameters&,
                     ResourceFetcher*,
                     ModuleGraphLevel,
                     Client*) = 0;

  void Trace(Visitor*) const override;

 protected:
  static bool WasModuleLoadSuccessful(
      Resource* resource,
      HeapVector<Member<ConsoleMessage>>* error_messages,
      ModuleScriptCreationParams::ModuleType* module_type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_FETCHER_H_
