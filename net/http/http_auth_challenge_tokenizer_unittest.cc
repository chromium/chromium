// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_challenge_tokenizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthChallengeTokenizerTest, Basic) {
  std::string challenge_str = "Basic realm=\"foobar\"";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("basic"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with no quote marks.
TEST(HttpAuthChallengeTokenizerTest, NoQuotes) {
  std::string challenge_str = "Basic realm=foobar@baz.com";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("basic"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotes) {
  std::string challenge_str = "Basic realm=\"foobar@baz.com";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("basic"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property without a value and with mismatching quote marks.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesNoValue) {
  std::string challenge_str = "Basic realm=\"";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("basic"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string(), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks and spaces in the
// value.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesSpaces) {
  std::string challenge_str = "Basic realm=\"foo bar";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("basic"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foo bar"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use multiple name=value properties with mismatching quote marks in the last
// value.
TEST(HttpAuthChallengeTokenizerTest, MismatchedQuotesMultiple) {
  std::string challenge_str = "Digest qop=auth-int, algorithm=md5, realm=\"foo";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("digest"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("qop"), parameters.name());
  EXPECT_EQ(std::string("auth-int"), parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("algorithm"), parameters.name());
  EXPECT_EQ(std::string("md5"), parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foo"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property which has no value.
TEST(HttpAuthChallengeTokenizerTest, NoValue) {
  std::string challenge_str = "Digest qop=";
  HttpAuthChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("digest"), challenge.auth_scheme());
  EXPECT_FALSE(parameters.GetNext());
  EXPECT_FALSE(parameters.valid());
}

// Specify multiple properties, comma separated.
TEST(HttpAuthChallengeTokenizerTest, Multiple) {
  std::string challenge_str =
      "Digest algorithm=md5, realm=\"Oblivion\", qop=auth-int";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("digest"), challenge.auth_scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("algorithm"), parameters.name());
  EXPECT_EQ(std::string("md5"), parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("Oblivion"), parameters.value());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("qop"), parameters.name());
  EXPECT_EQ(std::string("auth-int"), parameters.value());
  EXPECT_FALSE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
}

// Use a challenge which has no property.
TEST(HttpAuthChallengeTokenizerTest, NoProperty) {
  std::string challenge_str = "NTLM";
  HttpAuthChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("ntlm"), challenge.auth_scheme());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a challenge with Base64 encoded token.
TEST(HttpAuthChallengeTokenizerTest, Base64) {
  std::string challenge_str = "NTLM  SGVsbG8sIFdvcmxkCg===";
  HttpAuthChallengeTokenizer challenge(challenge_str.begin(),
                                       challenge_str.end());

  EXPECT_EQ(std::string("ntlm"), challenge.auth_scheme());
  // Notice the two equal statements below due to padding removal.
  EXPECT_EQ(std::string("SGVsbG8sIFdvcmxkCg=="), challenge.base64_param());
}

}  // namespace net
