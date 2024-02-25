// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for the Sanitizer API, intended to be run on ClusterFuzz.
//
// To test out locally:
// - Assuming
//     $OUT is your local build output directory with use_libbfuzzer set.
//     $GEN is the suitable 'gen' directory under $OUT.
//
// - Build:
//   $ ninja -C $OUT sanitizer_api_fuzzer
// - Run with:
//   $ GEN=$OUT/gen/third_party/blink/renderer/modules/sanitizer_api
//   $ $OUT/sanitizer_api_fuzzer --dict=$GEN/sanitizer_api.dict \
//       $(mktemp -d) $GEN/corpus/

#include "sanitizer.h"

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config.pb.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

ScriptState* Initialization() {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  static DummyPageHolder* g_page_holder = new DummyPageHolder();
  return ToScriptStateForMainWorld(&g_page_holder->GetFrame());
}

Vector<String> ToVector(
    const google::protobuf::RepeatedPtrField<std::string>& inputs) {
  Vector<String> elements;
  for (auto i : inputs) {
    elements.push_back(i.c_str());
  }
  return elements;
}

void MakeConfiguration(SanitizerConfig* sanitizer_config,
                       const SanitizerConfigProto& proto) {
  if (proto.allow_elements_size()) {
    sanitizer_config->setAllowElements(ToVector(proto.allow_elements()));
  }
  if (proto.block_elements_size()) {
    sanitizer_config->setBlockElements(ToVector(proto.block_elements()));
  }
  if (proto.drop_elements_size()) {
    sanitizer_config->setDropElements(ToVector(proto.drop_elements()));
  }
  if (proto.allow_attributes_size()) {
    Vector<std::pair<String, Vector<String>>> allow_attributes;
    for (auto i : proto.allow_attributes()) {
      allow_attributes.push_back(std::pair<String, Vector<String>>(
          i.first.c_str(), ToVector(i.second.element())));
    }
    sanitizer_config->setAllowAttributes(allow_attributes);
  }
  if (proto.drop_attributes_size()) {
    Vector<std::pair<String, Vector<String>>> drop_attributes;
    for (auto i : proto.drop_attributes()) {
      drop_attributes.push_back(std::pair<String, Vector<String>>(
          i.first.c_str(), ToVector(i.second.element())));
    }
    sanitizer_config->setDropAttributes(drop_attributes);
  }
  sanitizer_config->setAllowCustomElements(proto.allow_custom_elements());
  sanitizer_config->setAllowComments(proto.allow_comments());
}

void TextProtoFuzzer(const SanitizerConfigProto& proto,
                     ScriptState* script_state) {
  // Create Sanitizer based on proto's config..
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  auto* sanitizer_config = MakeGarbageCollected<SanitizerConfig>();
  MakeConfiguration(sanitizer_config, proto);
  auto* sanitizer = MakeGarbageCollected<Sanitizer>(
      window->GetExecutionContext(), sanitizer_config);

  // Sanitize string given in proto. Use proto.string_context to decide on
  // parsing context for sanitizeFor.
  // TODO(1225606): This needs to be updated to also support SVG & MathML
  // contexts, once those are implemented.
  String markup = proto.html_string().c_str();
  const char* string_context = nullptr;
  switch (proto.string_context()) {
    case SanitizerConfigProto::DIV:
      string_context = "div";
      break;
    case SanitizerConfigProto::TABLE:
      string_context = "table";
      break;
    case SanitizerConfigProto::TEMPLATE:
    default:
      string_context = "template";
      break;
  }
  sanitizer->sanitizeFor(script_state, string_context, markup,
                         IGNORE_EXCEPTION_FOR_TESTING);

  // The fuzzer will eventually run out of memory. Force the GC to run every
  // N-th time. This will trigger both V8 + Oilpan GC.
  static size_t counter = 0;
  if (counter++ > 1000) {
    counter = 0;
    script_state->GetIsolate()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }
}

}  // namespace blink

DEFINE_TEXT_PROTO_FUZZER(const SanitizerConfigProto& proto) {
  static blink::ScriptState* script_state = blink::Initialization();
  blink::TextProtoFuzzer(proto, script_state);
}
