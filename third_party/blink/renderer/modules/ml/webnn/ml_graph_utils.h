// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_

#include <utility>
#include <vector>

#include "components/ml/webnn/graph_validation_utils.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_auto_pad.h"
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

webnn::AutoPad BlinkAutoPadToComponent(blink::V8MLAutoPad::Enum type);

// Create a default permutation vector [rank - 1, ..., 0].
Vector<uint32_t> CreateDefaultPermutation(const wtf_size_t rank);

// Helper to get padding sizes for convolution 2d or pooling 2d Nodes.
template <typename OptionsType>
webnn::Padding2d CalculatePadding2D(const OptionsType* options,
                                    uint32_t input_height,
                                    uint32_t input_width,
                                    uint32_t filter_height,
                                    uint32_t filter_width,
                                    uint32_t stride_height,
                                    uint32_t stride_width,
                                    uint32_t dilation_height,
                                    uint32_t dilation_width) {
  webnn::Padding2d padding;
  switch (options->autoPad().AsEnum()) {
    case V8MLAutoPad::Enum::kExplicit: {
      // Set the padding from WebNN explicit padding that is in
      // [beginning_height, ending_height, beginning_width, ending_width],
      // default to 0.
      auto ml_padding = options->getPaddingOr({0, 0, 0, 0});
      CHECK_EQ(ml_padding.size(), 4u);
      padding.beginning.height = ml_padding[0];
      padding.ending.height = ml_padding[1];
      padding.beginning.width = ml_padding[2];
      padding.ending.width = ml_padding[3];
      break;
    }
    case V8MLAutoPad::Enum::kSameUpper:
    case V8MLAutoPad::Enum::kSameLower: {
      webnn::AutoPad auto_pad =
          BlinkAutoPadToComponent(options->autoPad().AsEnum());
      // Calculate padding based on WebNN auto padding mode and sizes.
      auto padding_sizes_height =
          webnn::CalculateConv2dPadding(auto_pad, input_height, filter_height,
                                        stride_height, dilation_height);
      CHECK(padding_sizes_height);
      padding.beginning.height = padding_sizes_height.value().begin;
      padding.ending.height = padding_sizes_height.value().end;
      auto padding_sizes_width = webnn::CalculateConv2dPadding(
          auto_pad, input_width, filter_width, stride_width, dilation_width);
      CHECK(padding_sizes_width);
      padding.beginning.width = padding_sizes_width.value().begin;
      padding.ending.width = padding_sizes_width.value().end;
      break;
    }
  }
  return padding;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_UTILS_H_
