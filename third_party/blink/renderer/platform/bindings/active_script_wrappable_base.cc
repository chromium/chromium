// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

void ActiveScriptWrappableBase::TraceActiveScriptWrappables(
    v8::Isolate* isolate,
    Visitor* visitor) {
  V8PerIsolateData* isolate_data = V8PerIsolateData::From(isolate);
  const auto* active_script_wrappables = isolate_data->ActiveScriptWrappables();
  if (!active_script_wrappables)
    return;

  for (const auto& active_wrappable : *active_script_wrappables) {
    // Ignore objects that are currently under construction. They are kept alive
    // via conservative stack scan.
    HeapObjectHeader const* const header =
        active_wrappable->GetHeapObjectHeader();
    if ((header == BlinkGC::kNotFullyConstructedObject) ||
        header->IsInConstruction())
      continue;

    // A wrapper isn't kept alive after its ExecutionContext becomes detached,
    // even if |HasPendingActivity()| returns |true|. This measure avoids memory
    // leaks and has proven not to be too eager wrt garbage collection of
    // objects belonging to discarded browser contexts (
    // https://html.spec.whatwg.org/C/#a-browsing-context-is-discarded )
    //
    // Consequently, an implementation of |HasPendingActivity()| is not required
    // to take the detached state of the associated ExecutionContext into
    // account (i.e., return |false|.) We probe the detached state of the
    // ExecutionContext via |IsContextDestroyed()|.
    if (active_wrappable->IsContextDestroyed())
      continue;

    if (!active_wrappable->DispatchHasPendingActivity())
      continue;

    ScriptWrappable* script_wrappable = active_wrappable->ToScriptWrappable();
    visitor->Trace(script_wrappable);
  }
}

ActiveScriptWrappableBase::ActiveScriptWrappableBase() {
  DCHECK(ThreadState::Current());
  v8::Isolate* isolate = ThreadState::Current()->GetIsolate();
  V8PerIsolateData* isolate_data = V8PerIsolateData::From(isolate);
  isolate_data->AddActiveScriptWrappable(this);
}

}  // namespace blink
