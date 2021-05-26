// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter_operation_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_fe_convolve_matrix_element.h"

namespace blink {

namespace {
BlurFilterOperation* ResolveBlur(const Dictionary& blur_dict,
                                 ExceptionState& exception_state) {
  absl::optional<double> std_deviation =
      blur_dict.Get<IDLDouble>("stdDeviation", exception_state);
  if (!std_deviation) {
    exception_state.ThrowTypeError(
        "Failed to construct blur filter, 'stdDeviation' required and must be "
        "a number.");
    return nullptr;
  }

  return MakeGarbageCollected<BlurFilterOperation>(
      Length::Fixed(*std_deviation));
}

ColorMatrixFilterOperation* ResolveColorMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  absl::optional<Vector<float>> values =
      dict.Get<IDLSequence<IDLFloat>>("values", exception_state);

  if (!values) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' array required.");
    return nullptr;
  }

  if (values->size() != 20) {
    exception_state.ThrowTypeError(
        "Failed to construct color matrix filter, 'values' must be an array "
        "of 20 numbers.");
    return nullptr;
  }

  return MakeGarbageCollected<ColorMatrixFilterOperation>(
      *values, FilterOperation::COLOR_MATRIX);
}

struct KernelMatrix {
  Vector<float> values;
  uint32_t width;
  uint32_t height;
};

// For resolving feConvolveMatrix type filters
absl::optional<KernelMatrix> GetKernelMatrix(const Dictionary& dict,
                                             ExceptionState& exception_state) {
  absl::optional<Vector<Vector<float>>> km_input =
      dict.Get<IDLSequence<IDLSequence<IDLFloat>>>("kernelMatrix",
                                                   exception_state);
  if (!km_input || km_input->size() == 0) {
    exception_state.ThrowTypeError(
        "Failed to construct convolve matrix filter. 'kernelMatrix' must be an "
        "array of arrays of numbers representing an n by m matrix.");
    return absl::nullopt;
  }
  KernelMatrix result;
  result.height = km_input->size();
  result.width = km_input.value()[0].size();

  for (uint32_t y = 0; y < result.height; ++y) {
    if (km_input.value()[y].size() != result.width) {
      exception_state.ThrowTypeError(
          "Failed to construct convolve matrix filter. All rows of the "
          "'kernelMatrix' must be the same length.");
      return absl::nullopt;
    }

    result.values.AppendVector(km_input.value()[y]);
  }

  return absl::optional<KernelMatrix>(result);
}

ConvolveMatrixFilterOperation* ResolveConvolveMatrix(
    const Dictionary& dict,
    ExceptionState& exception_state) {
  absl::optional<KernelMatrix> kernel_matrix =
      GetKernelMatrix(dict, exception_state);

  if (!kernel_matrix)
    return nullptr;

  IntSize kernel_size(kernel_matrix->width, kernel_matrix->height);
  double divisor = dict.Get<IDLDouble>("divisor", exception_state).value_or(1);
  double bias = dict.Get<IDLDouble>("bias", exception_state).value_or(0);
  IntPoint target_offset =
      IntPoint(dict.Get<IDLShort>("targetX", exception_state)
                   .value_or(kernel_matrix->width / 2),
               dict.Get<IDLShort>("targetY", exception_state)
                   .value_or(kernel_matrix->height / 2));

  String edge_mode_string =
      dict.Get<IDLString>("edgeMode", exception_state).value_or("duplicate");
  FEConvolveMatrix::EdgeModeType edge_mode =
      static_cast<FEConvolveMatrix::EdgeModeType>(
          GetEnumerationMap<FEConvolveMatrix::EdgeModeType>().ValueFromName(
              edge_mode_string));

  bool preserve_alpha =
      dict.Get<IDLBoolean>("preserveAlpha", exception_state).value_or(false);

  return MakeGarbageCollected<ConvolveMatrixFilterOperation>(
      kernel_size, divisor, bias, target_offset, edge_mode, preserve_alpha,
      kernel_matrix->values);
}
}  // namespace

FilterOperations CanvasFilterOperationResolver::CreateFilterOperations(
    HeapVector<Member<CanvasFilterDictionary>> filters,
    ExceptionState& exception_state) {
  FilterOperations operations;

  for (auto filter : filters) {
    v8::Local<v8::Object> v8_object;
    if (filter->hasBlur()) {
      Dictionary blur_dict(filter->blur());
      if (auto* blur_operation = ResolveBlur(blur_dict, exception_state))
        operations.Operations().push_back(blur_operation);
    }
    if (filter->hasColorMatrix()) {
      Dictionary colormatrix_dict(filter->colorMatrix());
      String type = colormatrix_dict.Get<IDLString>("type", exception_state)
                        .value_or("matrix");
      if (type == "hueRotate") {
        double amount =
            colormatrix_dict.Get<IDLDouble>("values", exception_state)
                .value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::HUE_ROTATE));
      } else if (type == "saturate") {
        double amount =
            colormatrix_dict.Get<IDLDouble>("values", exception_state)
                .value_or(0);
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                amount, FilterOperation::SATURATE));
      } else if (type == "luminanceToAlpha") {
        operations.Operations().push_back(
            MakeGarbageCollected<BasicColorMatrixFilterOperation>(
                0, FilterOperation::LUMINANCE_TO_ALPHA));
      } else if (auto* color_matrix_operation =
                     ResolveColorMatrix(colormatrix_dict, exception_state)) {
        operations.Operations().push_back(color_matrix_operation);
      }
    }
    if (filter->hasConvolveMatrix()) {
      Dictionary convolve_dict(filter->convolveMatrix());
      if (auto* convolve_operation =
              ResolveConvolveMatrix(convolve_dict, exception_state)) {
        operations.Operations().push_back(convolve_operation);
      }
    }
  }

  return operations;
}

}  // namespace blink
