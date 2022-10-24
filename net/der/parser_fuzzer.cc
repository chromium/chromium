// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/der/tag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  std::string der = provider.ConsumeRandomLengthString();
  net::der::Parser parser = net::der::Parser(net::der::Input(&der));
  while (provider.remaining_bytes()) {
    switch (provider.ConsumeIntegralInRange<int>(0, 13)) {
      case 0: {
        net::der::Tag tag;
        net::der::Input value;
        std::ignore = parser.ReadTagAndValue(&tag, &value);
        break;
      }
      case 1: {
        net::der::Input tlv;
        std::ignore = parser.ReadRawTLV(&tlv);
        break;
      }
      case 2: {
        absl::optional<net::der::Input> value;
        std::ignore = parser.ReadOptionalTag(
            provider.ConsumeIntegral<net::der::Tag>(), &value);
        break;
      }
      case 3: {
        bool was_present;
        std::ignore = parser.SkipOptionalTag(
            provider.ConsumeIntegral<net::der::Tag>(), &was_present);
        break;
      }
      case 4: {
        net::der::Input value;
        std::ignore =
            parser.ReadTag(provider.ConsumeIntegral<net::der::Tag>(), &value);
        break;
      }
      case 5: {
        std::ignore = parser.SkipTag(provider.ConsumeIntegral<net::der::Tag>());
        break;
      }
      case 6: {
        net::der::Parser new_parser;
        std::ignore = parser.ReadConstructed(
            provider.ConsumeIntegral<net::der::Tag>(), &new_parser);
        break;
      }
      case 7: {
        net::der::Parser new_parser;
        std::ignore = parser.ReadSequence(&new_parser);
        break;
      }
      case 8: {
        uint8_t value;
        std::ignore = parser.ReadUint8(&value);
        break;
      }
      case 9: {
        uint64_t value;
        std::ignore = parser.ReadUint64(&value);
        break;
      }
      case 10: {
        std::ignore = parser.ReadBitString();
        break;
      }
      case 11: {
        net::der::GeneralizedTime generalized_time;
        std::ignore = parser.ReadGeneralizedTime(&generalized_time);
        break;
      }
      case 12: {
        net::der::Tag tag;
        net::der::Input value;
        std::ignore = parser.PeekTagAndValue(&tag, &value);
        break;
      }
      case 13: {
        std::ignore = parser.Advance();
        break;
      }
    }
  }

  return 0;
}
