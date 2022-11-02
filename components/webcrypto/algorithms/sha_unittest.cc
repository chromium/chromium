// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/values.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/algorithms/test_helpers.h"
#include "components/webcrypto/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_crypto_key.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"

namespace webcrypto {

namespace {

class WebCryptoShaTest : public WebCryptoTestBase {};

TEST_F(WebCryptoShaTest, DigestSampleSets) {
  base::Value::List tests = ReadJsonTestFileAsList("sha.json");

  for (const auto& test_value : tests) {
    SCOPED_TRACE(&test_value - &tests[0]);
    ASSERT_TRUE(test_value.is_dict());
    const base::Value::Dict& test = test_value.GetDict();

    blink::WebCryptoAlgorithm test_algorithm =
        GetDigestAlgorithm(test, "algorithm");
    std::vector<uint8_t> test_input = GetBytesFromHexString(test, "input");
    std::vector<uint8_t> test_output = GetBytesFromHexString(test, "output");

    std::vector<uint8_t> output;
    ASSERT_EQ(Status::Success(), Digest(test_algorithm, test_input, &output));
    EXPECT_BYTES_EQ(test_output, output);
  }
}

}  // namespace

}  // namespace webcrypto
