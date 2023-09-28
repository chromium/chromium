// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for SpdyHeaderstoHttpResponseHeadersUsingBuilder. Compares the output
// for the same input to SpdyHeadersToHttpResponseHeadersUsingRawString and
// verifies they match.

// TODO(ricea): Remove this when SpdyHeadersToHttpResponseHeadersUsingRawString
// is removed.

#include <stddef.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"

namespace net {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece rest(reinterpret_cast<const char*>(data), size);
  // We split the input at "\n" to force the fuzzer to produce a corpus that is
  // human-readable. "\n" cannot appear in a valid header name or value so this
  // is safe.
  auto get_string = [&rest]() {
    size_t newline_pos = rest.find('\n');
    if (newline_pos == base::StringPiece::npos) {
      newline_pos = rest.size();
    }
    base::StringPiece first_line = rest.substr(0, newline_pos);
    if (newline_pos + 1 < rest.size()) {
      rest = rest.substr(newline_pos + 1);
    } else {
      rest = base::StringPiece();
    }
    return first_line;
  };
  spdy::Http2HeaderBlock input;

  const base::StringPiece status = get_string();
  if (!HttpUtil::IsValidHeaderValue(status)) {
    return 0;
  }
  input[":status"] = status;
  while (!rest.empty()) {
    const base::StringPiece name = get_string();
    if (!HttpUtil::IsValidHeaderName(name)) {
      return 0;
    }
    const base::StringPiece value = get_string();
    if (!HttpUtil::IsValidHeaderValue(value)) {
      return 0;
    }
    input.AppendValueOrAddHeader(name, value);
  }
  const auto by_builder = SpdyHeadersToHttpResponseHeadersUsingBuilder(input);
  const auto by_raw_string =
      SpdyHeadersToHttpResponseHeadersUsingRawString(input);
  if (by_builder.has_value()) {
    CHECK(by_builder.value()->StrictlyEquals(*by_raw_string.value()));
  } else {
    CHECK(!by_raw_string.has_value());
    CHECK_EQ(by_builder.error(), by_raw_string.error());
  }
  return 0;
}

}  // namespace net
