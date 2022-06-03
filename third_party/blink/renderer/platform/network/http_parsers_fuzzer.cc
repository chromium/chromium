// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/http_parsers.h"

#include <string>

#include "services/network/public/mojom/parsed_headers.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Larger inputs trigger OOMs, timeouts and slow units.
  if (size > 65536)
    return 0;

  static blink::BlinkFuzzerTestSupport test_support;

  blink::CommaDelimitedHeaderSet set;
  base::TimeDelta delay;
  String url;
  blink::ResourceResponse response;
  wtf_size_t end;

  std::string terminated(reinterpret_cast<const char*>(data), size);

  // There are no guarantees regarding the string capacity, but we are doing our
  // best to make it |size + 1|.
  terminated.shrink_to_fit();

  blink::IsValidHTTPToken(terminated.c_str());
  blink::ParseCacheControlDirectives(terminated.c_str(), AtomicString());
  blink::ParseCommaDelimitedHeader(terminated.c_str(), set);
  blink::ParseHTTPRefresh(terminated.c_str(), nullptr, delay, url);

  // Intentionally pass raw data as the API does not require trailing \0.
  blink::ParseMultipartHeadersFromBody(reinterpret_cast<const char*>(data),
                                       size, &response, &end);
  blink::ParseServerTimingHeader(terminated.c_str());
  blink::ParseContentTypeOptionsHeader(terminated.c_str());
  blink::ParseHeaders(terminated.c_str(), blink::KURL("http://example.com"));
  return 0;
}
