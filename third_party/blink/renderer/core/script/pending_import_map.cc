// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/pending_import_map.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

PendingImportMap* PendingImportMap::CreateInline(ScriptElementBase& element,
                                                 const String& import_map_text,
                                                 const KURL& base_url) {
  ExecutionContext* context = element.GetExecutionContext();

  std::optional<ImportMapError> error_to_rethrow;
  ImportMap* import_map =
      ImportMap::Parse(import_map_text, base_url, *context, &error_to_rethrow);
  return MakeGarbageCollected<PendingImportMap>(
      element, import_map, std::move(error_to_rethrow), *context);
}

PendingImportMap::PendingImportMap(
    ScriptElementBase& element,
    ImportMap* import_map,
    std::optional<ImportMapError> error_to_rethrow,
    const ExecutionContext& original_context)
    : element_(&element),
      import_map_(import_map),
      error_to_rethrow_(std::move(error_to_rethrow)),
      original_execution_context_(&original_context) {}

// <specdef
// href="https://html.spec.whatwg.org/C#register-an-import-map"> This is
// parallel to PendingScript::ExecuteScriptBlock().
void PendingImportMap::RegisterImportMap() {
  CHECK(import_map_);

  // TODO(crbug.com/364917757): This step is no longer in the spec, and it's not
  // clear when this can actually happen.
  //
  // <spec step="?">If element’s node document’s relevant settings
  // object is not equal to settings object, then return. ...</spec>
  ExecutionContext* context = element_->GetExecutionContext();
  if (original_execution_context_ != context)
    return;

  Modulator* modulator = Modulator::From(
      ToScriptStateForMainWorld(To<LocalDOMWindow>(context)->GetFrame()));
  if (!modulator)
    return;

  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);

  // <spec step="1">If result's error to rethrow is not null, then report an
  // exception given by result's error to rethrow for global and return.</spec>
  if (error_to_rethrow_.has_value()) {
    if (ExecutionContext::From(script_state)
            ->CanExecuteScripts(kAboutToExecuteScript)) {
      ModuleRecord::ReportException(script_state,
                                    error_to_rethrow_->ToV8(script_state));
    }
    return;
  }

  // <spec step="2">Merge existing and new import maps, given global and
  // result's import map.</spec>
  modulator->MergeExistingAndNewImportMaps(import_map_);
}

void PendingImportMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(import_map_);
  visitor->Trace(original_execution_context_);
}

}  // namespace blink
