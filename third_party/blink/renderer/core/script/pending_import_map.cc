// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/pending_import_map.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

PendingImportMap* PendingImportMap::CreateInline(ScriptElementBase& element,
                                                 const String& import_map_text,
                                                 const KURL& base_url) {
  Document& element_document = element.GetDocument();
  Document* context_document = element_document.ContextDocument();
  ScriptState* script_state =
      ToScriptStateForMainWorld(context_document->GetFrame());
  Modulator* modulator = Modulator::From(script_state);

  ScriptValue error_to_rethrow;
  ImportMap* import_map =
      ImportMap::Parse(*modulator, import_map_text, base_url,
                       modulator->BuiltInModuleInfraEnabled(),
                       *context_document, &error_to_rethrow);
  return MakeGarbageCollected<PendingImportMap>(
      script_state, element, import_map, error_to_rethrow, *context_document);
}

PendingImportMap::PendingImportMap(ScriptState* script_state,
                                   ScriptElementBase& element,
                                   ImportMap* import_map,
                                   ScriptValue error_to_rethrow,
                                   const Document& original_context_document)
    : element_(&element),
      import_map_(import_map),
      original_context_document_(&original_context_document) {
  if (!error_to_rethrow.IsEmpty()) {
    ScriptState::Scope scope(script_state);
    error_to_rethrow_.Set(script_state->GetIsolate(),
                          error_to_rethrow.V8Value());
  }
}

// <specdef href="https://wicg.github.io/import-maps/#register-an-import-map">
// This is parallel to PendingScript::ExecuteScriptBlock().
void PendingImportMap::RegisterImportMap() const {
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
  Document* context_document = element_->GetDocument().ContextDocument();
  if (!context_document)
    return;
  if (original_context_document_ != context_document)
    return;

  // Steps 7 and 8.
  LocalFrame* frame = context_document->GetFrame();
  if (!frame)
    return;

  Modulator* modulator = Modulator::From(ToScriptStateForMainWorld(frame));

  ScriptState* script_state = modulator->GetScriptState();
  ScriptState::Scope scope(script_state);
  ScriptValue error;
  if (!error_to_rethrow_.IsEmpty()) {
    error = ScriptValue(script_state->GetIsolate(),
                        error_to_rethrow_.Get(script_state));
  }
  modulator->RegisterImportMap(import_map_, error);

  // <spec step="9">If element is from an external file, then fire an event
  // named load at element.</spec>
  //
  // TODO(hiroshige): Implement this when external import maps are implemented.
}

void PendingImportMap::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(import_map_);
  visitor->Trace(error_to_rethrow_);
  visitor->Trace(original_context_document_);
}

}  // namespace blink
