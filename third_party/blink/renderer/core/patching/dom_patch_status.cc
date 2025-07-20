// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/dom_patch_status.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/patching/patch_supplement.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
DOMPatchStatus::DOMPatchStatus(HTMLTemplateElement* source,
                               ContainerNode* target)
    : source_(source),
      target_(target),
      finished_(
          MakeGarbageCollected<ScriptPromiseProperty<IDLUndefined, IDLAny>>(
              target->GetDocument().GetExecutionContext())) {}

ScriptPromise<IDLUndefined> DOMPatchStatus::finished(
    ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void DOMPatchStatus::OnComplete() {
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
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
