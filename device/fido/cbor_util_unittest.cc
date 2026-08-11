// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cbor_util.h"

#include <string>
#include <utility>

#include "components/cbor/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fido_cbor_util {

TEST(FidoCborUtilTest, RedactValueAtPaths) {
  cbor::Value::MapValue map;
  map[cbor::Value("secret")] = cbor::Value("password");
  map[cbor::Value("public")] = cbor::Value("hello");
  cbor::Value val(std::move(map));

  cbor::Value redacted = RedactValueAtPaths(val, Path("secret"));
  EXPECT_EQ(redacted.GetMap().at(cbor::Value("secret")).GetString(),
            "[redacted]");
  EXPECT_EQ(redacted.GetMap().at(cbor::Value("public")).GetString(), "hello");
}

}  // namespace fido_cbor_util
