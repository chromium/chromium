// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/json_parser_impl.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"

namespace data_decoder {

JsonParserImpl::JsonParserImpl() = default;

JsonParserImpl::~JsonParserImpl() = default;

void JsonParserImpl::Parse(const std::string& json, ParseCallback callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadAndReturnErrorDeprecated(json, base::JSON_PARSE_RFC,
                                                     &error_code, &error);
  if (value) {
    std::move(callback).Run(base::make_optional(std::move(*value)),
                            base::nullopt);
  } else {
    std::move(callback).Run(base::nullopt,
                            base::make_optional(std::move(error)));
  }
}

}  // namespace data_decoder
