// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/utils.h"
#include "base/functional/callback_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

V8MLOperandDataType::Enum ToV8MLOperandDataType(
    webnn_proto::MLOperandDataType data_type) {
  switch (data_type) {
    case webnn_proto::MLOperandDataType::FLOAT32:
      return V8MLOperandDataType::Enum::kFloat32;
    case webnn_proto::MLOperandDataType::FLOAT16:
      return V8MLOperandDataType::Enum::kFloat16;
    case webnn_proto::MLOperandDataType::INT32:
      return V8MLOperandDataType::Enum::kInt32;
    case webnn_proto::MLOperandDataType::UINT32:
      return V8MLOperandDataType::Enum::kUint32;
    case webnn_proto::MLOperandDataType::INT64:
      return V8MLOperandDataType::Enum::kInt64;
    case webnn_proto::MLOperandDataType::UINT64:
      return V8MLOperandDataType::Enum::kUint64;
    case webnn_proto::MLOperandDataType::INT8:
      return V8MLOperandDataType::Enum::kInt8;
    case webnn_proto::MLOperandDataType::UINT8:
      return V8MLOperandDataType::Enum::kUint8;
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

base::ScopedClosureRunner MakeScopedGarbageCollectionRequest(
    v8::Isolate* isolate) {
  return base::ScopedClosureRunner(WTF::BindOnce(
      [](v8::Isolate* isolate) {
        // Request a V8 GC. Oilpan will be invoked by the GC epilogue.
        //
        // Multiple GCs may be required to ensure everything is collected (due
        // to a chain of persistent handles), so some objects may not be
        // collected until a subsequent iteration. This is slow enough as is, so
        // we compromise on one major GC, as opposed to the 5 used in
        // V8GCController for unit tests.
        isolate->RequestGarbageCollectionForTesting(
            v8::Isolate::kFullGarbageCollection);
      },
      WTF::Unretained(isolate)));
}

}  // namespace blink
