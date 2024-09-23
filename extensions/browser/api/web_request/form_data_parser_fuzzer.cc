// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "extensions/browser/api/web_request/form_data_parser.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

using extensions::FormDataParser;

namespace {

// Does initialization and holds state that's shared across all runs.
class Environment {
 public:
  Environment() {
    // Disable noisy logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

net::HttpRequestHeaders GenerateHttpRequestHeaders(
    FuzzedDataProvider& provider) {
  net::HttpRequestHeaders headers;
  for (;;) {
    const std::string key = provider.ConsumeRandomLengthString();
    const std::string value = provider.ConsumeRandomLengthString();
    if (key.empty() || !net::HttpUtil::IsValidHeaderName(key) ||
        !net::HttpUtil::IsValidHeaderValue(value)) {
      break;
    }
    headers.SetHeader(key, value);
  }
  return headers;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider provider(data, size);

  // Create parser sources. Per API contract, they must outlive the parser.
  std::vector<std::string> sources;
  for (;;) {
    std::string source = provider.ConsumeRandomLengthString();
    if (source.empty()) {
      break;
    }
    sources.push_back(std::move(source));
  }

  // Create a parser with random initialization parameters.
  std::unique_ptr<FormDataParser> parser;
  switch (provider.ConsumeIntegralInRange<int>(0, 2)) {
    case 0: {
      parser = FormDataParser::Create(GenerateHttpRequestHeaders(provider));
      break;
    }
    case 1: {
      parser = FormDataParser::CreateFromContentTypeHeader(
          /*content_type_header=*/nullptr);
      break;
    }
    case 2: {
      std::string content_type_header = provider.ConsumeRandomLengthString();
      parser =
          FormDataParser::CreateFromContentTypeHeader(&content_type_header);
      break;
    }
  }
  if (!parser) {
    return 0;
  }

  // Run the parser.
  for (const auto& source : sources) {
    parser->SetSource(source);

    FormDataParser::Result result;
    while (parser->GetNextNameValue(&result)) {
      // Discard the result - we can't verify anything in it here.
    }
  }

  return 0;
}
