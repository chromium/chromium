// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker.h"

#include "third_party/blink/renderer/bindings/core/v8/script_module.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_tree_linker_registry.h"
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

void ModuleTreeLinker::Fetch(
    const KURL& url,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    const ScriptFetchOptions& options,
    Modulator* modulator,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(fetch_client_settings_object, destination, modulator,
                           custom_fetch_type, registry, client);
  registry->AddFetcher(fetcher);
  fetcher->FetchRoot(url, options);
  DCHECK(fetcher->IsFetching());
}

void ModuleTreeLinker::FetchDescendantsForInlineScript(
    ModuleScript* module_script,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    Modulator* modulator,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client) {
  DCHECK(module_script);
  ModuleTreeLinker* fetcher =
      new ModuleTreeLinker(fetch_client_settings_object, destination, modulator,
                           custom_fetch_type, registry, client);
  registry->AddFetcher(fetcher);
  fetcher->FetchRootInline(module_script);
  DCHECK(fetcher->IsFetching());
}

ModuleTreeLinker::ModuleTreeLinker(
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    Modulator* modulator,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeLinkerRegistry* registry,
    ModuleTreeClient* client)
    : fetch_client_settings_object_(fetch_client_settings_object),
      destination_(destination),
      modulator_(modulator),
      custom_fetch_type_(custom_fetch_type),
      registry_(registry),
      client_(client) {
  CHECK(modulator);
  CHECK(registry);
  CHECK(client);
}

void ModuleTreeLinker::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetch_client_settings_object_);
  visitor->Trace(modulator_);
  visitor->Trace(registry_);
  visitor->Trace(client_);
  visitor->Trace(result_);
  SingleModuleClient::Trace(visitor);
}

#if DCHECK_IS_ON()
const char* ModuleTreeLinker::StateToString(ModuleTreeLinker::State state) {
  switch (state) {
    case State::kInitial:
      return "Initial";
    case State::kFetchingSelf:
      return "FetchingSelf";
    case State::kFetchingDependencies:
      return "FetchingDependencies";
    case State::kInstantiating:
      return "Instantiating";
    case State::kFinished:
      return "Finished";
  }
  NOTREACHED();
  return "";
}
#endif

void ModuleTreeLinker::AdvanceState(State new_state) {
#if DCHECK_IS_ON()
  RESOURCE_LOADING_DVLOG(1)
      << *this << "::advanceState(" << StateToString(state_) << " -> "
      << StateToString(new_state) << ")";
#endif

  switch (state_) {
    case State::kInitial:
      CHECK_EQ(num_incomplete_fetches_, 0u);
      CHECK_EQ(new_state, State::kFetchingSelf);
      break;
    case State::kFetchingSelf:
      CHECK_EQ(num_incomplete_fetches_, 0u);
      CHECK(new_state == State::kFetchingDependencies ||
            new_state == State::kFinished);
      break;
    case State::kFetchingDependencies:
      CHECK(new_state == State::kInstantiating ||
            new_state == State::kFinished);
      break;
    case State::kInstantiating:
      CHECK_EQ(new_state, State::kFinished);
      break;
    case State::kFinished:
      NOTREACHED();
      break;
  }

  state_ = new_state;

  if (state_ == State::kFinished) {
#if DCHECK_IS_ON()
    if (result_) {
      RESOURCE_LOADING_DVLOG(1)
          << *this << " finished with final result " << *result_;
    } else {
      RESOURCE_LOADING_DVLOG(1) << *this << " finished with nullptr.";
    }
#endif

    registry_->ReleaseFinishedFetcher(this);

    // [IMSGF] Step 6. When the appropriate algorithm asynchronously completes
    // with final result, asynchronously complete this algorithm with final
    // result.
    client_->NotifyModuleTreeLoadFinished(result_);
  }
}

// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
void ModuleTreeLinker::FetchRoot(const KURL& original_url,
                                 const ScriptFetchOptions& options) {
#if DCHECK_IS_ON()
  original_url_ = original_url;
  root_is_inline_ = false;
#endif

  AdvanceState(State::kFetchingSelf);

  KURL url = original_url;
  // <spec
  // href="https://github.com/drufball/layered-apis/blob/master/spec.md#fetch-a-module-script-graph"
  // step="1">Set url to the layered API fetching URL given url and the current
  // settings object's API base URL.</spec>
  if (RuntimeEnabledFeatures::LayeredAPIEnabled())
    url = blink::layered_api::ResolveFetchingURL(url);

#if DCHECK_IS_ON()
  url_ = url;
#endif

  // <spec
  // href="https://github.com/drufball/layered-apis/blob/master/spec.md#fetch-a-module-script-graph"
  // step="2">If url is failure, asynchronously complete this algorithm with
  // null.</spec>
  if (!url.IsValid()) {
    result_ = nullptr;
    modulator_->TaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&ModuleTreeLinker::AdvanceState,
                             WrapPersistent(this), State::kFinished));
    return;
  }

  // Step 1. Let visited set be << url >>.
  visited_set_.insert(url);

  // Step 2. Perform the internal module script graph fetching procedure given
  // url, settings object, destination, options, settings object, visited set,
  // "client", and with the top-level module fetch flag set.
  ModuleScriptFetchRequest request(url, destination_, options,
                                   Referrer::ClientReferrerString(),
                                   TextPosition::MinimumPosition());

  InitiateInternalModuleScriptGraphFetching(
      request, ModuleGraphLevel::kTopLevelModuleFetch);
}

