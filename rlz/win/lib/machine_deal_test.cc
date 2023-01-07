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
  char dcc_50[50];
  dcc_50[0] = 0;

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));

  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("dcc_value", dcc_50);

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value_2"));

  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("dcc_value_2", dcc_50);
}

TEST_F(MachineDealCodeTest, Get) {
  MachineDealCodeHelper::Clear();
  char dcc_50[50], dcc_2[2];
  dcc_50[0] = 0;
  dcc_2[0] = 0;

  EXPECT_FALSE(rlz_lib::MachineDealCode::Get(dcc_50, 50));

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));

  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("dcc_value", dcc_50);

  EXPECT_FALSE(rlz_lib::MachineDealCode::Get(dcc_2, 2));
}

TEST_F(MachineDealCodeTest, SetFromPingResponse) {
  rlz_lib::MachineDealCode::Set("MyDCCode");
  char dcc_50[50];

  // Bad responses

  const char kBadDccResponse[] =
    "dcc: NotMyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: 1B4D6BB3";
  EXPECT_FALSE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kBadDccResponse));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("MyDCCode", dcc_50);

  const char kBadCrcResponse[] =
    "dcc: MyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: 90707106";
  EXPECT_FALSE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kBadCrcResponse));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("MyDCCode", dcc_50);

  // Good responses

  const char kMissingSetResponse[] =
    "dcc: MyDCCode \r\n"
    "crc32: 35F2E717";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kMissingSetResponse));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("MyDCCode", dcc_50);

  const char kGoodResponse[] =
    "dcc: MyDCCode \r\n"
    "set_dcc: NewDCCode\r\n"
    "crc32: C8540E02";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("NewDCCode", dcc_50);

  const char kGoodResponse2[] =
    "set_dcc: NewDCCode2  \r\n"
    "dcc:   NewDCCode \r\n"
    "crc32: 60B6409A";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse2));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("NewDCCode2", dcc_50);

  MachineDealCodeHelper::Clear();
  const char kGoodResponse3[] =
    "set_dcc: NewDCCode  \r\n"
    "crc32: 374C1C47";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse3));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("NewDCCode", dcc_50);

  MachineDealCodeHelper::Clear();
  const char kGoodResponse4[] =
    "dcc:   \r\n"
    "set_dcc: NewDCCode  \r\n"
    "crc32: 0AB1FB39";
  EXPECT_TRUE(rlz_lib::MachineDealCode::SetFromPingResponse(
      kGoodResponse4));
  EXPECT_TRUE(rlz_lib::MachineDealCode::Get(dcc_50, 50));
  EXPECT_STREQ("NewDCCode", dcc_50);
}

TEST_F(MachineDealCodeTest, GetAsCgi) {
  MachineDealCodeHelper::Clear();
  char cgi_50[50], cgi_2[2];
  cgi_50[0] = 0;
  cgi_2[0] = 0;

  EXPECT_FALSE(rlz_lib::MachineDealCode::GetAsCgi(cgi_50, 50));
  EXPECT_STREQ("", cgi_50);

  EXPECT_TRUE(rlz_lib::MachineDealCode::Set("dcc_value"));

  EXPECT_TRUE(rlz_lib::MachineDealCode::GetAsCgi(cgi_50, 50));
  EXPECT_STREQ("dcc=dcc_value", cgi_50);

  EXPECT_FALSE(rlz_lib::MachineDealCode::GetAsCgi(cgi_2, 2));
}
