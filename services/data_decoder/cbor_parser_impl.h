// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_CBOR_PARSER_IMPL_H_
#define SERVICES_DATA_DECODER_CBOR_PARSER_IMPL_H_

#include "components/cbor/values.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/data_decoder/public/mojom/cbor_parser.mojom.h"

namespace data_decoder {

// This is a class used to parse and decode CBOR values.

// Current Limitations:
// - Does not support null or undefined values
// - Integers must fit in the 'int' type
// - The keys in Maps must be a string or bytestring
// - If at least one Map key is invalid, an error will be returned
class CborParserImpl : public mojom::CborParser {
 public:
  CborParserImpl();
  ~CborParserImpl() override;

  // Implementation for mojom::CborParser
  void Parse(mojo_base::BigBuffer cbor, ParseCallback callback) override;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_CBOR_PARSER_IMPL_H_