void ModuleTreeLinker::FetchRootInline(ModuleScript* module_script) {
  // Top-level entry point for [FDaI] for an inline module script.
  DCHECK(module_script);
#if DCHECK_IS_ON()
  original_url_ = module_script->BaseURL();
  url_ = original_url_;
  root_is_inline_ = true;
#endif

  AdvanceState(State::kFetchingSelf);

  // Store the |module_script| here which will be used as result of the
  // algorithm when success. Also, this ensures that the |module_script| is
  // traced via ModuleTreeLinker.
  result_ = module_script;
  AdvanceState(State::kFetchingDependencies);

  modulator_->TaskRunner()->PostTask(
      FROM_HERE,
      WTF::Bind(&ModuleTreeLinker::FetchDescendants, WrapPersistent(this),
                WrapPersistent(module_script)));
}

void ModuleTreeLinker::InitiateInternalModuleScriptGraphFetching(
    const ModuleScriptFetchRequest& request,
    ModuleGraphLevel level) {
  // [IMSGF] Step 1. Assert: visited set contains url.
  DCHECK(visited_set_.Contains(request.Url()));

  ++num_incomplete_fetches_;

  // [IMSGF] Step 2. Fetch a single module script given ...
  modulator_->FetchSingle(request, fetch_client_settings_object_.Get(), level,
                          custom_fetch_type_, this);

  // [IMSGF] Step 3-- are executed when NotifyModuleLoadFinished() is called.
}

