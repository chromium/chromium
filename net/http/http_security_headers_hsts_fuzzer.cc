// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "net/http/http_security_headers.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

void TestParseHSTSHeader(const std::string& input) {
  base::TimeDelta max_age;
  bool include_subdomains = false;
  net::ParseHSTSHeader(input, &max_age, &include_subdomains);
}

}  // namespace

FUZZ_TEST(HttpSecurityHeadersHstsFuzzer, TestParseHSTSHeader)
    .WithDomains(
        fuzztest::String().WithDictionary(fuzztest::ReadDictionaryFromFile(
            base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
                .AppendASCII("net/data/fuzzer_dictionaries/"
                             "net_http_security_headers_fuzzer.dict")
                .AsUTF8Unsafe())))
    .WithSeeds(fuzztest::ReadFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
            .AppendASCII("net/data/fuzzer_data/http_security_headers/")
            .AsUTF8Unsafe()));
