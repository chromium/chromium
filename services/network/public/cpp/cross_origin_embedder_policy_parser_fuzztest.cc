// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void CrossOriginEmbedderPolicyParserFuzzerTest(const std::string& value1,
                                               const std::string& value2) {
  std::string raw_headers = base::StrCat(
      {"HTTP/1.1 200 OK\n", "cross-origin-embedder-policy: ", value1, "\n",
       "cross-origin-embedder-policy-report-only: ", value2, "\n\n"});

  // HttpResponseHeaders expects headers to be separated by '\0' (not '\n').
  // This replaces all newlines, including those embedded in value1/value2.
  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);

  network::ParseCrossOriginEmbedderPolicy(*headers);
}

FUZZ_TEST(CrossOriginEmbedderPolicyParserTest,
          CrossOriginEmbedderPolicyParserFuzzerTest);