void ModuleTreeLinker::NotifyModuleLoadFinished(ModuleScript* module_script) {
  // [IMSGF] Step 3. Return from this algorithm, and run the following steps
  // when fetching a single module script asynchronously completes with result:

  CHECK_GT(num_incomplete_fetches_, 0u);
  --num_incomplete_fetches_;

#if DCHECK_IS_ON()
  if (module_script) {
    RESOURCE_LOADING_DVLOG(1)
        << *this << "::NotifyModuleLoadFinished() with " << *module_script;
  } else {
    RESOURCE_LOADING_DVLOG(1)
        << *this << "::NotifyModuleLoadFinished() with nullptr.";
  }
#endif

  if (state_ == State::kFetchingSelf) {
    // Corresponds to top-level calls to
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-the-descendants-of-and-instantiate-a-module-script
    // i.e. [IMSGF] with the top-level module fetch flag set (external), or
    // Step 22 of "prepare a script" (inline).
    // |module_script| is the top-level module, and will be instantiated
    // and returned later.
    result_ = module_script;
    AdvanceState(State::kFetchingDependencies);
  }

  if (state_ != State::kFetchingDependencies) {
    // We may reach here if one of the descendant failed to load, and the other
    // descendants fetches were in flight.
    return;
  }

  // Note: top-level module fetch flag is implemented so that Instantiate()
  // is called once after all descendants are fetched, which corresponds to
  // the single invocation of "fetch the descendants of and instantiate".

  // [IMSGF] Step 4. If result is null, asynchronously complete this algorithm
  // with null, and abort these steps.
  if (!module_script) {
    result_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // [IMSGF] Step 5. If the top-level module fetch flag is set, fetch the
  // descendants of and instantiate result given destination and visited set.
  // Otherwise, fetch the descendants of result given the same arguments.
  FetchDescendants(module_script);
}

void ModuleTreeLinker::FetchDescendants(ModuleScript* module_script) {
  DCHECK(module_script);

  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    result_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // [FD] Step 2. Let record be module script's record.
  ScriptModule record = module_script->Record();

  // [FD] Step 1. If module script's record is null, then asynchronously
  // complete this algorithm with module script and abort these steps.
  if (record.IsNull()) {
    found_parse_error_ = true;
    // We don't early-exit here and wait until all module scripts to be
    // loaded, because we might be not sure which error to be reported.
    //
    // It is possible to determine whether the error to be reported can be
    // determined without waiting for loading module scripts, and thus to
    // early-exit here if possible. However, the complexity of such early-exit
    // implementation might be high, and optimizing error cases with the
    // implementation cost might be not worth doing.
    FinalizeFetchDescendantsForOneModuleScript();
    return;
  }

  // [FD] Step 3. If record.[[RequestedModules]] is empty, asynchronously
  // complete this algorithm with module script.
  //
  // Note: We defer this bail-out until the end of the procedure. The rest of
  // the procedure will be no-op anyway if record.[[RequestedModules]] is empty.

  // [FD] Step 4. Let urls be a new empty list.
  Vector<KURL> urls;
  Vector<TextPosition> positions;

  // [FD] Step 5. For each string requested of record.[[RequestedModules]],
  Vector<Modulator::ModuleRequest> module_requests =
      modulator_->ModuleRequestsFromScriptModule(record);
  for (const auto& module_request : module_requests) {
    // [FD] Step 5.1. Let url be the result of resolving a module specifier
    // given module script and requested.
    KURL url = module_script->ResolveModuleSpecifier(module_request.specifier);

    // [FD] Step 5.2. Assert: url is never failure, because resolving a module
    // specifier must have been previously successful with these same two
    // arguments.
    CHECK(url.IsValid()) << "ModuleScript::ResolveModuleSpecifier() impl must "
                            "return either a valid url or null.";

    // [FD] Step 5.3. If visited set does not contain url, then:
    if (!visited_set_.Contains(url)) {
      // [FD] Step 5.3.1. Append url to urls.
      urls.push_back(url);

      // [FD] Step 5.3.2. Append url to visited set.
      visited_set_.insert(url);

      positions.push_back(module_request.position);
    }
  }

  if (urls.IsEmpty()) {
    // [FD] Step 3. If record.[[RequestedModules]] is empty, asynchronously
    // complete this algorithm with module script.
    //
    // Also, if record.[[RequestedModules]] is not empty but |urls| is
    // empty here, we complete this algorithm.
    FinalizeFetchDescendantsForOneModuleScript();
    return;
  }

  // [FD] Step 6. Let options be the descendant script fetch options for module
  // script's fetch options.
  // https://html.spec.whatwg.org/multipage/webappapis.html#descendant-script-fetch-options
  // the descendant script fetch options are a new script fetch options whose
  // items all have the same values, except for the integrity metadata, which is
  // instead the empty string.
  ScriptFetchOptions options(module_script->FetchOptions().Nonce(),
                             IntegrityMetadataSet(), String(),
                             module_script->FetchOptions().ParserState(),
                             module_script->FetchOptions().CredentialsMode(),
                             module_script->FetchOptions().GetReferrerPolicy());

  // [FD] Step 7. For each url in urls, ...
  //
  // [FD] Step 7. These invocations of the internal module script graph fetching
  // procedure should be performed in parallel to each other.
  for (size_t i = 0; i < urls.size(); ++i) {
    // [FD] Step 7. ... perform the internal module script graph fetching
    // procedure given url, fetch client settings object, destination, options,
    // module script's settings object, visited set, module script's base URL,
    // and with the top-level module fetch flag unset. ...
    ModuleScriptFetchRequest request(urls[i], destination_, options,
                                     module_script->BaseURL().GetString(),
                                     positions[i]);
    InitiateInternalModuleScriptGraphFetching(
        request, ModuleGraphLevel::kDependentModuleFetch);
  }

  // Asynchronously continue processing after NotifyModuleLoadFinished() is
  // called num_incomplete_fetches_ times.
  CHECK_GT(num_incomplete_fetches_, 0u);
}

void ModuleTreeLinker::FinalizeFetchDescendantsForOneModuleScript() {
  // [FD] of a single module script is completed here:
  //
  // [FD] Step 7. Otherwise, wait until all of the internal module script graph
  // fetching procedure invocations have asynchronously completed. ...

  // And, if |num_incomplete_fetches_| is 0, all the invocations of [FD]
  // (called from [FDaI] Step 2) of the root module script is completed here
  // and thus we proceed to [FDaI] Step 4 implemented by Instantiate().
  if (num_incomplete_fetches_ == 0)
    Instantiate();
}

void ModuleTreeLinker::Instantiate() {
  // [nospec] Abort the steps if the browsing context is discarded.
  if (!modulator_->HasValidContext()) {
    result_ = nullptr;
    AdvanceState(State::kFinished);
    return;
  }

  // [FDaI] Step 4. If result is null, then asynchronously complete this
  // algorithm with result.
  if (!result_) {
    AdvanceState(State::kFinished);
    return;
  }

  // [FDaI] Step 6. If parse error is null, then:
  //
  // [Optimization] If |found_parse_error_| is false (i.e. no parse errors
  // were found during fetching), we are sure that |parse error| is null and
  // thus skip FindFirstParseError() call.
  if (!found_parse_error_) {
#if DCHECK_IS_ON()
    HeapHashSet<Member<ModuleScript>> discovered_set;
    DCHECK(FindFirstParseError(result_, &discovered_set).IsEmpty());
#endif

    // [FDaI] Step 6.1. Let record be result's record.
    ScriptModule record = result_->Record();

    // [FDaI] Step 6.2. Perform record.Instantiate().
    AdvanceState(State::kInstantiating);
    ScriptValue instantiation_error = modulator_->InstantiateModule(record);

    // [FDaI] Step 6.2. If this throws an exception, set result's error to
    // rethrow to that exception.
    if (!instantiation_error.IsEmpty())
      result_->SetErrorToRethrow(instantiation_error);
  } else {
    // [FDaI] Step 7. Otherwise ...

    // [FFPE] Step 2. If discoveredSet was not given, let it be an empty set.
    HeapHashSet<Member<ModuleScript>> discovered_set;

    // [FDaI] Step 5. Let parse error be the result of finding the first parse
    // error given result.
    ScriptValue parse_error = FindFirstParseError(result_, &discovered_set);
    DCHECK(!parse_error.IsEmpty());

    // [FDaI] Step 7. ... set result's error to rethrow to parse error.
    result_->SetErrorToRethrow(parse_error);
  }

  // [FDaI] Step 8. Asynchronously complete this algorithm with result.
  AdvanceState(State::kFinished);
}

// [FFPE] https://html.spec.whatwg.org/#finding-the-first-parse-error
//
// This returns non-empty ScriptValue iff a parse error is found.
ScriptValue ModuleTreeLinker::FindFirstParseError(
    ModuleScript* module_script,
    HeapHashSet<Member<ModuleScript>>* discovered_set) const {
  // FindFirstParseError() is called only when there is no fetch errors, i.e.
  // all module scripts in the graph are non-null.
  DCHECK(module_script);

  // [FFPE] Step 1. Let moduleMap be moduleScript's settings object's module
  // map.
  //
  // This is accessed via |modulator_|.

  // [FFPE] Step 2 is done before calling this in Instantiate().

  // [FFPE] Step 3. Append moduleScript to discoveredSet.
  discovered_set->insert(module_script);

  // [FFPE] Step 4. If moduleScript's record is null, then return moduleScript's
  // parse error.
  ScriptModule record = module_script->Record();
  if (record.IsNull())
    return module_script->CreateParseError();

  // [FFPE] Step 5. Let childSpecifiers be the value of moduleScript's record's
  // [[RequestedModules]] internal slot.
  Vector<Modulator::ModuleRequest> child_specifiers =
      modulator_->ModuleRequestsFromScriptModule(record);

  for (const auto& module_request : child_specifiers) {
    // [FFPE] Step 6. Let childURLs be the list obtained by calling resolve a
    // module specifier once for each item of childSpecifiers, given
    // moduleScript and that item.
    KURL child_url =
        module_script->ResolveModuleSpecifier(module_request.specifier);

    // [FFPE] Step 6. ...  (None of these will ever fail, as otherwise
    // moduleScript would have been marked as itself having a parse error.)
    CHECK(child_url.IsValid())
        << "ModuleScript::ResolveModuleSpecifier() impl must "
           "return either a valid url or null.";

    // [FFPE] Step 7. Let childModules be the list obtained by getting each
    // value in moduleMap whose key is given by an item of childURLs.
    //
    // [FFPE] Step 8. For each childModule of childModules:
    ModuleScript* child_module = modulator_->GetFetchedModuleScript(child_url);

    // [FFPE] Step 8.1. Assert: childModule is a module script (i.e., it is not
    // "fetching" or null)
    CHECK(child_module);

    // [FFPE] Step 8.2. If discoveredSet already contains childModule, continue.
    if (discovered_set->Contains(child_module))
      continue;

    // [FFPE] Step 8.3. Let childParseError be the result of finding the first
    // parse error given childModule and discoveredSet.
    ScriptValue child_parse_error =
        FindFirstParseError(child_module, discovered_set);

    // [FFPE] Step 8.4. If childParseError is not null, return childParseError.
    if (!child_parse_error.IsEmpty())
      return child_parse_error;
  }

  // [FFPE] Step 9. Return null.
  return ScriptValue();
}

#if DCHECK_IS_ON()
std::ostream& operator<<(std::ostream& stream, const ModuleTreeLinker& linker) {
  stream << "ModuleTreeLinker[" << &linker
         << ", original_url=" << linker.original_url_.GetString()
         << ", url=" << linker.url_.GetString()
         << ", inline=" << linker.root_is_inline_ << "]";
  return stream;
}
#endif

}  // namespace blink
