// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/dom_patch_status.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/patching/patch_event.h"
#include "third_party/blink/renderer/core/patching/patch_supplement.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
// static
DOMPatchStatus* DOMPatchStatus::Start(HTMLTemplateElement& source,
                                      ContainerNode& target) {
  // A patch replaces the existing children of the target.
  target.RemoveChildren();
  DOMPatchStatus* patch = MakeGarbageCollected<DOMPatchStatus>(source, target);
  MutationObserver::EnqueuePatch(*patch);
  PatchSupplement::From(target.GetDocument())->DidStart(target, patch);
  return patch;
}

DOMPatchStatus::DOMPatchStatus(HTMLTemplateElement& source,
                               ContainerNode& target)
    : source_(source),
      target_(target),
      finished_(
          MakeGarbageCollected<ScriptPromiseProperty<IDLUndefined, IDLAny>>(
              target.GetDocument().GetExecutionContext())),
      parser_(MakeGarbageCollected<HTMLDocumentParser>(
          target_,
          target.IsElementNode() ? &To<Element>(target)
                                 : target.parentElement(),
          ParserContentPolicy::kDisallowScriptingAndPluginContent)) {}

ScriptPromise<IDLUndefined> DOMPatchStatus::finished(
    ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void DOMPatchStatus::DispatchPatchEvent() {
  Event* event =
      MakeGarbageCollected<PatchEvent>(event_type_names::kPatch, this);
  event->SetTarget(target_);
  target_->DispatchEvent(*event);
}

void DOMPatchStatus::Finish() {
  parser_->Finish();
  finished_->ResolveWithUndefined();
  PatchSupplement::From(GetDocument())->DidComplete(*target_);
}

Document& DOMPatchStatus::GetDocument() {
  return target_->GetDocument();
}

void DOMPatchStatus::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(target_);
  visitor->Trace(finished_);
  visitor->Trace(parser_);
  ScriptWrappable::Trace(visitor);
}

void DOMPatchStatus::Append(const String& text) {
  parser_->Append(text);
}

}  // namespace blink
