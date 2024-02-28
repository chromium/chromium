// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/structured_headers_parser_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "net/http/structured_headers.h"

namespace data_decoder {

StructuredHeadersParserImpl::StructuredHeadersParserImpl() = default;

StructuredHeadersParserImpl::~StructuredHeadersParserImpl() = default;

void StructuredHeadersParserImpl::ParseItem(const std::string& header,
                                            ParseItemCallback callback) {
  std::move(callback).Run(net::structured_headers::ParseItem(header));
}

void StructuredHeadersParserImpl::ParseList(const std::string& header,
                                            ParseListCallback callback) {
  std::move(callback).Run(net::structured_headers::ParseList(header));
}

void StructuredHeadersParserImpl::ParseDictionary(
    const std::string& header,
    ParseDictionaryCallback callback) {
  std::move(callback).Run(net::structured_headers::ParseDictionary(header));
}

}  // namespace data_decoder
