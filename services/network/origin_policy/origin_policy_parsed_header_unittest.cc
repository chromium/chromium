// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parsed_header.h"
#include "services/network/origin_policy/origin_policy_header_values.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for OriginPolicyParsedHeader.
//
// Note that we're not trying to test the structured header implementation,
// so we don't test invalid syntax cases (e.g. strings outside the structured
// header string range). This is about the translation from valid structured
// header syntax into the parsed representation, i.e. we're testing the
// algorithm at
// https://wicg.github.io/origin-policy/#parse-an-origin-policy-header.

namespace network {

// In normal code we can assume that if a header value's is_string() is true,
// then its is_latest() and is_null() are false. Similarly for the other
// invariants. But this is a test, so let's use this opportunity to test that
// all those invariants were implemented correctly.

void AssertAllowedIsString(const OriginPolicyAllowedValue& value,
                           const std::string& string) {
  ASSERT_FALSE(value.is_latest());
  ASSERT_FALSE(value.is_null());
  ASSERT_TRUE(value.is_string());
  ASSERT_EQ(value.string(), string);
}

void AssertAllowedIsNull(const OriginPolicyAllowedValue& value) {
  ASSERT_FALSE(value.is_latest());
  ASSERT_TRUE(value.is_null());
  ASSERT_FALSE(value.is_string());
}

void AssertAllowedIsLatest(const OriginPolicyAllowedValue& value) {
  ASSERT_TRUE(value.is_latest());
  ASSERT_FALSE(value.is_null());
  ASSERT_FALSE(value.is_string());
}

void AssertPreferredIsString(
    const absl::optional<OriginPolicyPreferredValue>& value,
    const std::string& string) {
  ASSERT_TRUE(value.has_value());
  ASSERT_FALSE(value->is_latest_from_network());
  ASSERT_TRUE(value->is_string());
  ASSERT_EQ(value->string(), string);
}

void AssertPreferredIsLatestFromNetwork(
    const absl::optional<OriginPolicyPreferredValue>& value) {
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_latest_from_network());
  ASSERT_FALSE(value->is_string());
}

TEST(OriginPolicyParsedHeader, Empty) {
  auto parsed = OriginPolicyParsedHeader::FromString("");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, TokenNotDictionary) {
  auto parsed = OriginPolicyParsedHeader::FromString("latest");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, ListNotDictionary) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed, preferred");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, StringNotDictionary) {
  auto parsed = OriginPolicyParsedHeader::FromString("\"1\"");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedEmpty) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=()");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedEmptyString) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=(\"\")");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedValid) {
  auto parsed =
      OriginPolicyParsedHeader::FromString("allowed=(\"1\" null \"2\" latest)");

  ASSERT_TRUE(parsed.has_value());

  ASSERT_FALSE(parsed->preferred().has_value());

  ASSERT_EQ(parsed->allowed().size(), 4U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsNull(parsed->allowed()[1]);
  AssertAllowedIsString(parsed->allowed()[2], "2");
  AssertAllowedIsLatest(parsed->allowed()[3]);
}

TEST(OriginPolicyParsedHeader, AllowedDuplicates) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(latest \"1\" null \"1\" \"2\" null \"2\" latest)");

  ASSERT_TRUE(parsed.has_value());

  ASSERT_FALSE(parsed->preferred().has_value());

  ASSERT_EQ(parsed->allowed().size(), 4U);
  AssertAllowedIsLatest(parsed->allowed()[0]);
  AssertAllowedIsString(parsed->allowed()[1], "1");
  AssertAllowedIsNull(parsed->allowed()[2]);
  AssertAllowedIsString(parsed->allowed()[3], "2");
}

TEST(OriginPolicyParsedHeader, AllowedString) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=\"1\"");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedToken) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=latest");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedListWithWrongToken) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" latest-from-network \"2\")");

  ASSERT_TRUE(parsed.has_value());

  ASSERT_FALSE(parsed->preferred().has_value());

  ASSERT_EQ(parsed->allowed().size(), 2U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
}

