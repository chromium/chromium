// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/http_auth_negotiate_android.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/android/dummy_spnego_authenticator.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/log/net_log_with_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace android {

TEST(HttpAuthNegotiateAndroidTest, GenerateAuthToken) {
  base::test::TaskEnvironment task_environment;

  DummySpnegoAuthenticator::EnsureTestAccountExists();

  std::string auth_token;

  DummySpnegoAuthenticator authenticator;
  net::test::GssContextMockImpl mockContext;
  authenticator.ExpectSecurityContext("Negotiate", GSS_S_COMPLETE, 0,
                                      mockContext, "", "DummyToken");

  MockAllowHttpAuthPreferences prefs;
  prefs.set_auth_android_negotiate_account_type(
      "org.chromium.test.DummySpnegoAuthenticator");
  HttpAuthNegotiateAndroid auth(&prefs);
  EXPECT_TRUE(auth.Init(NetLogWithSource()));

  TestCompletionCallback callback;
  EXPECT_EQ(OK, callback.GetResult(auth.GenerateAuthToken(
                    nullptr, "Dummy", std::string(), &auth_token,
                    NetLogWithSource(), callback.callback())));

  EXPECT_EQ("Negotiate DummyToken", auth_token);

  DummySpnegoAuthenticator::RemoveTestAccounts();
}

TEST(HttpAuthNegotiateAndroidTest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  MockAllowHttpAuthPreferences prefs;
  prefs.set_auth_android_negotiate_account_type(
      "org.chromium.test.DummySpnegoAuthenticator");
  HttpAuthNegotiateAndroid auth(&prefs);
  std::string challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer challenge(challenge_text.begin(),
                                       challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth.ParseChallenge(&challenge));
}

TEST(HttpAuthNegotiateAndroidTest, ParseChallenge_UnexpectedTokenFirstRound) {
  // If the first round challenge has an additional authentication token, it
  // should be treated as an invalid challenge from the server.
  MockAllowHttpAuthPreferences prefs;
  prefs.set_auth_android_negotiate_account_type(
      "org.chromium.test.DummySpnegoAuthenticator");
  HttpAuthNegotiateAndroid auth(&prefs);
  std::string challenge_text = "Negotiate Zm9vYmFy";
  HttpAuthChallengeTokenizer challenge(challenge_text.begin(),
                                       challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth.ParseChallenge(&challenge));
}

TEST(HttpAuthNegotiateAndroidTest, ParseChallenge_TwoRounds) {
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  MockAllowHttpAuthPreferences prefs;
  prefs.set_auth_android_negotiate_account_type(
      "org.chromium.test.DummySpnegoAuthenticator");
  HttpAuthNegotiateAndroid auth(&prefs);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge(first_challenge_text.begin(),
                                             first_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth.ParseChallenge(&first_challenge));

  std::string second_challenge_text = "Negotiate Zm9vYmFy";
  HttpAuthChallengeTokenizer second_challenge(second_challenge_text.begin(),
                                              second_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth.ParseChallenge(&second_challenge));
}

TEST(HttpAuthNegotiateAndroidTest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  MockAllowHttpAuthPreferences prefs;
  prefs.set_auth_android_negotiate_account_type(
      "org.chromium.test.DummySpnegoAuthenticator");
  HttpAuthNegotiateAndroid auth(&prefs);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge(first_challenge_text.begin(),
                                             first_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth.ParseChallenge(&first_challenge));

  std::string second_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer second_challenge(second_challenge_text.begin(),
                                              second_challenge_text.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            auth.ParseChallenge(&second_challenge));
}

}  // namespace android
}  // namespace net
