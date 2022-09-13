// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_request_signer.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// This value is used to seed the PRNG at the beginning of a sequence of
// operations to produce a repeatable sequence.
#define RANDOM_SEED (0x69E3C47D)

TEST(OAuthRequestSignerTest, Encode) {
  ASSERT_EQ(OAuthRequestSigner::Encode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz"
                                       "0123456789"
                                       "-._~"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789"
            "-._~");
  ASSERT_EQ(OAuthRequestSigner::Encode(
                "https://accounts.google.com/OAuthLogin"),
            "https%3A%2F%2Faccounts.google.com%2FOAuthLogin");
  ASSERT_EQ(OAuthRequestSigner::Encode("%"), "%25");
  ASSERT_EQ(OAuthRequestSigner::Encode("%25"), "%2525");
  ASSERT_EQ(OAuthRequestSigner::Encode(
                "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
                "do eiusmod tempor incididunt ut labore et dolore magna "
                "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
                "ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis "
                "aute irure dolor in reprehenderit in voluptate velit esse "
                "cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                "occaecat cupidatat non proident, sunt in culpa qui officia "
                "deserunt mollit anim id est laborum."),
            "Lorem%20ipsum%20dolor%20sit%20amet%2C%20consectetur%20"
            "adipisicing%20elit%2C%20sed%20do%20eiusmod%20tempor%20"
            "incididunt%20ut%20labore%20et%20dolore%20magna%20aliqua.%20Ut%20"
            "enim%20ad%20minim%20veniam%2C%20quis%20nostrud%20exercitation%20"
            "ullamco%20laboris%20nisi%20ut%20aliquip%20ex%20ea%20commodo%20"
            "consequat.%20Duis%20aute%20irure%20dolor%20in%20reprehenderit%20"
            "in%20voluptate%20velit%20esse%20cillum%20dolore%20eu%20fugiat%20"
            "nulla%20pariatur.%20Excepteur%20sint%20occaecat%20cupidatat%20"
            "non%20proident%2C%20sunt%20in%20culpa%20qui%20officia%20"
            "deserunt%20mollit%20anim%20id%20est%20laborum.");
  ASSERT_EQ(OAuthRequestSigner::Encode("!5}&QF~0R-Ecy[?2Cig>6g=;hH!\\Ju4K%UK;"),
            "%215%7D%26QF~0R-Ecy%5B%3F2Cig%3E6g%3D%3BhH%21%5CJu4K%25UK%3B");
  ASSERT_EQ(OAuthRequestSigner::Encode("1UgHf(r)SkMRS`fRZ/8PsTcXT0:\\<9I=6{|:"),
            "1UgHf%28r%29SkMRS%60fRZ%2F8PsTcXT0%3A%5C%3C9I%3D6%7B%7C%3A");
  ASSERT_EQ(OAuthRequestSigner::Encode("|<XIy1?o`r\"RuGSX#!:MeP&RLZQM@:\\';2X"),
            "%7C%3CXIy1%3Fo%60r%22RuGSX%23%21%3AMeP%26RLZQM%40%3A%5C%27%3B2X");
  ASSERT_EQ(OAuthRequestSigner::Encode("#a@A>ZtcQ/yb.~^Q_]daRT?ffK>@A:afWuZL"),
            "%23a%40A%3EZtcQ%2Fyb.~%5EQ_%5DdaRT%3FffK%3E%40A%3AafWuZL");
}

// http://crbug.com/685352
#if BUILDFLAG(IS_WIN)
#define MAYBE_DecodeEncoded DISABLED_DecodeEncoded
#else
#define MAYBE_DecodeEncoded DecodeEncoded
#endif
TEST(OAuthRequestSignerTest, MAYBE_DecodeEncoded) {
  srand(RANDOM_SEED);
  static const int kIterations = 500;
  static const int kLengthLimit = 500;
  for (int iteration = 0; iteration < kIterations; ++iteration) {
    std::string text;
    int length = rand() % kLengthLimit;
    for (int position = 0; position < length; ++position) {
      text += static_cast<char>(rand() % 256);
    }
    std::string encoded = OAuthRequestSigner::Encode(text);
    std::string decoded;
    ASSERT_TRUE(OAuthRequestSigner::Decode(encoded, &decoded));
    ASSERT_EQ(decoded, text);
  }
}

