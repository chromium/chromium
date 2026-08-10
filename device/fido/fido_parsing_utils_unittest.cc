// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_parsing_utils.h"

#include <array>
#include <string>

#include "components/cbor/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::fido_parsing_utils {

TEST(FidoParsingUtils, RedactCbor) {
  cbor::Value::MapValue map;
  map[cbor::Value("secret")] = cbor::Value("password");
  map[cbor::Value("public")] = cbor::Value("hello");
  cbor::Value val(std::move(map));

  cbor::Value redacted = RedactCbor(val, std::array{ToCborVector("secret")});
  EXPECT_EQ(redacted.GetMap().at(cbor::Value("secret")).GetString(),
            "[redacted]");
  EXPECT_EQ(redacted.GetMap().at(cbor::Value("public")).GetString(), "hello");
}

}  // namespace device::fido_parsing_utils
