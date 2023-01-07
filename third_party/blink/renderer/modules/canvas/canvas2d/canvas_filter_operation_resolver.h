// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_OPERATION_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_OPERATION_RESOLVER_H_

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_filter_dictionary.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// Similar to
// third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h but
// the input is a from a canvas filter object instead of a CSSValue.
// CanvasFilters are created in javascript by passing in dictionaries like so:
//  ctx.filter = new CanvasFilter({filter: "gaussianBlur", stdDeviation: 5});
// This class resolves these inputs into FilterOperations that can be used by
// CanvasRenderingContext2DState's GetFilter() functions.
class MODULES_EXPORT CanvasFilterOperationResolver {
  STATIC_ONLY(CanvasFilterOperationResolver);

 public:
  static FilterOperations CreateFilterOperations(
      ExecutionContext* execution_context,
      HeapVector<ScriptValue>,
      ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_OPERATION_RESOLVER_H_