TEST(OriginPolicyParsedHeader, AllowedListWithBoolean) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=(\"1\" ?0)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedListWithInteger) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=(\"1\" 42)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedListWithDecimal) {
  auto parsed = OriginPolicyParsedHeader::FromString("allowed=(\"1\" 2.5)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, AllowedListWithByteSequence) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" :cHJldGVuZCB0aGlzIGlzIGJpbmFyeSBjb250ZW50Lg==:)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, PreferredStringValid) {
  auto parsed = OriginPolicyParsedHeader::FromString("preferred=\"1\"");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsString(parsed->preferred(), "1");

  ASSERT_EQ(parsed->allowed().size(), 0U);
}

TEST(OriginPolicyParsedHeader, PreferredLatestFromNetworkValid) {
  auto parsed =
      OriginPolicyParsedHeader::FromString("preferred=latest-from-network");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 0U);
}

TEST(OriginPolicyParsedHeader, PreferredList) {
  auto parsed = OriginPolicyParsedHeader::FromString("preferred=(\"1\")");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, PreferredOtherToken) {
  auto parsed = OriginPolicyParsedHeader::FromString("preferred=latest");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, PreferredEmptyString) {
  auto parsed = OriginPolicyParsedHeader::FromString("preferred=\"\"");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, CombinedValidEmptyAllowedStringPreferred) {
  auto parsed =
      OriginPolicyParsedHeader::FromString("allowed=(), preferred=\"1\"");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsString(parsed->preferred(), "1");

  ASSERT_EQ(parsed->allowed().size(), 0U);
}

TEST(OriginPolicyParsedHeader,
     CombinedValidEmptyAllowedLatestFromNetworkPreferred) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(), preferred=latest-from-network");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 0U);
}

TEST(OriginPolicyParsedHeader, CombinedValidNonemptyAllowedStringPreferred) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" \"2\" latest), preferred=\"1\"");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsString(parsed->preferred(), "1");

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader,
     CombinedValidNonemptyAllowedLatestFromNetworkPreferred) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" \"2\" latest), preferred=latest-from-network");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader, CombinedValidExtraDictionaryEntriesIgnored) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" \"2\" latest), preferred=latest-from-network, foo=bar, "
      "baz=?0");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader, CombinedValidParametersIgnored) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\";param-a=1;param-b=?0 \"2\" latest);param-c=\"x\", "
      "preferred=latest-from-network;param-d=y");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader, CombinedValidUnknownAllowedTokenIgnored) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" unknown-token \"2\" latest-from-network latest), "
      "preferred=latest-from-network");

  ASSERT_TRUE(parsed.has_value());

  AssertPreferredIsLatestFromNetwork(parsed->preferred());

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader, CombinedValidUnknownPreferredTokenIgnored) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "allowed=(\"1\" \"2\" latest), "
      "preferred=unknown-token");

  ASSERT_TRUE(parsed.has_value());

  ASSERT_FALSE(parsed->preferred().has_value());

  ASSERT_EQ(parsed->allowed().size(), 3U);
  AssertAllowedIsString(parsed->allowed()[0], "1");
  AssertAllowedIsString(parsed->allowed()[1], "2");
  AssertAllowedIsLatest(parsed->allowed()[2]);
}

TEST(OriginPolicyParsedHeader, CombinedInvalidPreferredNumber) {
  auto parsed =
      OriginPolicyParsedHeader::FromString("preferred=?0, allowed=(latest)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, CombinedInvalidAllowedNumber) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "preferred=latest-from-network, allowed=(\"1\" ?0)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, CombinedInvalidPreferredEmptyString) {
  auto parsed =
      OriginPolicyParsedHeader::FromString("preferred=\"\", allowed=(latest)");

  ASSERT_FALSE(parsed.has_value());
}

TEST(OriginPolicyParsedHeader, CombinedInvalidAllowedEmptyString) {
  auto parsed = OriginPolicyParsedHeader::FromString(
      "preferred=latest-from-network, allowed=(\"1\" \"\")");

  ASSERT_FALSE(parsed.has_value());
}

}  // namespace network
