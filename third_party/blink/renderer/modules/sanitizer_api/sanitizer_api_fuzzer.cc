// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sanitizer.h"

#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_document_fragment_or_document.h"
#include "third_party/blink/renderer/bindings/modules/v8/string_or_trusted_html_or_document_fragment_or_document.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_sanitizer_config.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config.pb.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

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
}

DEFINE_TEXT_PROTO_FUZZER(const SanitizerConfigProto& proto) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  // Scope cannot be created before BlinkFuzzerTestSupport because it requires
  // that Oilpan be initialized to access blink::ThreadState::Current.
  LEAK_SANITIZER_DISABLED_SCOPE;
  static DummyPageHolder* g_page_holder = new DummyPageHolder();

  ScriptState* script_state =
      ToScriptStateForMainWorld(&g_page_holder->GetFrame());

  // Create random Sanitizer.
  auto* sanitizer_config = MakeGarbageCollected<SanitizerConfig>();
  MakeConfiguration(sanitizer_config, proto);
  auto* sanitizer = MakeGarbageCollected<Sanitizer>(sanitizer_config);

  // Sanitize random strings.
  StringOrTrustedHTMLOrDocumentFragmentOrDocument str1 =
      StringOrTrustedHTMLOrDocumentFragmentOrDocument::FromString(
          proto.html_string().c_str());
  sanitizer->sanitize(script_state, str1, IGNORE_EXCEPTION_FOR_TESTING);

  StringOrDocumentFragmentOrDocument str2 =
      StringOrDocumentFragmentOrDocument::FromString(
          proto.html_string().c_str());
  sanitizer->sanitizeToString(script_state, str2, IGNORE_EXCEPTION_FOR_TESTING);

  // Request a garbage collection.
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);
}

}  // namespace blink
