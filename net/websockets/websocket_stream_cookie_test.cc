// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "net/socket/socket_test_util.h"
#include "net/websockets/websocket_stream_create_test_base.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

using ::testing::TestWithParam;
using ::testing::ValuesIn;

const char kNoCookieHeader[] = "";

class TestBase : public WebSocketStreamCreateTestBase {
 public:
  void CreateAndConnect(const GURL& url,
                        const url::Origin& origin,
                        const GURL& site_for_cookies,
                        const net::NetworkIsolationKey& network_isolation_key,
                        const std::string& cookie_header,
                        const std::string& response_body) {
    // We assume cookie_header ends with CRLF if not empty, as
    // WebSocketStandardRequestWithCookies requires. Use AddCRLFIfNotEmpty
    // in a call site.
    CHECK(cookie_header.empty() ||
          base::EndsWith(cookie_header, "\r\n", base::CompareCase::SENSITIVE));

    url_request_context_host_.SetExpectations(
        WebSocketStandardRequestWithCookies(url.path(), url.host(), origin,
                                            cookie_header, std::string(),
                                            std::string()),
        response_body);
    CreateAndConnectStream(url, NoSubProtocols(), origin, site_for_cookies,
                           network_isolation_key, HttpRequestHeaders(),
                           nullptr);
  }

  std::string AddCRLFIfNotEmpty(const std::string& s) {
    return s.empty() ? s : s + "\r\n";
  }
};

struct ClientUseCookieParameter {
  // The URL for the WebSocket connection.
  const char* const url;
  // The URL for the previously set cookies.
  const char* const cookie_url;
  // The previously set cookies contents.
  const char* const cookie_line;
  // The Cookie: HTTP header expected to appear in the WS request. An empty
  // string means there is no Cookie: header.
  const char* const cookie_header;
};

class WebSocketStreamClientUseCookieTest
    : public TestBase,
      public TestWithParam<ClientUseCookieParameter> {
 public:
  ~WebSocketStreamClientUseCookieTest() override {
    // Permit any endpoint locks to be released.
    stream_request_.reset();
    stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  static void SetCookieHelperFunction(
      const base::RepeatingClosure& task,
      base::WeakPtr<bool> weak_is_called,
      base::WeakPtr<bool> weak_result,
      CanonicalCookie::CookieInclusionStatus status) {
    *weak_is_called = true;
    *weak_result = status.IsInclude();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
  }
};

struct ServerSetCookieParameter {
  // The URL for the WebSocket connection.
  const char* const url;
  // The URL used to query cookies after the response received.
  const char* const cookie_url;
  // The cookies expected to appear for |cookie_url| inquiry.
  const char* const cookie_line;
  // The Set-Cookie: HTTP header attached to the response.
  const char* const cookie_header;
};

class WebSocketStreamServerSetCookieTest
    : public TestBase,
      public TestWithParam<ServerSetCookieParameter> {
 public:
  ~WebSocketStreamServerSetCookieTest() override {
    // Permit any endpoint locks to be released.
    stream_request_.reset();
    stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  static void GetCookieListHelperFunction(
      base::OnceClosure task,
      base::WeakPtr<bool> weak_is_called,
      base::WeakPtr<CookieList> weak_result,
      const CookieStatusList& cookie_list,
      const CookieStatusList& excluded_cookies) {
    *weak_is_called = true;
    *weak_result = cookie_util::StripStatuses(cookie_list);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
  }
};

TEST_P(WebSocketStreamClientUseCookieTest, ClientUseCookie) {
  // For wss tests.
  url_request_context_host_.AddSSLSocketDataProvider(
      std::make_unique<SSLSocketDataProvider>(ASYNC, OK));

  CookieStore* store =
      url_request_context_host_.GetURLRequestContext()->cookie_store();

  const GURL url(GetParam().url);
  const GURL cookie_url(GetParam().cookie_url);
  const url::Origin origin =
      url::Origin::Create(GURL("http://www.example.com"));
  const GURL site_for_cookies("http://www.example.com/");
  const net::NetworkIsolationKey network_isolation_key(origin, origin);
  const std::string cookie_line(GetParam().cookie_line);
  const std::string cookie_header(AddCRLFIfNotEmpty(GetParam().cookie_header));

  bool is_called = false;
  bool set_cookie_result = false;
  base::WeakPtrFactory<bool> weak_is_called(&is_called);
  base::WeakPtrFactory<bool> weak_set_cookie_result(&set_cookie_result);

  base::RunLoop run_loop;
  auto cookie =
      CanonicalCookie::Create(cookie_url, cookie_line, base::Time::Now(),
                              base::nullopt /* server_time */);
  store->SetCanonicalCookieAsync(
      std::move(cookie), cookie_url.scheme(),
      net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SetCookieHelperFunction, run_loop.QuitClosure(),
                     weak_is_called.GetWeakPtr(),
                     weak_set_cookie_result.GetWeakPtr()));
  run_loop.Run();
  ASSERT_TRUE(is_called);
  ASSERT_TRUE(set_cookie_result);

  CreateAndConnect(url, origin, site_for_cookies, network_isolation_key,
                   cookie_header, WebSocketStandardResponse(""));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
}

TEST_P(WebSocketStreamServerSetCookieTest, ServerSetCookie) {
  // For wss tests.
  url_request_context_host_.AddSSLSocketDataProvider(
      std::make_unique<SSLSocketDataProvider>(ASYNC, OK));

  const GURL url(GetParam().url);
  const GURL cookie_url(GetParam().cookie_url);
  const url::Origin origin =
      url::Origin::Create(GURL("http://www.example.com"));
  const GURL site_for_cookies("http://www.example.com/");
  const net::NetworkIsolationKey network_isolation_key(origin, origin);
  const std::string cookie_line(GetParam().cookie_line);
  const std::string cookie_header(AddCRLFIfNotEmpty(GetParam().cookie_header));

  const std::string response = base::StringPrintf(
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "%s"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n",
      cookie_header.c_str());

  CookieStore* store =
      url_request_context_host_.GetURLRequestContext()->cookie_store();

  CreateAndConnect(url, origin, site_for_cookies, network_isolation_key, "",
                   response);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());

  bool is_called = false;
  CookieList get_cookie_list_result;
  base::WeakPtrFactory<bool> weak_is_called(&is_called);
  base::WeakPtrFactory<CookieList> weak_get_cookie_list_result(
      &get_cookie_list_result);
  base::RunLoop run_loop;
  store->GetCookieListWithOptionsAsync(
      cookie_url, net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&GetCookieListHelperFunction, run_loop.QuitClosure(),
                     weak_is_called.GetWeakPtr(),
                     weak_get_cookie_list_result.GetWeakPtr()));
  run_loop.Run();
  EXPECT_TRUE(is_called);
  EXPECT_THAT(get_cookie_list_result, MatchesCookieLine(cookie_line));
}

