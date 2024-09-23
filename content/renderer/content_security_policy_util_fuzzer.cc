// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Configure:
// # tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Linux ASan'  out/libfuzzer
// Build:
// # autoninja -C out/libfuzzer content_security_policy_util_fuzzer
// Run:
// # ./out/libfuzzer/content_security_policy_util_fuzzer
//
// For more details, see
// https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/README.md

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_util.h"
#include "base/test/test_timeouts.h"
#include "content/public/test/blink_test_environment.h"
#include "content/renderer/content_security_policy_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"


namespace {

// This is similar to blink::BlinkFuzzerTestSupport, which we can't import from
// content.
class Environment {
 public:
  Environment() {
    // Note: we don't tear anything down here after an iteration of the fuzzer
    // is complete, this is for efficiency. We rerun the fuzzer with the same
    // environment as the previous iteration.
    base::AtExitManager at_exit;

    CHECK(base::i18n::InitializeICU());

    base::CommandLine::Init(0, nullptr);

    TestTimeouts::Initialize();

    blink_environment_.SetUp();
  }
  ~Environment() {}

 private:
  content::BlinkTestEnvironment blink_environment_;
};

}  // namespace

namespace content {

// Entry point for LibFuzzer. This function uses |data| to create a
// network::mojom::ContentSecurityPolicy |csp|, and then checks that the
// composition of BuildContentSecurityPolicy and ToWebContentSecurityPolicy is
// the identity on |csp|.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment environment = Environment();

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
    // Origins should not be too long. The fuzzer will run oom otherwise.
    return EXIT_SUCCESS;
  }

  std::string raw_url(reinterpret_cast<const char*>(data), it - 1 - data);
  std::string raw_csp(reinterpret_cast<const char*>(it), size - (it - data));

  if (blink::WebString::FromUTF8(raw_url).Utf8() != raw_url ||
      blink::WebString::FromUTF8(raw_csp).Utf8() != raw_csp) {
    // The back-and-forth conversion can only work for valid utf8 input.
    return EXIT_SUCCESS;
  }

  GURL parsed_url(raw_url);
  if (!parsed_url.is_valid()) {
    return EXIT_SUCCESS;
  }

  static const uint8_t kEnforcementBit = 0x01;
  static const uint8_t kSourceBit1 = 0x02;

  // Generate pseudo-random |header_type| and |header_source|.
  network::mojom::ContentSecurityPolicyType header_type =
      data[0] & kEnforcementBit
          ? network::mojom::ContentSecurityPolicyType::kEnforce
          : network::mojom::ContentSecurityPolicyType::kReport;

  network::mojom::ContentSecurityPolicySource header_source =
      data[0] & kSourceBit1
          ? network::mojom::ContentSecurityPolicySource::kMeta
          : network::mojom::ContentSecurityPolicySource::kHTTP;

  // Parse the Content Security Policy string.
  std::vector<network::mojom::ContentSecurityPolicyPtr> csp =
      network::ParseContentSecurityPolicies(raw_csp, header_type, header_source,
                                            GURL(raw_url));

  if (csp.size() > 0) {
    network::mojom::ContentSecurityPolicyPtr converted_csp =
        BuildContentSecurityPolicy(ToWebContentSecurityPolicy(csp[0]->Clone()));
    CHECK(converted_csp->Equals(*csp[0]));
  }

  return EXIT_SUCCESS;
}

}  // namespace content

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return content::LLVMFuzzerTestOneInput(data, size);
}
