// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A test application for the MachineDealCode class.
//
// These tests should not be executed on the build server:
// - They assert for the failed cases.
// - They modify machine state (registry).
//
// These tests require write access to HKLM and HKCU, unless
// rlz_lib::CreateMachineState() has been successfully called.

#include "rlz/win/lib/machine_deal.h"

#include "rlz/lib/machine_deal_win.h"
#include "rlz/test/rlz_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MachineDealCodeHelper : public rlz_lib::MachineDealCode {
 public:
  static bool Clear() { return rlz_lib::MachineDealCode::Clear(); }

 private:
  MachineDealCodeHelper() {}
  ~MachineDealCodeHelper() {}
};

class MachineDealCodeTest : public RlzLibTestBase {
};

TEST_F(MachineDealCodeTest, CreateMachineState) {
  EXPECT_TRUE(rlz_lib::CreateMachineState());
}

TEST_F(MachineDealCodeTest, Set) {
  MachineDealCodeHelper::Clear();

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
  EXPECT_EQ("dcc_value", rlz_lib::MachineDealCode::Get());

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value_2"));
  EXPECT_EQ("dcc_value_2", rlz_lib::MachineDealCode::Get());

  // Overly long deal code should fail validation.
  std::string too_long_dcc(rlz_lib::kMaxDccLength + 1, 'a');
  EXPECT_FALSE(rlz_lib::MachineDealCode::Set(too_long_dcc));

  // Invalid characters should be normalized to '.'.
  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("bad deal code?"));
  EXPECT_EQ("bad.deal.code.", rlz_lib::MachineDealCode::Get());
}

TEST_F(MachineDealCodeTest, Get) {
  MachineDealCodeHelper::Clear();

  EXPECT_EQ(std::nullopt, rlz_lib::MachineDealCode::Get());

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
  EXPECT_EQ("dcc_value", rlz_lib::MachineDealCode::Get());

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set(""));
  EXPECT_EQ(std::nullopt, rlz_lib::MachineDealCode::Get());
}

TEST_F(MachineDealCodeTest, SetFromPingResponse) {
  rlz_lib::MachineDealCode::Set("MyDCCode");

  // Bad responses

  const char kBadDccResponse[] =
    "dcc: NotMyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: 1B4D6BB3";
  EXPECT_FALSE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kBadDccResponse));
  EXPECT_EQ("MyDCCode", rlz_lib::MachineDealCode::Get());

  const char kBadCrcResponse[] =
    "dcc: MyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: 90707106";
  EXPECT_FALSE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kBadCrcResponse));
  EXPECT_EQ("MyDCCode", rlz_lib::MachineDealCode::Get());

  // Good responses

  const char kMissingSetResponse[] =
    "dcc: MyDCCode \r\n"
    "crc32: 35F2E717";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kMissingSetResponse));
  EXPECT_EQ("MyDCCode", rlz_lib::MachineDealCode::Get());

  const char kGoodResponse[] =
    "dcc: MyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: C8540E02";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse));
  EXPECT_EQ("NewDCCode", rlz_lib::MachineDealCode::Get());

  const char kGoodResponse2[] =
    "set_dcc: NewDCCode2  \r\n"
    "dcc:   NewDCCode \r\n"
    "crc32: 60B6409A";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse2));
  EXPECT_EQ("NewDCCode2", rlz_lib::MachineDealCode::Get());

  MachineDealCodeHelper::Clear();
  const char kGoodResponse3[] =
    "set_dcc: NewDCCode  \r\n"
    "crc32: 374C1C47";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse3));
  EXPECT_EQ("NewDCCode", rlz_lib::MachineDealCode::Get());

  MachineDealCodeHelper::Clear();
  const char kGoodResponse4[] =
    "dcc:   \r\n"
    "set_dcc: NewDCCode  \r\n"
    "crc32: 0AB1FB39";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse4));
  EXPECT_EQ("NewDCCode", rlz_lib::MachineDealCode::Get());
}

TEST_F(MachineDealCodeTest, GetAsCgi) {
  MachineDealCodeHelper::Clear();

  EXPECT_EQ(std::nullopt, rlz_lib::MachineDealCode::GetAsCgi());

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));
  EXPECT_EQ("dcc=dcc_value", rlz_lib::MachineDealCode::GetAsCgi());
}