// Test parameters definitions follow...

const ClientUseCookieParameter kClientUseCookieParameters[] = {
    // Non-secure cookies for ws
    {"ws://www.example.com",
     "http://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "https://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "ws://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "wss://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    // Non-secure cookies for wss
    {"wss://www.example.com",
     "http://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "https://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "ws://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www.example.com",
     "test-cookie",
     "Cookie: test-cookie"},

    // Secure-cookies for ws
    {"ws://www.example.com",
     "https://www.example.com",
     "test-cookie; secure",
     kNoCookieHeader},

    {"ws://www.example.com",
     "wss://www.example.com",
     "test-cookie; secure",
     kNoCookieHeader},

    // Secure-cookies for wss
    {"wss://www.example.com",
     "https://www.example.com",
     "test-cookie; secure",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www.example.com",
     "test-cookie; secure",
     "Cookie: test-cookie"},

    // Non-secure cookies for ws (sharing domain)
    {"ws://www.example.com",
     "http://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "https://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "ws://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"ws://www.example.com",
     "wss://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    // Non-secure cookies for wss (sharing domain)
    {"wss://www.example.com",
     "http://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "https://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "ws://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www2.example.com",
     "test-cookie; Domain=example.com",
     "Cookie: test-cookie"},

    // Secure-cookies for ws (sharing domain)
    {"ws://www.example.com",
     "https://www2.example.com",
     "test-cookie; Domain=example.com; secure",
     kNoCookieHeader},

    {"ws://www.example.com",
     "wss://www2.example.com",
     "test-cookie; Domain=example.com; secure",
     kNoCookieHeader},

    // Secure-cookies for wss (sharing domain)
    {"wss://www.example.com",
     "https://www2.example.com",
     "test-cookie; Domain=example.com; secure",
     "Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www2.example.com",
     "test-cookie; Domain=example.com; secure",
     "Cookie: test-cookie"},

    // Non-matching cookies for ws
    {"ws://www.example.com",
     "http://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"ws://www.example.com",
     "https://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"ws://www.example.com",
     "ws://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"ws://www.example.com",
     "wss://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    // Non-matching cookies for wss
    {"wss://www.example.com",
     "http://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"wss://www.example.com",
     "https://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"wss://www.example.com",
     "ws://www2.example.com",
     "test-cookie",
     kNoCookieHeader},

    {"wss://www.example.com",
     "wss://www2.example.com",
     "test-cookie",
     kNoCookieHeader},
};

INSTANTIATE_TEST_SUITE_P(WebSocketStreamClientUseCookieTest,
                         WebSocketStreamClientUseCookieTest,
                         ValuesIn(kClientUseCookieParameters));

const ServerSetCookieParameter kServerSetCookieParameters[] = {
    // Cookies coming from ws
    {"ws://www.example.com",
     "http://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "https://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "ws://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "wss://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    // Cookies coming from wss
    {"wss://www.example.com",
     "http://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "https://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "ws://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie"},

    // cookies coming from ws (sharing domain)
    {"ws://www.example.com",
     "http://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"ws://www.example.com",
     "https://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"ws://www.example.com",
     "ws://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"ws://www.example.com",
     "wss://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    // cookies coming from wss (sharing domain)
    {"wss://www.example.com",
     "http://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"wss://www.example.com",
     "https://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"wss://www.example.com",
     "ws://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    {"wss://www.example.com",
     "wss://www2.example.com",
     "test-cookie",
     "Set-Cookie: test-cookie; Domain=example.com"},

    // Non-matching cookies coming from ws
    {"ws://www.example.com",
     "http://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "https://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "ws://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"ws://www.example.com",
     "wss://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    // Non-matching cookies coming from wss
    {"wss://www.example.com",
     "http://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "https://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "ws://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},

    {"wss://www.example.com",
     "wss://www2.example.com",
     "",
     "Set-Cookie: test-cookie"},
};

INSTANTIATE_TEST_SUITE_P(WebSocketStreamServerSetCookieTest,
                         WebSocketStreamServerSetCookieTest,
                         ValuesIn(kServerSetCookieParameters));

}  // namespace
}  // namespace net
