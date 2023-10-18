// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/fuzzer/webnn.pb.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

namespace blink {

namespace {

V8MLConv2dFilterOperandLayout::Enum ToV8MLFilterOperandLayout(
    webnn_proto::MLConv2dFilterOperandLayout filter_layout) {
  switch (filter_layout) {
    case webnn_proto::MLConv2dFilterOperandLayout::HWIO:
      return V8MLConv2dFilterOperandLayout::Enum::kHwio;
    case webnn_proto::MLConv2dFilterOperandLayout::IHWO:
      return V8MLConv2dFilterOperandLayout::Enum::kIhwo;
    case webnn_proto::MLConv2dFilterOperandLayout::OHWI:
      return V8MLConv2dFilterOperandLayout::Enum::kOhwi;
    case webnn_proto::MLConv2dFilterOperandLayout::OIHW:
      return V8MLConv2dFilterOperandLayout::Enum::kOihw;
  }
}

void ProtobufToConv2dOptions(const webnn_proto::conv2dOptions& data,
                             MLConv2dOptions* options) {
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

  if (data.has_groups()) {
    options->setGroups(data.groups());
  }

  if (data.has_input_layout()) {
    options->setInputLayout(ToV8MLInputOperandLayout(data.input_layout()));
  }

  if (data.has_filter_layout()) {
    options->setFilterLayout(ToV8MLFilterOperandLayout(data.filter_layout()));
  }
}
}  // namespace

DEFINE_PROTO_FUZZER(const webnn_proto::conv2d& conv2d) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* page_holder = []() {
    auto page_holder = std::make_unique<DummyPageHolder>();
    page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    return page_holder.release();
  }();

  ScriptState* script_state =
      ToScriptStateForMainWorld(&page_holder->GetFrame());

  DummyExceptionStateForTesting exception_state;
  auto* builder = CreateMLGraphBuilder(
      page_holder->GetFrame().DomWindow()->GetExecutionContext(), script_state,
      exception_state);
  CHECK(builder);

  auto* input =
      BuildInput(builder, "input", Vector<uint32_t>(conv2d.input_dimensions()),
                 ToV8MLOperandType(conv2d.input_type()), exception_state);
  auto* filter =
      BuildConstant(builder, Vector<uint32_t>(conv2d.filter_dimensions()),
                    ToV8MLOperandType(conv2d.filter_type()), exception_state);
  auto* conv2d_options = blink::MLConv2dOptions::Create();
  if (conv2d.has_conv2d_options()) {
    ProtobufToConv2dOptions(conv2d.conv2d_options(), conv2d_options);
  }

  if (input == nullptr || filter == nullptr) {
    return;
  }
  builder->conv2d(input, filter, conv2d_options, exception_state);

  page_holder->GetPage()
      ->GetAgentGroupScheduler()
      ->Isolate()
      ->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
}

}  // namespace blink