TEST(OAuthRequestSignerTest, SignGet1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["scope"] = "https://accounts.google.com/OAuthLogin";
  parameters["oauth_nonce"] = "2oiE_aHdk5qRTz0L9C8Lq0g";
  parameters["xaouth_display_name"] = "Chromium";
  parameters["oauth_timestamp"] = "1308152953";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::SignURL(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "johndoe",  // oauth_consumer_key
                  "53cR3t",  // consumer secret
                  "4/VGY0MsQadcmO8VnCv9gnhoEooq1v",  // oauth_token
                  "c5e0531ff55dfbb4054e", // token secret
                  &signed_text));
  ASSERT_EQ("https://www.google.com/accounts/o8/GetOAuthToken"
            "?oauth_consumer_key=johndoe"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature=PFqDTaiyey1UObcvOyI4Ng2HXW0%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FVGY0MsQadcmO8VnCv9gnhoEooq1v"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Faccounts.google.com%2FOAuthLogin"
            "&xaouth_display_name=Chromium",
            signed_text);
}

TEST(OAuthRequestSignerTest, SignGet2) {
  GURL request_url("https://accounts.google.com/OAuthGetAccessToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["oauth_timestamp"] = "1308147831";
  parameters["oauth_nonce"] = "4d4hZW9DygWQujP2tz06UN";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::SignURL(
      request_url,
      parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::GET_METHOD,
      "anonymous",                       // oauth_consumer_key
      "anonymous",                       // consumer secret
      "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
      std::string(),                     // token secret
      &signed_text));
  ASSERT_EQ(signed_text,
            "https://accounts.google.com/OAuthGetAccessToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=4d4hZW9DygWQujP2tz06UN"
            "&oauth_signature=YiJv%2BEOWsvCDCi13%2FhQBFrr0J7c%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308147831"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0");
}

TEST(OAuthRequestSignerTest, ParseAndSignGet1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken"
                   "?scope=https://accounts.google.com/OAuthLogin"
                   "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
                   "&xaouth_display_name=Chromium"
                   "&oauth_timestamp=1308152953");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
      request_url,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::GET_METHOD,
      "anonymous",                       // oauth_consumer_key
      "anonymous",                       // consumer secret
      "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
      std::string(),                     // token secret
      &signed_text));
  ASSERT_EQ("https://www.google.com/accounts/o8/GetOAuthToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature=PH7KP6cP%2BzZ1SJ6WGqBgXwQP9Mc%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Faccounts.google.com%2FOAuthLogin"
            "&xaouth_display_name=Chromium",
            signed_text);
}

TEST(OAuthRequestSignerTest, ParseAndSignGet2) {
  GURL request_url("https://accounts.google.com/OAuthGetAccessToken"
                   "?oauth_timestamp=1308147831"
                   "&oauth_nonce=4d4hZW9DygWQujP2tz06UN");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
      request_url,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::GET_METHOD,
      "anonymous",                       // oauth_consumer_key
      "anonymous",                       // consumer secret
      "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
      std::string(),                     // token secret
      &signed_text));
  ASSERT_EQ(signed_text,
            "https://accounts.google.com/OAuthGetAccessToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=4d4hZW9DygWQujP2tz06UN"
            "&oauth_signature=YiJv%2BEOWsvCDCi13%2FhQBFrr0J7c%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308147831"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0");
}

TEST(OAuthRequestSignerTest, SignPost1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["scope"] = "https://accounts.google.com/OAuthLogin";
  parameters["oauth_nonce"] = "2oiE_aHdk5qRTz0L9C8Lq0g";
  parameters["xaouth_display_name"] = "Chromium";
  parameters["oauth_timestamp"] = "1308152953";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::SignURL(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/X8x0r7bHif_VNCLjUMutxGkzo13d",  // oauth_token
                  "b7120598d47594bd3522", // token secret
                  &signed_text));
  ASSERT_EQ("oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature=vVlfv6dnV2%2Fx7TozS0Gf83zS2%2BQ%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FX8x0r7bHif_VNCLjUMutxGkzo13d"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Faccounts.google.com%2FOAuthLogin"
            "&xaouth_display_name=Chromium",
            signed_text);
}

