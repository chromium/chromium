// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;

CanvasFilter::CanvasFilter(FilterOperations filter_operations)
    : filter_operations_(std::move(filter_operations)) {}

CanvasFilter* CanvasFilter::Create(ExecutionContext* execution_context,
                                   const V8CanvasFilterInput* init,
                                   ExceptionState& exception_state) {
  CHECK(execution_context);
  CHECK(init);
  return MakeGarbageCollected<CanvasFilter>(
      CanvasFilterOperationResolver::CreateFilterOperations(
          *init, *execution_context, exception_state));
}

void CanvasFilter::Trace(Visitor* visitor) const {
  visitor->Trace(filter_operations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
