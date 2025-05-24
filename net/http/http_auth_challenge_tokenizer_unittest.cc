// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_challenge_tokenizer.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthChallengeTokenizerTest, Basic) {
  HttpAuthChallengeTokenizer challenge("Basic realm=\"foobar\"");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("basic", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("foobar", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with no quote marks.
TEST(HttpAuthChallengeTokenizerTest, NoQuotes) {
  HttpAuthChallengeTokenizer challenge("Basic realm=foobar@baz.com");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("basic", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("foobar@baz.com", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotes) {
  HttpAuthChallengeTokenizer challenge("Basic realm=\"foobar@baz.com");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("basic", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("foobar@baz.com", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property without a value and with mismatching quote marks.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesNoValue) {
  HttpAuthChallengeTokenizer challenge("Basic realm=\"");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("basic", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks and spaces in the
// value.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesSpaces) {
  HttpAuthChallengeTokenizer challenge("Basic realm=\"foo bar");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("basic", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("foo bar", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use multiple name=value properties with mismatching quote marks in the last
// value.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesMultiple) {
  HttpAuthChallengeTokenizer challenge(
      "Digest qop=auth-int, algorithm=md5, realm=\"foo");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("digest", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("qop", parameters.name());
  EXPECT_EQ("auth-int", parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("algorithm", parameters.name());
  EXPECT_EQ("md5", parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("foo", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property which has no value.
TEST(HttpAuthChallengeTokenizerTest, NoValue) {
  HttpAuthChallengeTokenizer challenge("Digest qop=");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("digest"), challenge.auth_scheme());
  EXPECT_FALSE(parameters.GetNext());
  EXPECT_FALSE(parameters.valid());
}

// Specify multiple properties, comma separated.
TEST(HttpAuthChallengeTokenizerTest, Multiple) {
  HttpAuthChallengeTokenizer challenge(
      "Digest algorithm=md5, realm=\"Oblivion\", qop=auth-int");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("digest", challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("algorithm", parameters.name());
  EXPECT_EQ("md5", parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("realm", parameters.name());
  EXPECT_EQ("Oblivion", parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ("qop", parameters.name());
  EXPECT_EQ("auth-int", parameters.value());
  EXPECT_FALSE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
}

// Use a challenge which has no property.
TEST(HttpAuthChallengeTokenizerTest, NoProperty) {
  HttpAuthChallengeTokenizer challenge("NTLM");
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("ntlm"), challenge.auth_scheme());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a challenge with Base64 encoded token.
TEST(HttpAuthChallengeTokenizerTest, Base64) {
  HttpAuthChallengeTokenizer challenge("NTLM  SGVsbG8sIFdvcmxkCg===");

  EXPECT_EQ(std::string("ntlm"), challenge.auth_scheme());
  // Notice the two equal statements below due to padding removal.
  EXPECT_EQ(std::string("SGVsbG8sIFdvcmxkCg=="), challenge.base64_param());
}

}  // namespace net