TEST(OAuthRequestSignerTest, SignPost2) {
  GURL request_url("https://accounts.google.com/OAuthGetAccessToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["oauth_timestamp"] = "1234567890";
  parameters["oauth_nonce"] = "17171717171717171";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::SignURL(
      request_url,
      parameters,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::POST_METHOD,
      "anonymous",                       // oauth_consumer_key
      "anonymous",                       // consumer secret
      "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
      std::string(),                     // token secret
      &signed_text));
  ASSERT_EQ(signed_text,
            "oauth_consumer_key=anonymous"
            "&oauth_nonce=17171717171717171"
            "&oauth_signature=tPX2XqKQICWzopZ80CFGX%2F53DLo%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1234567890"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0");
}

TEST(OAuthRequestSignerTest, ParseAndSignPost1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken"
                   "?scope=https://accounts.google.com/OAuthLogin"
                   "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
                   "&xaouth_display_name=Chromium"
                   "&oauth_timestamp=1308152953");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
                  request_url,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/X8x0r7bHif_VNCLjUMutxGkzo13d",  // oauth_token
                  "b7120598d47594bd3522", // token secret
                  &signed_text));
  ASSERT_EQ("oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature=vVlfv6dnV2%2Fx7TozS0Gf83zS2%2BQ%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FX8x0r7bHif_VNCLjUMutxGkzo13d"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Faccounts.google.com%2FOAuthLogin"
            "&xaouth_display_name=Chromium",
            signed_text);
}

TEST(OAuthRequestSignerTest, ParseAndSignPost2) {
  GURL request_url("https://accounts.google.com/OAuthGetAccessToken"
                   "?oauth_timestamp=1234567890"
                   "&oauth_nonce=17171717171717171");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
      request_url,
      OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
      OAuthRequestSigner::POST_METHOD,
      "anonymous",                       // oauth_consumer_key
      "anonymous",                       // consumer secret
      "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
      std::string(),                     // token secret
      &signed_text));
  ASSERT_EQ(signed_text,
            "oauth_consumer_key=anonymous"
            "&oauth_nonce=17171717171717171"
            "&oauth_signature=tPX2XqKQICWzopZ80CFGX%2F53DLo%3D"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1234567890"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0");
}

TEST(OAuthRequestSignerTest, SignAuthHeader) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["scope"] = "https://accounts.google.com/OAuthLogin";
  parameters["oauth_nonce"] = "2oiE_aHdk5qRTz0L9C8Lq0g";
  parameters["xaouth_display_name"] = "Chromium";
  parameters["oauth_timestamp"] = "1308152953";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::SignAuthHeader(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "johndoe",  // oauth_consumer_key
                  "53cR3t",  // consumer secret
                  "4/VGY0MsQadcmO8VnCv9gnhoEooq1v",  // oauth_token
                  "c5e0531ff55dfbb4054e", // token secret
                  &signed_text));
  ASSERT_EQ("OAuth "
            "oauth_consumer_key=\"johndoe\", "
            "oauth_nonce=\"2oiE_aHdk5qRTz0L9C8Lq0g\", "
            "oauth_signature=\"PFqDTaiyey1UObcvOyI4Ng2HXW0%3D\", "
            "oauth_signature_method=\"HMAC-SHA1\", "
            "oauth_timestamp=\"1308152953\", "
            "oauth_token=\"4%2FVGY0MsQadcmO8VnCv9gnhoEooq1v\", "
            "oauth_version=\"1.0\", "
            "scope=\"https%3A%2F%2Faccounts.google.com%2FOAuthLogin\", "
            "xaouth_display_name=\"Chromium\"",
            signed_text);
}
