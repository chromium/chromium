// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_XML_PARSER_H_
#define SERVICES_DATA_DECODER_XML_PARSER_H_

#include <memory>
#include <string>

#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

namespace data_decoder {

class XmlParser : public mojom::XmlParser {
 public:
  XmlParser();

  XmlParser(const XmlParser&) = delete;
  XmlParser& operator=(const XmlParser&) = delete;

  ~XmlParser() override;

 private:
  // mojom::XmlParser implementation.
  void Parse(const std::string& xml,
             WhitespaceBehavior whitespace_behavior,
             ParseCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_XML_PARSER_H_
