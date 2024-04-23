// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

constexpr std::string_view kTestPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "YWtsc2Rqa2FidmprYW5idWlkcnR1aW93dGdodWlyYiBqaWtnYmFzamtsbmR2YXVp\n"
    "ZXJ0YnVpd2Jlcmd1aSBhZGZ1amllZGZidWluYXNkamt2amtkYyAgZXJoZmdpb2py\n"
    "d2Vpb2doaW9xYWJudmlvZmRudmlvZXdoZ2lvdXV2IGYgICAgICAgICAgd2Vpamhy\n"
    "aWZvd2VuZmlvbndkZmlvbmlvd2Vub2Zub2l3ZW5maW4uMjM1NDM0NTY0MzZhc2xk\n"
    "amZua2xkam5mMjM0MzUxMjUxNDA5NWtsbmFzZ2xuaW9ybmU=\n"
    "-----END PUBLIC KEY-----\n";

constexpr std::string_view kTestRaw =
    "aklsdjkabvjkanbuidrtuiowtghuirb jikgbasjklndvauiertbuiwbergui "
    "adfujiedfbuinasdjkvjkdc  erhfgiojrweioghioqabnviofdnvioewhgiouuv f        "
    "  "
    "weijhrifowenfionwdfioniowenofnoiwenfin."
    "23543456436asldjfnkldjnf2343512514095klnasglniorne";

TEST(UtilsTest, LooksLikePem_WithValidPem_ReturnsTrue) {
  EXPECT_TRUE(device::enclave::LooksLikePem(kTestPem));
}

TEST(UtilsTest, LooksLikePem_WithInvalidPem_ReturnsFalse) {
  EXPECT_FALSE(device::enclave::LooksLikePem("This should return false"));
}

TEST(UtilsTest, ConvertPemToRaw_WithValidPem_ReturnsRaw) {
  auto res = device::enclave::ConvertPemToRaw(kTestPem);
  EXPECT_TRUE(res.has_value());
  EXPECT_EQ(std::string(res->begin(), res->end()), kTestRaw);
}

TEST(UtilsTest, ConvertPemToRaw_WithInvalidPem_ReturnsError) {
  auto res = device::enclave::ConvertPemToRaw("Not a valid PEM");
  EXPECT_FALSE(res.has_value());
}

TEST(UtilsTest, ConvertRawToPem_ReturnsPem) {
  std::vector<uint8_t> temp(kTestRaw.begin(), kTestRaw.end());
  auto res = device::enclave::ConvertRawToPem(temp);
  EXPECT_EQ(res, kTestPem);
}

}  // namespace device::enclave
