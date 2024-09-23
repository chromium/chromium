// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_security_headers.h"

#include <stdint.h>

#include <iterator>

#include "base/base64.h"
#include "base/stl_util.h"
#include "crypto/sha2.h"
#include "net/base/host_port_pair.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

namespace test_default {
#include "base/time/time.h"
#include "net/http/transport_security_state_static_unittest_default.h"
}

}  // anonymous namespace

class HttpSecurityHeadersTest : public testing::Test {
 public:
  ~HttpSecurityHeadersTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }
};

TEST_F(HttpSecurityHeadersTest, LeadingTrailingSemicolons) {
  base::TimeDelta max_age;
  bool include_subdomains = false;

  const char* test_cases[] = {
      "max-age=123",       ";max-age=123",        ";;max-age=123",
      ";;;;max-age=123",   "; ;max-age=123",      "; ; max-age=123",
      ";max-age=123;",     ";;max-age=123;;",     ";;;;max-age=123;;;;",
      "; ;max-age=123; ;", "; ; max-age=123; ; ", "max-age=123;",
      "max-age=123;;",     "max-age=123;;;;",     "max-age=123; ;",
      "max-age=123; ; ",
  };
  for (const char* value : test_cases) {
    SCOPED_TRACE(value);
    EXPECT_TRUE(ParseHSTSHeader(value, &max_age, &include_subdomains));
    EXPECT_EQ(base::Seconds(123), max_age);
    EXPECT_FALSE(include_subdomains);
  }
}

TEST_F(HttpSecurityHeadersTest, InvalidDirectiveNames) {
  base::TimeDelta max_age;
  bool include_subdomains = false;

  const char* test_cases[] = {
      "'max-age'=1",
      "\"max-age\"=1",
      "max-age=1; max-age=2",
      "max-age=1; MaX-AgE=2",
      "max-age=1; includeSubDomains; iNcLUdEsUbDoMaInS",
      "max-age=1; \"",
      "max-age=1; \"includeSubdomains",
      "max-age=1; in\"cludeSubdomains",
      "max-age=1; includeSubdomains\"",
      "max-age=1; \"includeSubdomains\"",
      "max-age=1; includeSubdomains; non\"token",
      "max-age=1; includeSubdomains; non@token",
      "max-age=1; includeSubdomains; non,token",
      "max-age=1; =2",
      "max-age=1; =2; unknownDirective",
  };

  for (const char* value : test_cases) {
    SCOPED_TRACE(value);
    EXPECT_FALSE(ParseHSTSHeader(value, &max_age, &include_subdomains));
  }
}

TEST_F(HttpSecurityHeadersTest, InvalidDirectiveValues) {
  base::TimeDelta max_age;
  bool include_subdomains = false;

  const char* test_cases[] = {
      "max-age=",
      "max-age=@",
      "max-age=1a;",
      "max-age=1a2;",
      "max-age=1##;",
      "max-age=12\";",
      "max-age=-1;",
      "max-age=+1;",
      "max-age='1';",
      "max-age=1abc;",
      "max-age=1 abc;",
      "max-age=1.5;",
      "max-age=1; includeSubDomains=true",
      "max-age=1; includeSubDomains=false",
      "max-age=1; includeSubDomains=\"\"",
      "max-age=1; includeSubDomains=''",
      "max-age=1; includeSubDomains=\"true\"",
      "max-age=1; includeSubDomains=\"false\"",
      "max-age=1; unknownDirective=non\"token",
      "max-age=1; unknownDirective=non@token",
      "max-age=1; unknownDirective=non,token",
      "max-age=1; unknownDirective=",
  };

  for (const char* value : test_cases) {
    SCOPED_TRACE(value);
    EXPECT_FALSE(ParseHSTSHeader(value, &max_age, &include_subdomains));
  }
}

TEST_F(HttpSecurityHeadersTest, BogusHeaders) {
  base::TimeDelta max_age;
  bool include_subdomains = false;

  EXPECT_FALSE(
      ParseHSTSHeader(std::string(), &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("    ", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("abc", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  abc", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  abc   ", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  max-age", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("  max-age  ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age=", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age=   ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     xy", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   max-age  =     3488a923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488a923  ", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-ag=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-aged=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age==3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("amax-age=3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=-3488923", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(
      ParseHSTSHeader("max-age=+3488923", &max_age, &include_subdomains));
  EXPECT_FALSE(
      ParseHSTSHeader("max-age=13####", &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=9223372036854775807#####", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=18446744073709551615####", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=999999999999999999999999$.&#!",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923     e", &max_age,
                               &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923     includesubdomain",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923=includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomainx",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomain=",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomain=true",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomainsx",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=3488923 includesubdomains x",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=34889.23 includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age=34889 includesubdomains",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader(";;;; ;;;",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader(";;;; includeSubDomains;;;",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("   includeSubDomains;  ",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader(";",
                               &max_age, &include_subdomains));
  EXPECT_FALSE(ParseHSTSHeader("max-age; ;",
                               &max_age, &include_subdomains));

  // Check the out args were not updated by checking the default
  // values for its predictable fields.
  EXPECT_EQ(0, max_age.InSeconds());
  EXPECT_FALSE(include_subdomains);
}

TEST_F(HttpSecurityHeadersTest, ValidSTSHeaders) {
  base::TimeDelta max_age;
  base::TimeDelta expect_max_age;
  bool include_subdomains = false;

  EXPECT_TRUE(ParseHSTSHeader("max-age=243", &max_age,
                              &include_subdomains));
  expect_max_age = base::Seconds(243);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=3488923;", &max_age,
                              &include_subdomains));

  EXPECT_TRUE(ParseHSTSHeader("  Max-agE    = 567", &max_age,
                              &include_subdomains));
  expect_max_age = base::Seconds(567);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("  mAx-aGe    = 890      ", &max_age,
                              &include_subdomains));
  expect_max_age = base::Seconds(890);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=123;incLudesUbdOmains", &max_age,
                              &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("incLudesUbdOmains; max-age=123", &max_age,
                              &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("   incLudesUbdOmains; max-age=123",
                              &max_age, &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   incLudesUbdOmains; max-age=123; pumpkin=kitten", &max_age,
                                   &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin=894; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=\"123\"  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "animal=\"squirrel; distinguished\"; incLudesUbdOmains; max-age=123",
                                   &max_age, &include_subdomains));
  expect_max_age = base::Seconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=394082;  incLudesUbdOmains",
                              &max_age, &include_subdomains));
  expect_max_age = base::Seconds(394082);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 39408299u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=394082038  ; incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=394082038  ; incLudesUbdOmains;", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";; max-age=394082038  ; incLudesUbdOmains; ;", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";; max-age=394082038  ;", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";;    ; ; max-age=394082038;;; includeSubdomains     ;;  ;", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "incLudesUbdOmains   ; max-age=394082038 ;;", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age,
      &include_subdomains));
  expect_max_age = base::Seconds(0);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   ", &max_age, &include_subdomains));
  expect_max_age = base::Seconds(kMaxHSTSAgeSecs);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);
}

}  // namespace net
