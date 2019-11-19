// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"

namespace blink {

class ResourceFetcher;

// WorkletModuleScriptFetcher is an implementation of ModuleScriptFetcher
// interface for Worklets. This implements the custom "perform the fetch" hook
// defined in the Worklets spec:
// https://html.spec.whatwg.org/C/#fetching-scripts-perform-fetch
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
//
// WorkletModuleScriptFetcher either fetchs a cached result from
// WorkletModuleResponsesMap, or defers to ModuleScriptFetcher and
// stores the result in WorkletModuleResponsesMap on fetch completion.
class CORE_EXPORT WorkletModuleScriptFetcher final
    : public GarbageCollected<WorkletModuleScriptFetcher>,
      public ModuleScriptFetcher {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletModuleScriptFetcher);

 public:
  explicit WorkletModuleScriptFetcher(WorkletModuleResponsesMap*);

  // Implements ModuleScriptFetcher.
  void Fetch(FetchParameters&,
             ResourceFetcher*,
             const Modulator* modulator_for_built_in_modules,
             ModuleGraphLevel,
             ModuleScriptFetcher::Client*) override;

 private:
  // Implements ResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "WorkletModuleScriptFetcher"; }

  // TODO(nhiroki): In general, CrossThreadPersistent is heavy and should not be
  // owned by objects that can frequently be created like this class. Instead of
  // retaining a reference to WorkletModuleResponsesMap, this class should
  // access the map via WorkletGlobalScope::GetModuleResponsesMap().
  // Bonus: WorkletGlobalScope can provide ResourceFetcher, too.
  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map_;

  KURL url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_WORKLET_MODULE_SCRIPT_FETCHER_H_
