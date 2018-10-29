// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_TREE_LINKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_TREE_LINKER_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ModuleScriptFetchRequest;
class ModuleTreeLinkerRegistry;

// A ModuleTreeLinker is responsible for running and keeping intermediate states
// for a top-level [IMSGF] "internal module script graph fetching procedure" or
// a top-level [FDaI] "fetch the descendants of and instantiate", and all the
// invocations of [IMSGF] and [FD] "fetch the descendants" under that.
//
// Spec links:
// [IMSGF]
// https://html.spec.whatwg.org/#internal-module-script-graph-fetching-procedure
// [FD]
// https://html.spec.whatwg.org/#fetch-the-descendants-of-a-module-script
// [FDaI]
// https://html.spec.whatwg.org/#fetch-the-descendants-of-and-instantiate-a-module-script
// [FFPE]
// https://html.spec.whatwg.org/#finding-the-first-parse-error
class CORE_EXPORT ModuleTreeLinker final : public SingleModuleClient {
 public:
  // https://html.spec.whatwg.org/#fetch-a-module-script-tree
  static void Fetch(
      const KURL&,
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      mojom::RequestContextType destination,
      const ScriptFetchOptions&,
      Modulator*,
      ModuleScriptCustomFetchType,
      ModuleTreeLinkerRegistry*,
      ModuleTreeClient*);

  // [FDaI] for an inline script.
  static void FetchDescendantsForInlineScript(
      ModuleScript*,
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      mojom::RequestContextType destination,
      Modulator*,
      ModuleScriptCustomFetchType,
      ModuleTreeLinkerRegistry*,
      ModuleTreeClient*);

  ~ModuleTreeLinker() override = default;
  void Trace(blink::Visitor*) override;

  bool IsFetching() const {
    return State::kFetchingSelf <= state_ && state_ < State::kFinished;
  }
  bool HasFinished() const { return state_ == State::kFinished; }

 private:
  ModuleTreeLinker(
      FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
      mojom::RequestContextType destination,
      Modulator*,
      ModuleScriptCustomFetchType,
      ModuleTreeLinkerRegistry*,
      ModuleTreeClient*);

  enum class State {
    kInitial,
    // Running fetch of the module script corresponding to the target node.
    kFetchingSelf,
    // Running fetch of descendants of the target node.
    kFetchingDependencies,
    // Instantiating module_script_ and the node descendants.
    kInstantiating,
    kFinished,
  };
#if DCHECK_IS_ON()
  static const char* StateToString(State);
#endif
  void AdvanceState(State);

  void FetchRoot(const KURL&, const ScriptFetchOptions&);
  void FetchRootInline(ModuleScript*);

  // Steps 1--2 of [IMSGF].
  void InitiateInternalModuleScriptGraphFetching(
      const ModuleScriptFetchRequest&,
      ModuleGraphLevel);

  // Steps 3--7 of [IMSGF], and [FD]/[FDaI] called from [IMSGF].
  // TODO(hiroshige): Currently
  void NotifyModuleLoadFinished(ModuleScript*) override;
  void FetchDescendants(ModuleScript*);

  // Completion of [FD].
  void FinalizeFetchDescendantsForOneModuleScript();

  // [FDaI] Steps 4--8.
  void Instantiate();

  // [FFPE]
  ScriptValue FindFirstParseError(ModuleScript*,
                                  HeapHashSet<Member<ModuleScript>>*) const;

  const Member<FetchClientSettingsObjectSnapshot> fetch_client_settings_object_;
  const mojom::RequestContextType destination_;
  const Member<Modulator> modulator_;
  const ModuleScriptCustomFetchType custom_fetch_type_;
  HashSet<KURL> visited_set_;
  const Member<ModuleTreeLinkerRegistry> registry_;
  const Member<ModuleTreeClient> client_;
  State state_ = State::kInitial;

  // Correspond to _result_ in
  // https://html.spec.whatwg.org/multipage/webappapis.html#internal-module-script-graph-fetching-procedure
  TraceWrapperMember<ModuleScript> result_;

  bool found_parse_error_ = false;

  size_t num_incomplete_fetches_ = 0;

#if DCHECK_IS_ON()
  KURL original_url_;
  KURL url_;
  bool root_is_inline_;

  friend CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                              const ModuleTreeLinker&);
#endif
};

#if DCHECK_IS_ON()
CORE_EXPORT std::ostream& operator<<(std::ostream&, const ModuleTreeLinker&);
#endif

}  // namespace blink

#endif
