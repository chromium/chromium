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
  // <spec step="1">If element’s the script’s result is null, then fire an event
  // named error at element, and return.</spec>
  if (!import_map_) {
    element_->DispatchErrorEvent();
    return;
  }

  // <spec step="2">Let import map parse result be element’s the script’s
  // result.</spec>
  //
  // This is |this|.

  // <spec step="3">Assert: element’s the script’s type is "importmap".</spec>
  //
  // <spec step="4">Assert: import map parse result is an import map parse
  // result.</spec>
  //
  // These are ensured by C++ type.

  // <spec step="5">Let settings object be import map parse result’s settings
  // object.</spec>
  //
  // <spec step="6">If element’s node document’s relevant settings object is not
  // equal to settings object, then return. ...</spec>
  ExecutionContext* context = element_->GetExecutionContext();
  if (original_execution_context_ != context)
    return;

  // Steps 7 and 8.
  Modulator* modulator = Modulator::From(
      ToScriptStateForMainWorld(To<LocalDOMWindow>(context)->GetFrame()));
  if (!modulator)
    return;

  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);

  // <spec step="7">If import map parse result’s error to rethrow is not null,
  // then:</spec>
  if (error_to_rethrow_.has_value()) {
    // <spec step="7.1">Report the exception given import map parse result’s
    // error to rethrow. ...</spec>
    if (ExecutionContext::From(script_state)
            ->CanExecuteScripts(kAboutToExecuteScript)) {
      ModuleRecord::ReportException(script_state,
                                    error_to_rethrow_->ToV8(script_state));
    }

    // <spec step="7.2">Return.</spec>
    return;
  }

  // <spec step="8">Update element’s node document's import map with import map
  // parse result’s import map.</spec>
  //
  // TODO(crbug.com/927119): Implement merging. Currently only one import map
  // is allowed.
  modulator->SetImportMap(import_map_);

  // <spec step="9">If element is from an external file, then fire an event
  // named load at element.</spec>
  //
  // TODO(hiroshige): Implement this when external import maps are implemented.
}

void PendingImportMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(import_map_);
  visitor->Trace(original_execution_context_);
}

}  // namespace blink
