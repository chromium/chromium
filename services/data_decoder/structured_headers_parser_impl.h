// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_STRUCTURED_HEADERS_PARSER_IMPL_H_
#define SERVICES_DATA_DECODER_STRUCTURED_HEADERS_PARSER_IMPL_H_

#include <string>

#include "services/data_decoder/public/mojom/structured_headers_parser.mojom.h"

namespace data_decoder {

class StructuredHeadersParserImpl : public mojom::StructuredHeadersParser {
 public:
  StructuredHeadersParserImpl();

  StructuredHeadersParserImpl(const StructuredHeadersParserImpl&) = delete;
  StructuredHeadersParserImpl& operator=(const StructuredHeadersParserImpl&) =
      delete;

  ~StructuredHeadersParserImpl() override;

 private:
  // mojom::StructuredHeadersParser:
  void ParseItem(const std::string& header,
                 ParseItemCallback callback) override;
  void ParseList(const std::string& header,
                 ParseListCallback callback) override;
  void ParseDictionary(const std::string& header,
                       ParseDictionaryCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_STRUCTURED_HEADERS_PARSER_IMPL_H_
