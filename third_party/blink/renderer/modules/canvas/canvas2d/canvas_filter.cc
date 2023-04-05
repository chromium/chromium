// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;

CanvasFilter::CanvasFilter(FilterOperations filter_operations)
    : filter_operations_(filter_operations) {}

CanvasFilter* CanvasFilter::Create(ExecutionContext* execution_context,
                                   const V8CanvasFilterInput* init,
                                   ExceptionState& exception_state) {
  HeapVector<ScriptValue> filter_array;

  switch (init->GetContentType()) {
    case V8CanvasFilterInput::ContentType::kObject:
      filter_array.push_back(init->GetAsObject());
      break;
    case V8CanvasFilterInput::ContentType::kObjectArray:
      filter_array = init->GetAsObjectArray();
      break;
  }

  FilterOperations filter_operations =
      CanvasFilterOperationResolver::CreateFilterOperations(
          execution_context, filter_array, exception_state);

  return MakeGarbageCollected<CanvasFilter>(filter_operations);
}

void CanvasFilter::Trace(Visitor* visitor) const {
  visitor->Trace(filter_operations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
