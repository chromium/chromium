// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"

#include "testing/libfuzzer/libfuzzer_exports.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Intentionally leaked during fuzzing.
// See testing/libfuzzer/efficient_fuzzing.md.
DummyPageHolder* g_page_holder = nullptr;

int LLVMFuzzerInitialize(int* argc, char*** argv) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  // Scope cannot be created before BlinkFuzzerTestSupport because it requires
  // that Oilpan be initialized to access blink::ThreadState::Current.
  LEAK_SANITIZER_DISABLED_SCOPE;
  g_page_holder = std::make_unique<DummyPageHolder>().release();

  // Set loader sandbox flags and install a new document so the document
  // has all possible sandbox flags set on the document already when the
  // CSP is bound.
  scoped_refptr<SharedBuffer> empty_document_data = SharedBuffer::Create();
  g_page_holder->GetFrame().Loader().ForceSandboxFlags(WebSandboxFlags::kAll);
  g_page_holder->GetFrame().ForceSynchronousDocumentInstall(
      "text/html", empty_document_data);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  String header = String::FromUTF8(data, size);
  unsigned hash = header.IsNull() ? 0 : header.Impl()->GetHash();

  // Use the 'hash' value to pick header_type and header_source input.
  // 1st bit: header type.
  // 2nd bit: header source: HTTP (or other)
  // 3rd bit: header source: Meta or OriginPolicy (if not HTTP)
  ContentSecurityPolicyHeaderType header_type =
      hash & 0x01 ? kContentSecurityPolicyHeaderTypeEnforce
                  : kContentSecurityPolicyHeaderTypeReport;
  ContentSecurityPolicyHeaderSource header_source =
      kContentSecurityPolicyHeaderSourceHTTP;
  if (hash & 0x02) {
    header_source = (hash & 0x04)
                        ? kContentSecurityPolicyHeaderSourceMeta
                        : kContentSecurityPolicyHeaderSourceOriginPolicy;
  }

  // Construct and initialize a policy from the string.
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader(header, header_type, header_source);
  g_page_holder->GetDocument().InitContentSecurityPolicy(csp);

  // Force a garbage collection.
  // Specify namespace explicitly. Otherwise it conflicts on Mac OS X with:
  // CoreServices.framework/Frameworks/CarbonCore.framework/Headers/Threads.h.
  ThreadState::Current()->CollectAllGarbageForTesting(
      BlinkGC::kNoHeapPointersOnStack);

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  return blink::LLVMFuzzerInitialize(argc, argv);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
