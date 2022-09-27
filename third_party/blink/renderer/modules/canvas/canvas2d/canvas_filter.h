// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_H_

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_objectarray.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
// class V8UnionCanvasFilterDictionaryOrCanvasFilterDictionaryArray;

// This class stores an unresolved filter on CanvasRenderingContext2DState that
// has been created from the CanvasFilter javascript object. It will be parsed
// into FilterOperations by the CanvasFilterOperationResolver upon creation.
class MODULES_EXPORT CanvasFilter final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CanvasFilter* Create(ExecutionContext* execution_context,
                              const V8CanvasFilterInput* init,
                              ExceptionState& exception_state);

  explicit CanvasFilter(FilterOperations filter_operations);

  const FilterOperations& Operations() const { return filter_operations_; }

  void Trace(Visitor* visitor) const override;

 private:
  FilterOperations filter_operations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FILTER_H_
