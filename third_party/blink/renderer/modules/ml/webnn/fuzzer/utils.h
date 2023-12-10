// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_FUZZER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_FUZZER_UTILS_H_

#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/webnn.pb.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class ScopedClosureRunner;
}

namespace blink {

V8MLOperandDataType::Enum ToV8MLOperandDataType(
    webnn_proto::MLOperandDataType data_type);

V8MLAutoPad::Enum ToV8MLAutoPad(webnn_proto::MLAutoPad auto_pad);

V8MLInputOperandLayout::Enum ToV8MLInputOperandLayout(
    webnn_proto::MLInputOperandLayout input_layout);

template <typename T>
Vector<T> RepeatedFieldToVector(
    const ::google::protobuf::RepeatedField<T>& repeated_field) {
  Vector<T> result;
  for (auto& field : repeated_field) {
    result.push_back(field);
  }
  return result;
}

base::ScopedClosureRunner MakeScopedGarbageCollectionRequest(v8::Isolate*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_FUZZER_UTILS_H_
