// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"

#include <utility>

#include "base/check_deref.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

CanvasFilter::CanvasFilter(FilterOperations filter_operations)
    : filter_operations_(std::move(filter_operations)) {}

CanvasFilter* CanvasFilter::Create(ScriptState* script_state,
                                   const V8CanvasFilterInput* init,
                                   ExceptionState& exception_state) {
  Font font_for_filter = Font();
  return MakeGarbageCollected<CanvasFilter>(CreateFilterOperations(
      CHECK_DEREF(init), font_for_filter, nullptr,
      CHECK_DEREF(ExecutionContext::From(script_state)), exception_state));
}

FilterOperations CanvasFilter::CreateFilterOperations(
    const V8CanvasFilterInput& filter_input,
    const Font& font,
    Element* style_resolution_host,
    ExecutionContext& execution_context,
    ExceptionState& exception_state) {
  switch (filter_input.GetContentType()) {
    case V8CanvasFilterInput::ContentType::kString:
      return CanvasFilterOperationResolver::CreateFilterOperationsFromCSSFilter(
          filter_input.GetAsString(), execution_context, style_resolution_host,
          font);
    case V8CanvasFilterInput::ContentType::kObjectArray:
      return CanvasFilterOperationResolver::CreateFilterOperationsFromList(
          filter_input.GetAsObjectArray(), execution_context, exception_state);
    case V8CanvasFilterInput::ContentType::kObject:
      return CanvasFilterOperationResolver::CreateFilterOperationsFromList(
          {filter_input.GetAsObject()}, execution_context, exception_state);
  }
  return FilterOperations();
}

void CanvasFilter::Trace(Visitor* visitor) const {
  visitor->Trace(filter_operations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
