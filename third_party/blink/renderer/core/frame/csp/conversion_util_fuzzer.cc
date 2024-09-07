// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

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
  if (it - data > 250) {
    // Origins should not be too long. The origin of size 'N' is copied into 'M'
    // policies. The fuzzer can send an input of size N+M and use O(N*M) memory.
    // Due to this quadratic behavior, we must limit the size of the origin to
    // prevent the fuzzer from triggering OOM crash. Note that real domain names
    // are limited to 253 characters.
    return EXIT_SUCCESS;
  }

  String url = String(data, static_cast<unsigned>(it - 1 - data));
  String header = String(it, static_cast<unsigned>(size - (it - data)));
  unsigned hash = header.IsNull() ? 0 : header.Impl()->GetHash();

  // Use the 'hash' value to pick header_type and header_source input.
  // 1st bit: header type.
  // 2nd bit: header source: HTTP (or other)
  network::mojom::ContentSecurityPolicyType header_type =
      hash & 0x01 ? network::mojom::ContentSecurityPolicyType::kEnforce
                  : network::mojom::ContentSecurityPolicyType::kReport;
  network::mojom::ContentSecurityPolicySource header_source =
      network::mojom::ContentSecurityPolicySource::kHTTP;
  if (hash & 0x02) {
    header_source = network::mojom::ContentSecurityPolicySource::kMeta;
  }

  // Construct a policy from the string.
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed_policies =
      ParseContentSecurityPolicies(header, header_type, header_source,
                                   KURL(url));

  if (parsed_policies.size() > 0) {
    network::mojom::blink::ContentSecurityPolicyPtr converted_csp =
        ConvertToMojoBlink(ConvertToPublic(parsed_policies[0]->Clone()));
    CHECK(converted_csp->Equals(*parsed_policies[0]));
  }

  // Force a garbage collection.
  // Specify namespace explicitly. Otherwise it conflicts on Mac OS X with:
  // CoreServices.framework/Frameworks/CarbonCore.framework/Headers/Threads.h.
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
