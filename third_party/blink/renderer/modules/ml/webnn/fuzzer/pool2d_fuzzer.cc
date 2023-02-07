// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/webnn.pb.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

namespace blink {

namespace {

V8MLRoundingType::Enum ToV8MLRoundingType(
    webnn_proto::MLRoundingType rounding_type) {
  switch (rounding_type) {
    case webnn_proto::MLRoundingType::FLOOR:
      return V8MLRoundingType::Enum::kFloor;
    case webnn_proto::MLRoundingType::CEIL:
      return V8MLRoundingType::Enum::kCeil;
  }
}

void ProtobufToPool2dOptions(const webnn_proto::pool2dOptions& data,
                             MLPool2dOptions* options) {
  if (data.windows_dimensions_size() > 0) {
    options->setWindowDimensions(Vector<uint32_t>(data.windows_dimensions()));
  }
  if (data.padding_size() > 0) {
    options->setPadding(Vector<uint32_t>(data.padding()));
  }

  if (data.strides_size() > 0) {
    options->setStrides(Vector<uint32_t>(data.strides()));
  }

  if (data.dilations_size() > 0) {
    options->setDilations(Vector<uint32_t>(data.dilations()));
  }

  if (data.has_auto_pad()) {
    options->setAutoPad(ToV8MLAutoPad(data.auto_pad()));
  }

  if (data.has_layout()) {
    options->setLayout(ToV8MLInputOperandLayout(data.layout()));
  }

  if (data.has_rounding_type()) {
    options->setRoundingType(ToV8MLRoundingType(data.rounding_type()));
  }

  if (data.output_size_size() > 0) {
    options->setOutputSizes(Vector<uint32_t>(data.output_size()));
  }
}
}  // namespace

DEFINE_PROTO_FUZZER(const webnn_proto::pool2d& pool2d) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

  auto* builder = CreateMLGraphBuilder(
      page_holder->GetFrame().DomWindow()->GetExecutionContext());

  DummyExceptionStateForTesting exception_state;
  auto* input =
      BuildInput(builder, "input", Vector<uint32_t>(pool2d.input_dimensions()),
                 ToV8MLOperandType(pool2d.input_type()), exception_state);
  auto* pool2d_options = blink::MLPool2dOptions::Create();
  if (pool2d.has_pool2d_options()) {
    ProtobufToPool2dOptions(pool2d.pool2d_options(), pool2d_options);
  }

  if (input == nullptr) {
    return;
  }
  switch (pool2d.pool2d_kind()) {
    case webnn_proto::MLPool2dKind::AVERAGE:
      builder->averagePool2d(input, pool2d_options, exception_state);
      break;
    case webnn_proto::MLPool2dKind::MAX:
      builder->maxPool2d(input, pool2d_options, exception_state);
      break;
  }

  V8PerIsolateData::MainThreadIsolate()->RequestGarbageCollectionForTesting(
      v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
