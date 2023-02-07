// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/utils.h"

namespace blink {

V8MLOperandType::Enum ToV8MLOperandType(webnn_proto::MLOperandType type) {
  switch (type) {
    case webnn_proto::MLOperandType::FLOAT32:
      return V8MLOperandType::Enum::kFloat32;
    case webnn_proto::MLOperandType::FLOAT16:
      return V8MLOperandType::Enum::kFloat16;
    case webnn_proto::MLOperandType::INT32:
      return V8MLOperandType::Enum::kInt32;
    case webnn_proto::MLOperandType::UINT32:
      return V8MLOperandType::Enum::kUint32;
    case webnn_proto::MLOperandType::INT8:
      return V8MLOperandType::Enum::kInt8;
    case webnn_proto::MLOperandType::UINT8:
      return V8MLOperandType::Enum::kUint8;
  }
}

V8MLAutoPad::Enum ToV8MLAutoPad(webnn_proto::MLAutoPad auto_pad) {
  switch (auto_pad) {
    case webnn_proto::MLAutoPad::EXPLICIT:
      return V8MLAutoPad::Enum::kExplicit;
    case webnn_proto::MLAutoPad::SAME_UPPER:
      return V8MLAutoPad::Enum::kSameUpper;
    case webnn_proto::MLAutoPad::SAME_LOWER:
      return V8MLAutoPad::Enum::kSameLower;
  }
}

V8MLInputOperandLayout::Enum ToV8MLInputOperandLayout(
    webnn_proto::MLInputOperandLayout input_layout) {
  switch (input_layout) {
    case webnn_proto::MLInputOperandLayout::NCHW:
      return V8MLInputOperandLayout::Enum::kNchw;
    case webnn_proto::MLInputOperandLayout::NHWC:
      return V8MLInputOperandLayout::Enum::kNhwc;
  }
}

}  // namespace blink
