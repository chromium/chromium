// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/base64.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"
#include "net/base/host_port_pair.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_security_headers.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

namespace test_default {
#include "net/http/transport_security_state_static_unittest_default.h"
}

}  // anonymous namespace

class HttpSecurityHeadersTest : public testing::Test {
 public:
  ~HttpSecurityHeadersTest() override {
    SetTransportSecurityStateSourceForTesting(nullptr);
  }
};


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
  expect_max_age = base::TimeDelta::FromSeconds(243);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=3488923;", &max_age,
                              &include_subdomains));

  EXPECT_TRUE(ParseHSTSHeader("  Max-agE    = 567", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(567);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("  mAx-aGe    = 890      ", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(890);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=123;incLudesUbdOmains", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("incLudesUbdOmains; max-age=123", &max_age,
                              &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("   incLudesUbdOmains; max-age=123",
                              &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   incLudesUbdOmains; max-age=123; pumpkin=kitten", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin=894; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=123  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "   pumpkin; incLudesUbdOmains; max-age=\"123\"  ", &max_age,
                                   &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "animal=\"squirrel; distinguished\"; incLudesUbdOmains; max-age=123",
                                   &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(123);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader("max-age=394082;  incLudesUbdOmains",
                              &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(394082);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=39408299  ;incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 39408299u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=394082038  ; incLudesUbdOmains", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "max-age=394082038  ; incLudesUbdOmains;", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";; max-age=394082038  ; incLudesUbdOmains; ;", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";; max-age=394082038  ;", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_FALSE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      ";;    ; ; max-age=394082038;;; includeSubdomains     ;;  ;", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "incLudesUbdOmains   ; max-age=394082038 ;;", &max_age,
      &include_subdomains));
  expect_max_age =
      base::TimeDelta::FromSeconds(std::min(kMaxHSTSAgeSecs, 394082038u));
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=0  ;  incLudesUbdOmains   ", &max_age,
      &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(0);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);

  EXPECT_TRUE(ParseHSTSHeader(
      "  max-age=999999999999999999999999999999999999999999999  ;"
      "  incLudesUbdOmains   ", &max_age, &include_subdomains));
  expect_max_age = base::TimeDelta::FromSeconds(
      kMaxHSTSAgeSecs);
  EXPECT_EQ(expect_max_age, max_age);
  EXPECT_TRUE(include_subdomains);
}

TEST_F(HttpSecurityHeadersTest, BogusExpectCTHeaders) {
  base::TimeDelta max_age;
  bool enforce = false;
  GURL report_uri;
  EXPECT_FALSE(
      ParseExpectCTHeader(std::string(), &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("    ", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("abc", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("  abc", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("  abc   ", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("  max-age", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("  max-age  ", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age=", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("   max-age=", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("   max-age  =", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("   max-age=   ", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("   max-age  =     ", &max_age, &enforce,
                                   &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("   max-age  =     xy", &max_age, &enforce,
                                   &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("   max-age  =     3488a923", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488a923  ", &max_age, &enforce,
                                   &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-ag=3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-aged=3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age==3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("amax-age=3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age=-3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age=+3488923", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age=13####", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=9223372036854775807#####", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=18446744073709551615####", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999999999999999999999999$.&#!",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923     e", &max_age, &enforce,
                                   &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923     includesubdomain",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923includesubdomains", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923=includesubdomains",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923 includesubdomainx",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader(
      "max-age=3488923 includesubdomain=", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923 includesubdomain=true",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923 includesubdomainsx",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=3488923 includesubdomains x",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=34889.23 includesubdomains",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=34889 includesubdomains", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader(",,,, ,,,", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader(",,,, includeSubDomains,,,", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("   includeSubDomains,  ", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader(",", &max_age, &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age, ,", &max_age, &enforce, &report_uri));

  // Test that the parser rejects misquoted or invalid report-uris.
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999, report-uri=\"http://foo;bar\'",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999, report-uri=\"foo;bar\"",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999, report-uri=\"\"", &max_age,
                                   &enforce, &report_uri));

  // Test that the parser does not fix up misquoted values.
  EXPECT_FALSE(
      ParseExpectCTHeader("max-age=\"999", &max_age, &enforce, &report_uri));

  // Test that the parser rejects headers that contain duplicate directives.
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999, enforce, max-age=99999",
                                   &max_age, &enforce, &report_uri));
  EXPECT_FALSE(ParseExpectCTHeader("enforce, max-age=999, enforce", &max_age,
                                   &enforce, &report_uri));
  EXPECT_FALSE(
      ParseExpectCTHeader("report-uri=\"http://foo\", max-age=999, enforce, "
                          "report-uri=\"http://foo\"",
                          &max_age, &enforce, &report_uri));

  // Test that the parser rejects headers with values for the valueless
  // 'enforce' directive.
  EXPECT_FALSE(ParseExpectCTHeader("max-age=999, enforce=true", &max_age,
                                   &enforce, &report_uri));

  // Check the out args were not updated by checking the default
  // values for its predictable fields.
  EXPECT_EQ(0, max_age.InSeconds());
  EXPECT_FALSE(enforce);
  EXPECT_TRUE(report_uri.is_empty());
}

TEST_F(HttpSecurityHeadersTest, ValidExpectCTHeaders) {
  base::TimeDelta max_age;
  bool enforce = false;
  GURL report_uri;

  EXPECT_TRUE(
      ParseExpectCTHeader("max-age=243", &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(243), max_age);
  EXPECT_FALSE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  EXPECT_TRUE(ParseExpectCTHeader("  Max-agE    = 567", &max_age, &enforce,
                                  &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(567), max_age);
  EXPECT_FALSE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  EXPECT_TRUE(ParseExpectCTHeader("  mAx-aGe    = 890      ", &max_age,
                                  &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(890), max_age);
  EXPECT_FALSE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  EXPECT_TRUE(ParseExpectCTHeader("max-age=123,enFoRce", &max_age, &enforce,
                                  &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("enFoRCE, max-age=123", &max_age, &enforce,
                                  &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("   enFORce, max-age=123", &max_age, &enforce,
                                  &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(
      "report-uri=\"https://foo.test\",   enFORce, max-age=123", &max_age,
      &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_EQ(GURL("https://foo.test"), report_uri);

  enforce = false;
  report_uri = GURL();
  EXPECT_TRUE(
      ParseExpectCTHeader("enforce,report-uri=\"https://foo.test\",max-age=123",
                          &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_EQ(GURL("https://foo.test"), report_uri);

  enforce = false;
  report_uri = GURL();
  EXPECT_TRUE(
      ParseExpectCTHeader("enforce,report-uri=https://foo.test,max-age=123",
                          &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_EQ(GURL("https://foo.test"), report_uri);

  report_uri = GURL();
  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("report-uri=\"https://foo.test\",max-age=123",
                                  &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_FALSE(enforce);
  EXPECT_EQ(GURL("https://foo.test"), report_uri);

  report_uri = GURL();
  EXPECT_TRUE(ParseExpectCTHeader("   enFORcE, max-age=123, pumpkin=kitten",
                                  &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(
      "   pumpkin=894, report-uri=     \"https://bar\", enFORce, max-age=123  ",
      &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_EQ(GURL("https://bar"), report_uri);

  enforce = false;
  report_uri = GURL();
  EXPECT_TRUE(ParseExpectCTHeader("   pumpkin, enFoRcE, max-age=123  ",
                                  &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("   pumpkin, enforce, max-age=\"123\"  ",
                                  &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(
      "animal=\"squirrel, distinguished\", enFoRce, max-age=123", &max_age,
      &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(123), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("max-age=394082,  enforce", &max_age,
                                  &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(394082), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("max-age=39408299  ,enforce", &max_age,
                                  &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  // Per RFC 7230, "a recipient MUST parse and ignore a reasonable number of
  // empty list elements".
  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(",, max-age=394082038  , enfoRce, ,",
                                  &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(",, max-age=394082038  ,", &max_age, &enforce,
                                  &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_FALSE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  EXPECT_TRUE(
      ParseExpectCTHeader(",,    , , max-age=394082038,,, enforce     ,,  ,",
                          &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("enfORce   , max-age=394082038 ,,", &max_age,
                                  &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader("  max-age=0  ,  enforce   ", &max_age,
                                  &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());

  enforce = false;
  EXPECT_TRUE(ParseExpectCTHeader(
      "  max-age=999999999999999999999999999999999999999999999  ,"
      "  enforce   ",
      &max_age, &enforce, &report_uri));
  EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxExpectCTAgeSecs), max_age);
  EXPECT_TRUE(enforce);
  EXPECT_TRUE(report_uri.is_empty());
}

}  // namespace net
