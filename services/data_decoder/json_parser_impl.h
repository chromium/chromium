// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_
#define SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_

#include <string>

#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace data_decoder {

class JsonParserImpl : public mojom::JsonParser {
 public:
  JsonParserImpl();

  JsonParserImpl(const JsonParserImpl&) = delete;
  JsonParserImpl& operator=(const JsonParserImpl&) = delete;

  ~JsonParserImpl() override;

 private:
  // mojom::JsonParser implementation.
  void Parse(const std::string& json,
             uint32_t options,
             ParseCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_
