// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Configure: # gn args out/Fuzz
// with args:
//   use_libfuzzer = true
//   is_asan = true
//   is_ubsan_security = true
//   is_debug = false
//   use_remoteexec = true
// Build:     # autoninja -C out/Fuzz blink_security_origin_fuzzer
// Run:       # ./out/Fuzz/blink_security_origin_fuzzer
//
// For more details, see
// https://chromium.googlesource.com/chromium/src/+/main/testing/libfuzzer/README.md
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Make sure an origin created from content (e.g. url::Origin) survives the
// conversion from/to blink.
void RoundTripFromContent(const GURL& input) {
  url::Origin origin_1 = url::Origin::Create(input);
  WebSecurityOrigin web_security_origin_1 = origin_1;
  scoped_refptr<const SecurityOrigin> security_origin = web_security_origin_1;
  WebSecurityOrigin web_security_origin_2 = security_origin;
  url::Origin origin_2 = web_security_origin_2;

  CHECK_EQ(origin_1, origin_2);
}

// Make sure an origin created from blink (e.g. blink::SecurityOrigin) survives
// the conversion from/to content.
void RoundTripFromBlink(String input) {
  scoped_refptr<const SecurityOrigin> security_origin_1 =
      SecurityOrigin::CreateFromString(input);
  WebSecurityOrigin web_security_origin_1 = security_origin_1;
  url::Origin origin = web_security_origin_1;
  WebSecurityOrigin web_security_origin_2 = origin;
  scoped_refptr<const SecurityOrigin> security_origin_2 = web_security_origin_2;

  CHECK(security_origin_1->IsSameOriginWith(security_origin_2.get()));
}

// Entry point for LibFuzzer.
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  test::TaskEnvironment task_environment;

  std::string input(reinterpret_cast<const char*>(data), size);
  RoundTripFromContent(GURL(input));
  RoundTripFromBlink(String::FromUTF8(input));
  return EXIT_SUCCESS;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}
