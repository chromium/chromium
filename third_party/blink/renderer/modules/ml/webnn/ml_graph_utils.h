// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_

#include <utility>
#include <vector>

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class MLOperator;

// Return the operators in topological order by searching from the named
// output operands. It ensures operator 'j' appears before operator 'i' in the
// result, if 'i' depends on 'j'.
MODULES_EXPORT HeapVector<Member<const MLOperator>>*
GetOperatorsInTopologicalOrder(const MLNamedOperands& named_outputs);

// Stores information about a transferred `ArrayBufferView`. This struct doesn't
// include Blink GC objects, and can be accessed by any threads.
//
// The information is used to recreate `ArrayBufferView` when computation
// completes.
struct ArrayBufferViewInfo {
  ArrayBufferViewInfo() = default;
  ~ArrayBufferViewInfo() = default;

  ArrayBufferViewInfo(ArrayBufferViewInfo&& other) = default;
  ArrayBufferViewInfo& operator=(ArrayBufferViewInfo&& other) = default;

  ArrayBufferViewInfo(const ArrayBufferViewInfo&) = delete;
  ArrayBufferViewInfo& operator=(const ArrayBufferViewInfo&) = delete;

  DOMArrayBufferView::ViewType type;
  size_t offset;
  size_t length;
  ArrayBufferContents contents;
};

// `TransferNamedArrayBufferViews()` and `CreateNamedArrayBufferViews()`
// implement the MLNamedArrayBufferViews transfer algorithm of WebNN spec:
// https://www.w3.org/TR/webnn/#mlnamedarraybufferviews-transfer
//
// The `NamedArrayBufferViewsInfo` returned by `TransferNamedArrayBufferViews()`
// doesn't contain any GC objects, so it is safe to be posted to a background
// thread that invokes the XNNPACK Runtime. After that,
// `NamedArrayBufferViewsInfo` should be posted back to the calling thread and
// call `CreateNamedArrayBufferViews()` to create `MLNamedArrayBufferViews` from
// the info.
//
// If it fails to transfer an `ArrayBufferView` of the
// `MLNamedArrayBufferViews`, the current implementation leaves the
// already-transferred views detached, the failing one and remaining others
// unchanged.
//
// TODO(crbug.com/1273291): Revisit the error handling once the WebNN spec issue
// is resolved: https://github.com/webmachinelearning/webnn/issues/351
std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>>
TransferNamedArrayBufferViews(v8::Isolate* isolate,
                              const MLNamedArrayBufferViews& source_views,
                              ExceptionState& exception_state);

MLNamedArrayBufferViews* CreateNamedArrayBufferViews(
    std::unique_ptr<Vector<std::pair<String, ArrayBufferViewInfo>>> views_info);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
