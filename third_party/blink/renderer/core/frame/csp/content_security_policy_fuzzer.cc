// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"

#include "testing/libfuzzer/libfuzzer_exports.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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

  scoped_refptr<SharedBuffer> empty_document_data = SharedBuffer::Create();
  g_page_holder->GetFrame().ForceSynchronousDocumentInstall(
      "text/html", empty_document_data);
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // We need two pieces of input: a URL and a CSP string. Split |data| in two at
  // the first whitespace.
  const uint8_t* it = data;
  for (; it < data + size; it++) {
    if (base::IsAsciiWhitespace(*reinterpret_cast<const char*>(it))) {
      it++;
      break;
    }
  }
  if (it == data + size) {
    // Not much point in going on with an empty CSP string.
    return EXIT_SUCCESS;
  }

  String url = String(data, it - 1 - data);
  String header = String(it, size - (it - data));
  unsigned hash = header.IsNull() ? 0 : header.Impl()->GetHash();

  // Use the 'hash' value to pick header_type and header_source input.
  // 1st bit: header type.
  // 2nd bit: header source: HTTP (or other)
  // 3rd bit: header source: Meta or OriginPolicy (if not HTTP)
  network::mojom::ContentSecurityPolicyType header_type =
      hash & 0x01 ? network::mojom::ContentSecurityPolicyType::kEnforce
                  : network::mojom::ContentSecurityPolicyType::kReport;
  network::mojom::ContentSecurityPolicySource header_source =
      network::mojom::ContentSecurityPolicySource::kHTTP;
  if (hash & 0x02) {
    header_source =
        (hash & 0x04)
            ? network::mojom::ContentSecurityPolicySource::kMeta
            : network::mojom::ContentSecurityPolicySource::kOriginPolicy;
  }

  scoped_refptr<SecurityOrigin> self_origin = SecurityOrigin::Create(KURL(url));

  // Construct and initialize a policy from the string.
  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();
  csp->DidReceiveHeader(header, *self_origin, header_type, header_source);
  auto* window = g_page_holder->GetFrame().DomWindow();
  window->SetContentSecurityPolicy(csp);

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
