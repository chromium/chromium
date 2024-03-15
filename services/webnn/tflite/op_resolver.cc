// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/op_resolver.h"

#include "third_party/tflite/buildflags.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/tflite/src/tensorflow/lite/tflite_with_xnnpack_optional.h"
#endif

namespace webnn::tflite {

OpResolver::OpResolver() {
  AddBuiltin(::tflite::BuiltinOperator_ABS,
             ::tflite::ops::builtin::Register_ABS());
  AddBuiltin(::tflite::BuiltinOperator_AVERAGE_POOL_2D,
             ::tflite::ops::builtin::Register_AVERAGE_POOL_2D(),
             /* min_version */ 1,
             /* max_version */ 3);
  AddBuiltin(::tflite::BuiltinOperator_CONCATENATION,
             ::tflite::ops::builtin::Register_CONCATENATION(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_CAST,
             ::tflite::ops::builtin::Register_CAST());
  AddBuiltin(::tflite::BuiltinOperator_ADD,
             ::tflite::ops::builtin::Register_ADD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_CEIL,
             ::tflite::ops::builtin::Register_CEIL());
  AddBuiltin(::tflite::BuiltinOperator_CONV_2D,
             ::tflite::ops::builtin::Register_CONV_2D(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(::tflite::BuiltinOperator_COS,
             ::tflite::ops::builtin::Register_COS());
  AddBuiltin(::tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
             ::tflite::ops::builtin::Register_DEPTHWISE_CONV_2D(),
             /* min_version = */ 1,
             /* max_version = */ 5);
  AddBuiltin(::tflite::BuiltinOperator_DIV,
             ::tflite::ops::builtin::Register_DIV(),
             /* min_version */ 1,
             /* max_version */ 2);
  AddBuiltin(::tflite::BuiltinOperator_ELU,
             ::tflite::ops::builtin::Register_ELU());
  AddBuiltin(::tflite::BuiltinOperator_EXP,
             ::tflite::ops::builtin::Register_EXP());
  AddBuiltin(::tflite::BuiltinOperator_FLOOR,
             ::tflite::ops::builtin::Register_FLOOR());
  AddBuiltin(::tflite::BuiltinOperator_FULLY_CONNECTED,
             ::tflite::ops::builtin::Register_FULLY_CONNECTED(),
             /* min_version = */ 1,
             /* max_version = */ 9);
  AddBuiltin(::tflite::BuiltinOperator_HARD_SWISH,
             ::tflite::ops::builtin::Register_HARD_SWISH());
  AddBuiltin(::tflite::BuiltinOperator_LEAKY_RELU,
             ::tflite::ops::builtin::Register_LEAKY_RELU(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_LOG,
             ::tflite::ops::builtin::Register_LOG());
  AddBuiltin(::tflite::BuiltinOperator_LOGISTIC,
             ::tflite::ops::builtin::Register_LOGISTIC(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_MAX_POOL_2D,
             ::tflite::ops::builtin::Register_MAX_POOL_2D(),
             /* min_version */ 1,
             /* max_version */ 3);
  AddBuiltin(::tflite::BuiltinOperator_MAXIMUM,
             ::tflite::ops::builtin::Register_MAXIMUM(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(::tflite::BuiltinOperator_MINIMUM,
             ::tflite::ops::builtin::Register_MINIMUM(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(::tflite::BuiltinOperator_MIRROR_PAD,
             ::tflite::ops::builtin::Register_MIRROR_PAD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_MUL,
             ::tflite::ops::builtin::Register_MUL(),
             /* min_version = */ 1,
             /* max_version = */ 4);
  AddBuiltin(::tflite::BuiltinOperator_NEG,
             ::tflite::ops::builtin::Register_NEG());
  AddBuiltin(::tflite::BuiltinOperator_PAD,
             ::tflite::ops::builtin::Register_PAD(),
             /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_PADV2,
             ::tflite::ops::builtin::Register_PADV2(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_POW,
             ::tflite::ops::builtin::Register_POW());
  AddBuiltin(::tflite::BuiltinOperator_RELU,
             ::tflite::ops::builtin::Register_RELU(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_RELU_N1_TO_1,
             ::tflite::ops::builtin::Register_RELU_N1_TO_1());
  AddBuiltin(::tflite::BuiltinOperator_RELU6,
             ::tflite::ops::builtin::Register_RELU6(), /* min_version = */ 1,
             /* max_version = */ 2);
  AddBuiltin(::tflite::BuiltinOperator_RESHAPE,
             ::tflite::ops::builtin::Register_RESHAPE());
  AddBuiltin(::tflite::BuiltinOperator_RESIZE_BILINEAR,
             ::tflite::ops::builtin::Register_RESIZE_BILINEAR(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR,
             ::tflite::ops::builtin::Register_RESIZE_NEAREST_NEIGHBOR(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_SIN,
             ::tflite::ops::builtin::Register_SIN());
  AddBuiltin(::tflite::BuiltinOperator_SLICE,
             ::tflite::ops::builtin::Register_SLICE(),
             /* min_version = */ 1,
             /* max_version = */ 6);
  AddBuiltin(::tflite::BuiltinOperator_SOFTMAX,
             ::tflite::ops::builtin::Register_SOFTMAX(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_SQRT,
             ::tflite::ops::builtin::Register_SQRT());
  AddBuiltin(::tflite::BuiltinOperator_SUB,
             ::tflite::ops::builtin::Register_SUB(),
             /* min_version = */ 1,
             /* max_version = */ 3);
  AddBuiltin(::tflite::BuiltinOperator_TRANSPOSE,
             ::tflite::ops::builtin::Register_TRANSPOSE(),
             /* min_version = */ 1,
             /* max_version = */ 4);

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  delegate_creators_.push_back([](TfLiteContext* context) {
    return ::tflite::MaybeCreateXNNPACKDelegate(
        context, ::tflite::XNNPackQS8Options::default_value);
  });
#endif
}

}  // namespace webnn::tflite
