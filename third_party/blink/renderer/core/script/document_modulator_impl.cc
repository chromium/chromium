// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/document_modulator_impl.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {
Vector<AtomicString> FindUrlPrefixes(AtomicString specifier) {
  Vector<size_t> positions;
  constexpr char slash = '/';
  size_t position = specifier.find(slash);

  while (position != kNotFound) {
    positions.push_back(++position);
    position = specifier.find(slash, position);
  }

  Vector<AtomicString> result;
  for (size_t pos : positions) {
    result.push_back(specifier.GetString().Substring(0, pos));
  }

  return result;
}

}  // namespace

DocumentModulatorImpl::DocumentModulatorImpl(ScriptState* script_state)
    : ModulatorImplBase(script_state) {
  import_map_ = MakeGarbageCollected<ImportMap>();
}

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

// https://html.spec.whatwg.org/C/#merge-existing-and-new-import-maps
void DocumentModulatorImpl::MergeExistingAndNewImportMaps(
    ImportMap* new_import_map) {
  import_map_->MergeExistingAndNewImportMaps(
      new_import_map, scoped_resolved_module_map_,
      toplevel_resolved_module_set_, *GetExecutionContext());
}

// https://html.spec.whatwg.org/C#add-module-to-resolved-module-set
void DocumentModulatorImpl::AddModuleToResolvedModuleSet(
    std::optional<AtomicString> referring_script_url,
    AtomicString specifier) {
  // 1. Let global be settingsObject's global object.

  // 2. If global does not implement Window, then return.

  // 3. Let pair be a new referring script specifier pair, with referring script
  // set to referringScriptURL, and specifier set to specifier.

  // 4. Append pair to global's resolved module set.

  // We're using a different algorithm here where we find all the prefixes the
  // specifier has and add them to the top_level_resolved_module_set. We then
  // find all the prefixes that the referring script URL has, and add all the
  // prefixes to the sets of these referring prefixes in the
  // scoped_resolved_module_map.
  toplevel_resolved_module_set_.insert(specifier);
  Vector<AtomicString> specifier_prefixes = FindUrlPrefixes(specifier);
  for (const auto& specifier_prefix : specifier_prefixes) {
    toplevel_resolved_module_set_.insert(specifier_prefix);
  }

  if (!referring_script_url) {
    return;
  }
  Vector<AtomicString> referring_script_prefixes =
      FindUrlPrefixes(referring_script_url.value());
  for (const auto& referring_script_prefix : referring_script_prefixes) {
    const auto& current_set_it =
        scoped_resolved_module_map_.find(referring_script_prefix);
    HashSet<AtomicString>* current_set = nullptr;
    if (current_set_it != scoped_resolved_module_map_.end()) {
      current_set = &current_set_it->value;
    } else {
      current_set =
          &(scoped_resolved_module_map_
                .insert(referring_script_prefix, HashSet<AtomicString>())
                .stored_value->value);
    }
    current_set->insert(specifier);
    for (const auto& specifier_prefix : specifier_prefixes) {
      current_set->insert(specifier_prefix);
    }
  }
}

}  // namespace blink
