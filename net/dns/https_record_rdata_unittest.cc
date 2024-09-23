// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/https_record_rdata.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(HttpsRecordRdataTest, ParsesAlias) {
  const char kRdata[] =
      // Priority: 0 for alias record
      "\000\000"
      // Alias name: chromium.org
      "\010chromium\003org\000";

  std::unique_ptr<HttpsRecordRdata> rdata =
      HttpsRecordRdata::Parse(std::string_view(kRdata, sizeof(kRdata) - 1));
  ASSERT_TRUE(rdata);

  AliasFormHttpsRecordRdata expected("chromium.org");
  EXPECT_TRUE(rdata->IsEqual(&expected));

  EXPECT_TRUE(rdata->IsAlias());
  AliasFormHttpsRecordRdata* alias_rdata = rdata->AsAliasForm();
  ASSERT_TRUE(alias_rdata);
  EXPECT_EQ(alias_rdata->alias_name(), "chromium.org");
}

TEST(HttpsRecordRdataTest, ParseAliasWithEmptyName) {
  const char kRdata[] =
      // Priority: 0 for alias record
      "\000\000"
      // Alias name: ""
      "\000";

  std::unique_ptr<HttpsRecordRdata> rdata =
      HttpsRecordRdata::Parse(std::string_view(kRdata, sizeof(kRdata) - 1));
  ASSERT_TRUE(rdata);

  AliasFormHttpsRecordRdata expected("");
  EXPECT_TRUE(rdata->IsEqual(&expected));

  EXPECT_TRUE(rdata->IsAlias());
  AliasFormHttpsRecordRdata* alias_rdata = rdata->AsAliasForm();
  ASSERT_TRUE(alias_rdata);
  EXPECT_TRUE(alias_rdata->alias_name().empty());
}

TEST(HttpsRecordRdataTest, IgnoreAliasParams) {
  const char kRdata[] =
      // Priority: 0 for alias record
      "\000\000"
      // Alias name: chromium.org
      "\010chromium\003org\000"
      // no-default-alpn
      "\000\002\000\000";

  std::unique_ptr<HttpsRecordRdata> rdata =
      HttpsRecordRdata::Parse(std::string_view(kRdata, sizeof(kRdata) - 1));
  ASSERT_TRUE(rdata);

  AliasFormHttpsRecordRdata expected("chromium.org");
  EXPECT_TRUE(rdata->IsEqual(&expected));

  EXPECT_TRUE(rdata->IsAlias());
  AliasFormHttpsRecordRdata* alias_rdata = rdata->AsAliasForm();
  ASSERT_TRUE(alias_rdata);
  EXPECT_EQ(alias_rdata->alias_name(), "chromium.org");
}

