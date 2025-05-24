// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

TEST(HttpAuthHandlerTest, NetLog) {
  base::test::TaskEnvironment task_environment;

  url::SchemeHostPort scheme_host_port(GURL("http://www.example.com"));
  std::string challenge = "Mock asdf";
  AuthCredentials credentials(u"user", u"pass");
  std::string auth_token;
  HttpRequestInfo request;

  for (auto async : {true, false}) {
    for (auto target : {HttpAuth::AUTH_PROXY, HttpAuth::AUTH_SERVER}) {
      TestCompletionCallback test_callback;
      HttpAuthChallengeTokenizer tokenizer(challenge);
      HttpAuthHandlerMock mock_handler;
      RecordingNetLogObserver net_log_observer;

      // set_connection_based(true) indicates that the HandleAnotherChallenge()
      // call after GenerateAuthToken() is expected and does not result in
      // AUTHORIZATION_RESULT_REJECT.
      mock_handler.set_connection_based(true);
      mock_handler.InitFromChallenge(
          &tokenizer, target, SSLInfo(), NetworkAnonymizationKey(),
          scheme_host_port, NetLogWithSource::Make(NetLogSourceType::NONE));
      mock_handler.SetGenerateExpectation(async, OK);
      mock_handler.GenerateAuthToken(&credentials, &request,
                                     test_callback.callback(), &auth_token);
      if (async)
        test_callback.WaitForResult();

      mock_handler.HandleAnotherChallenge(&tokenizer);

      auto entries = net_log_observer.GetEntries();

      ASSERT_EQ(5u, entries.size());
      EXPECT_TRUE(LogContainsBeginEvent(entries, 0,
                                        NetLogEventType::AUTH_HANDLER_INIT));
      EXPECT_TRUE(
          LogContainsEndEvent(entries, 1, NetLogEventType::AUTH_HANDLER_INIT));
      EXPECT_TRUE(LogContainsBeginEvent(entries, 2,
                                        NetLogEventType::AUTH_GENERATE_TOKEN));
      EXPECT_TRUE(LogContainsEndEvent(entries, 3,
                                      NetLogEventType::AUTH_GENERATE_TOKEN));
      EXPECT_TRUE(LogContainsEntryWithType(
          entries, 4, NetLogEventType::AUTH_HANDLE_CHALLENGE));
    }
  }
}

}  // namespace net
