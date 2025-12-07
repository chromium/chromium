// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include <memory>

#include "base/check.h"
#include "net/cookies/canonical_cookie.pb.h"
#include "net/cookies/canonical_cookie_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace net {

DEFINE_BINARY_PROTO_FUZZER(
    const canonical_cookie_proto::CanonicalCookie& cookie) {
  std::unique_ptr<CanonicalCookie> sanitized_cookie =
      canonical_cookie_proto::Convert(cookie);

  if (sanitized_cookie) {
    CanonicalCookie::CanonicalizationResult result =
        sanitized_cookie->IsCanonical();
    CHECK(result) << result;

    // Check identity property of various comparison functions
    const CanonicalCookie copied_cookie = *sanitized_cookie;
    CHECK(sanitized_cookie->IsEquivalent(copied_cookie));
    CHECK(sanitized_cookie->IsEquivalentForSecureCookieMatching(copied_cookie));
  }
}

}  // namespace net
