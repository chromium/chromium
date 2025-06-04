// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/test_util.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  // SAFETY: Making a span from the input provided by libFuzzer.
  auto data_span = UNSAFE_BUFFERS(base::span(data, size));

  // We need two pieces of input: a URL and a CSP string. Split |data| in two at
  // the first whitespace.
  auto it = std::ranges::find_if(data_span, [](uint8_t c) {
    return base::IsAsciiWhitespace(static_cast<char>(c));
  });
  if (it == data_span.end()) {
    // Not much point in going on with an empty CSP string.
    return EXIT_SUCCESS;
  }
  const size_t url_length = it - data_span.begin();
  if (url_length > 250) {
    // Origins should not be too long. The origin of size 'N' is copied into 'M'
    // policies. The fuzzer can send an input of size N+M and use O(N*M) memory.
    // Due to this quadratic behavior, we must limit the size of the origin to
    // prevent the fuzzer from triggering OOM crash. Note that real domain names
    // are limited to 253 characters.
    return EXIT_SUCCESS;
  }

  String url(data_span.first(url_length));
  String header(data_span.subspan(url_length + 1));
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
