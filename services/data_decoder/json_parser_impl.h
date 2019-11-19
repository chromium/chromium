// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_
#define SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace data_decoder {

class JsonParserImpl : public mojom::JsonParser {
 public:
  JsonParserImpl();
  ~JsonParserImpl() override;

 private:
  // mojom::JsonParser implementation.
  void Parse(const std::string& json, ParseCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(JsonParserImpl);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_JSON_PARSER_IMPL_H_