TEST(HttpsRecordRdataTest, ParsesService) {
  const char kRdata[] =
      // Priority: 1
      "\000\001"
      // Service name: chromium.org
      "\010chromium\003org\000"
      // mandatory=alpn,no-default-alpn,port,ipv4hint,echconfig,ipv6hint
      "\000\000\000\014\000\001\000\002\000\003\000\004\000\005\000\006"
      // alpn=foo,bar
      "\000\001\000\010\003foo\003bar"
      // no-default-alpn
      "\000\002\000\000"
      // port=46
      "\000\003\000\002\000\056"
      // ipv4hint=8.8.8.8
      "\000\004\000\004\010\010\010\010"
      // echconfig=hello
      "\000\005\000\005hello"
      // ipv6hint=2001:4860:4860::8888
      "\000\006\000\020\x20\x01\x48\x60\x48\x60\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x88\x88"
      // Unknown key7=foo
      "\000\007\000\003foo";

  std::unique_ptr<HttpsRecordRdata> rdata =
      HttpsRecordRdata::Parse(std::string_view(kRdata, sizeof(kRdata) - 1));
  ASSERT_TRUE(rdata);

  IPAddress expected_ipv6;
  ASSERT_TRUE(expected_ipv6.AssignFromIPLiteral("2001:4860:4860::8888"));
  ServiceFormHttpsRecordRdata expected(
      1 /* priority */, "chromium.org", std::set<uint16_t>({1, 2, 3, 4, 5, 6}),
      std::vector<std::string>({"foo", "bar"}) /* alpn_ids */,
      false /* default_alpn */, std::optional<uint16_t>(46) /* port */,
      std::vector<IPAddress>({IPAddress(8, 8, 8, 8)}) /* ipv4_hint */,
      "hello" /* ech_config */,
      std::vector<IPAddress>({expected_ipv6}) /* ipv6_hint */,
      std::map<uint16_t, std::string>({{7, "foo"}}) /* unparsed_params */);
  EXPECT_TRUE(rdata->IsEqual(&expected));

  EXPECT_FALSE(rdata->IsAlias());
  ServiceFormHttpsRecordRdata* service_rdata = rdata->AsServiceForm();
  ASSERT_TRUE(service_rdata);
  EXPECT_EQ(service_rdata->priority(), 1);
  EXPECT_EQ(service_rdata->service_name(), "chromium.org");
  EXPECT_THAT(service_rdata->mandatory_keys(),
              testing::ElementsAre(1, 2, 3, 4, 5, 6));
  EXPECT_THAT(service_rdata->alpn_ids(), testing::ElementsAre("foo", "bar"));
  EXPECT_FALSE(service_rdata->default_alpn());
  EXPECT_THAT(service_rdata->port(), testing::Optional(46));
  EXPECT_THAT(service_rdata->ipv4_hint(),
              testing::ElementsAre(IPAddress(8, 8, 8, 8)));
  EXPECT_EQ(service_rdata->ech_config(), "hello");
  EXPECT_THAT(service_rdata->ipv6_hint(), testing::ElementsAre(expected_ipv6));
  EXPECT_THAT(service_rdata->unparsed_params(),
              testing::ElementsAre(testing::Pair(7, "foo")));
  EXPECT_TRUE(service_rdata->IsCompatible());
}

TEST(HttpsRecordRdataTest, RejectCorruptRdata) {
  const char kRdata[] =
      // Priority: 5
      "\000\005"
      // Service name: chromium.org
      "\010chromium\003org\000"
      // Malformed alpn
      "\000\001\000\005hi";

  std::unique_ptr<HttpsRecordRdata> rdata =
      HttpsRecordRdata::Parse(std::string_view(kRdata, sizeof(kRdata) - 1));
  EXPECT_FALSE(rdata);
}

TEST(HttpsRecordRdataTest, AliasIsEqualRejectsWrongType) {
  AliasFormHttpsRecordRdata alias("alias.name.test");
  ServiceFormHttpsRecordRdata service(
      1u /* priority */, "service.name.test", {} /* mandatory_keys */,
      {} /* alpn_ids */, true /* default_alpn */, std::nullopt /* port */,
      {} /* ipv4_hint */, "" /* ech_config */, {} /* ipv6_hint */,
      {} /* unparsed_params */);

  EXPECT_TRUE(alias.IsEqual(&alias));
  EXPECT_FALSE(alias.IsEqual(&service));
}

TEST(HttpsRecordRdataTest, ServiceIsEqualRejectsWrongType) {
  AliasFormHttpsRecordRdata alias("alias.name.test");
  ServiceFormHttpsRecordRdata service(
      1u /* priority */, "service.name.test", {} /* mandatory_keys */,
      {} /* alpn_ids */, true /* default_alpn */, std::nullopt /* port */,
      {} /* ipv4_hint */, "" /* ech_config */, {} /* ipv6_hint */,
      {} /* unparsed_params */);

  EXPECT_FALSE(service.IsEqual(&alias));
  EXPECT_TRUE(service.IsEqual(&service));
}

}  // namespace
}  // namespace net
