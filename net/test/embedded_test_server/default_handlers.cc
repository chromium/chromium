// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/default_handlers.h"

#include <ctime>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

namespace net::test_server {
namespace {

const char kDefaultRealm[] = "testrealm";
const char kDefaultPassword[] = "secret";
const char kEtag[] = "abc";
const char kLogoPath[] = "chrome/test/data/google/logo.gif";

// method: CONNECT
// Responses with a BAD_REQUEST to any CONNECT requests.
std::unique_ptr<HttpResponse> HandleDefaultConnect(const HttpRequest& request) {
  if (request.method != METHOD_CONNECT)
    return nullptr;

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(HTTP_BAD_REQUEST);
  http_response->set_content(
      "Your client has issued a malformed or illegal request.");
  http_response->set_content_type("text/html");
  return http_response;
}

// /cachetime
// Returns a cacheable response.
std::unique_ptr<HttpResponse> HandleCacheTime(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content("<!doctype html><title>Cache: max-age=60</title>");
  http_response->set_content_type("text/html");
  http_response->AddCustomHeader("Cache-Control", "max-age=60");
  return http_response;
}

// /echoheader?HEADERS | /echoheadercache?HEADERS
// Responds with the headers echoed in the message body.
// echoheader does not cache the results, while echoheadercache does.
std::unique_ptr<HttpResponse> HandleEchoHeader(const std::string& url,
                                               const std::string& cache_control,
                                               const HttpRequest& request) {
  if (!ShouldHandle(request, url))
    return nullptr;

  auto http_response = std::make_unique<BasicHttpResponse>();

  GURL request_url = request.GetURL();
  std::string vary;
  std::string content;
  RequestQuery headers = ParseQuery(request_url);
  for (const auto& header : headers) {
    std::string header_name = header.first;
    std::string header_value = "None";
    if (request.headers.find(header_name) != request.headers.end())
      header_value = request.headers.at(header_name);
    if (!vary.empty())
      vary += ",";
    vary += header_name;
    if (!content.empty())
      content += "\n";
    content += header_value;
  }

  http_response->AddCustomHeader("Vary", vary);
  http_response->set_content(content);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->AddCustomHeader("Cache-Control", cache_control);
  return http_response;
}

// /echo-cookie-with-status?status=###
// Responds with the given status code and echos the cookies sent in the request
std::unique_ptr<HttpResponse> HandleEchoCookieWithStatus(
    const std::string& url,
    const HttpRequest& request) {
  if (!ShouldHandle(request, url))
    return nullptr;

  auto http_response = std::make_unique<BasicHttpResponse>();

  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  int status_code = 400;
  const auto given_status = query.find("status");

  if (given_status != query.end() && !given_status->second.empty() &&
      !base::StringToInt(given_status->second.front(), &status_code)) {
    status_code = 400;
  }

  http_response->set_code(static_cast<HttpStatusCode>(status_code));

  const auto given_cookie = request.headers.find("Cookie");
  std::string content =
      (given_cookie == request.headers.end()) ? "None" : given_cookie->second;
  http_response->set_content(content);
  http_response->set_content_type("text/plain");
  return http_response;
}

// TODO(crbug.com/40153192): Remove when request handlers are
// implementable in Android's embedded test server implementation
std::unique_ptr<HttpResponse> HandleEchoCriticalHeader(
    const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();

  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");

  http_response->AddCustomHeader("Accept-CH", "Sec-CH-UA-Platform");
  http_response->AddCustomHeader("Critical-CH", "Sec-CH-UA-Platform");

  http_response->set_content(
      request.headers.find("Sec-CH-UA-Mobile")->second +
      request.headers.find("Sec-CH-UA-Platform")->second);

  return http_response;
}

// /echo?status=STATUS
// Responds with the request body as the response body and
// a status code of STATUS.
std::unique_ptr<HttpResponse> HandleEcho(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();

  GURL request_url = request.GetURL();
  if (request_url.has_query()) {
    RequestQuery query = ParseQuery(request_url);
    if (query.find("status") != query.end())
      http_response->set_code(static_cast<HttpStatusCode>(
          std::atoi(query["status"].front().c_str())));
  }

  http_response->set_content_type("text/html");
  if (request.method != METHOD_POST && request.method != METHOD_PUT)
    http_response->set_content("Echo");
  else
    http_response->set_content(request.content);
  return http_response;
}

// /echotitle
// Responds with the request body as the title.
std::unique_ptr<HttpResponse> HandleEchoTitle(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content("<!doctype html><title>" + request.content +
                             "</title>");
  return http_response;
}

// /echoall?QUERY
// Responds with the list of QUERY and the request headers.
//
// Alternative form:
// /echoall/nocache?QUERY prevents caching of the response.
std::unique_ptr<HttpResponse> HandleEchoAll(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();

  std::string body =
      "<!doctype html><title>EmbeddedTestServer - EchoAll</title><style>"
      "pre { border: 1px solid black; margin: 5px; padding: 5px }"
      "</style>"
      "<div style=\"float: right\">"
      "<a href=\"/echo\">back to referring page</a></div>"
      "<h1>Request Body:</h1><pre>";

  if (request.has_content) {
    std::vector<std::string> query_list = base::SplitString(
        request.content, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const auto& query : query_list)
      body += query + "\n";
  }

  body +=
      "</pre>"
      "<h1>Request Headers:</h1><pre id='request-headers'>" +
      request.all_headers + "</pre>" +
      "<h1>Response nonce:</h1><pre id='response-nonce'>" +
      base::UnguessableToken::Create().ToString() + "</pre>";

  http_response->set_content_type("text/html");
  http_response->set_content(body);

  if (request.GetURL().path_piece().ends_with("/nocache")) {
    http_response->AddCustomHeader("Cache-Control",
                                   "no-cache, no-store, must-revalidate");
  }

  return http_response;
}

// /echo-raw
// Returns the query string as the raw response (no HTTP headers).
std::unique_ptr<HttpResponse> HandleEchoRaw(const HttpRequest& request) {
  return std::make_unique<RawHttpResponse>("", request.GetURL().query());
}

// /set-cookie?COOKIES
// Sets response cookies to be COOKIES.
std::unique_ptr<HttpResponse> HandleSetCookie(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  std::string content;
  GURL request_url = request.GetURL();
  if (request_url.has_query()) {
    std::vector<std::string> cookies = base::SplitString(
        request_url.query(), "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const auto& cookie : cookies) {
      http_response->AddCustomHeader("Set-Cookie", cookie);
      content += cookie;
    }
  }

  http_response->set_content(content);
  return http_response;
}

// /set-invalid-cookie
// Sets invalid response cookies "\x01" (chosen via fuzzer to not be a parsable
// cookie).
std::unique_ptr<HttpResponse> HandleSetInvalidCookie(
    const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  std::string content;
  GURL request_url = request.GetURL();

  http_response->AddCustomHeader("Set-Cookie", "\x01");

  http_response->set_content("TEST");
  return http_response;
}

// /expect-and-set-cookie?expect=EXPECTED&set=SET&data=DATA
// Verifies that the request cookies match EXPECTED and then returns cookies
// that match SET and a content that matches DATA.
std::unique_ptr<HttpResponse> HandleExpectAndSetCookie(
    const HttpRequest& request) {
  std::vector<std::string> received_cookies;
  if (request.headers.find("Cookie") != request.headers.end()) {
    received_cookies =
        base::SplitString(request.headers.at("Cookie"), ";",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }

  bool got_all_expected = true;
  GURL request_url = request.GetURL();
  RequestQuery query_list = ParseQuery(request_url);
  if (query_list.find("expect") != query_list.end()) {
    for (const auto& expected_cookie : query_list.at("expect")) {
      bool found = false;
      for (const auto& received_cookie : received_cookies) {
        if (expected_cookie == received_cookie)
          found = true;
      }
      got_all_expected &= found;
    }
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  if (got_all_expected) {
    for (const auto& cookie : query_list.at("set")) {
      http_response->AddCustomHeader(
          "Set-Cookie",
          base::UnescapeBinaryURLComponent(
              cookie, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE));
    }
  }

  std::string content;
  if (query_list.find("data") != query_list.end()) {
    for (const auto& item : query_list.at("data"))
      content += item;
  }

  http_response->set_content(content);
  return http_response;
}

// An internal utility to extract HTTP Headers from a URL in the format of
// "/url&KEY1: VALUE&KEY2: VALUE2". Returns a header key to header value map.
std::multimap<std::string, std::string> ExtractHeadersFromQuery(
    const GURL& url) {
  std::multimap<std::string, std::string> key_to_value;
  if (url.has_query()) {
    RequestQuery headers = ParseQuery(url);
    for (const auto& header : headers) {
      size_t delimiter = header.first.find(": ");
      if (delimiter == std::string::npos) {
        continue;
      }
      std::string key = header.first.substr(0, delimiter);
      std::string value = header.first.substr(delimiter + 2);
      key_to_value.emplace(key, value);
    }
  }
  return key_to_value;
}

// /set-header?HEADERS
// Returns a response with HEADERS set as the response headers, and also set as
// the response content.
//
// Example:
//    /set-header?Content-Security-Policy: sandbox&Referer-Policy: origin
std::unique_ptr<HttpResponse> HandleSetHeader(const HttpRequest& request) {
  std::string content;

  GURL request_url = request.GetURL();

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  auto headers = ExtractHeadersFromQuery(request_url);
  for (const auto& [key, value] : headers) {
    http_response->AddCustomHeader(key, value);
    content += key + ": " + value;
  }

  http_response->set_content(content);
  return http_response;
}

// /set-header-with-file/FILE_PATH?HEADERS
// Returns a response with context read from FILE_PATH as the response content,
// and HEADERS as the response header. Unlike /set-header?HEADERS, which only
// serves a response with HEADERS as response header and also HEADERS as its
// content.
//
// FILE_PATH points to the static test file. For example, a query like
// /set-header-with-file/content/test/data/title1.html will returns the content
// of the file at content/test/data/title1.html.
// HEADERS is composed of a list of "key: value" pairs. Note that unlike how a
// file is normally served by `HandleFileRequest()`, its static mock headers
// from the other file FILE_PATH.mock-http-headers will NOT be used here.
//
// Example:
//    /set-header-with-file/content/test/data/title1.html?Referer-Policy: origin
std::unique_ptr<HttpResponse> HandleSetHeaderWithFile(
    const std::string& prefix,
    const HttpRequest& request) {
  if (!ShouldHandle(request, prefix)) {
    return nullptr;
  }

  GURL request_url = request.GetURL();
  auto http_response = std::make_unique<BasicHttpResponse>();

  base::FilePath server_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &server_root);
  base::FilePath file_path =
      server_root.AppendASCII(request_url.path().substr(prefix.size() + 1));
  std::string file_content;
  CHECK(base::ReadFileToString(file_path, &file_content));
  http_response->set_content(file_content);
  http_response->set_content_type(GetContentType(file_path));

  auto headers = ExtractHeadersFromQuery(request_url);
  for (const auto& [key, value] : headers) {
    http_response->AddCustomHeader(key, value);
  }

  http_response->set_code(HTTP_OK);
  return http_response;
}

// /iframe?URL
// Returns a page that iframes the specified URL.
std::unique_ptr<HttpResponse> HandleIframe(const HttpRequest& request) {
  GURL request_url = request.GetURL();

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");

  GURL iframe_url("about:blank");
  if (request_url.has_query()) {
    iframe_url = GURL(base::UnescapeBinaryURLComponent(request_url.query()));
  }

  http_response->set_content(base::StringPrintf(
      "<!doctype html><iframe src=\"%s\">", iframe_url.spec().c_str()));
  return http_response;
}

// /nocontent
// Returns a NO_CONTENT response.
std::unique_ptr<HttpResponse> HandleNoContent(const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(HTTP_NO_CONTENT);
  return http_response;
}

// /close-socket
// Immediately closes the connection.
std::unique_ptr<HttpResponse> HandleCloseSocket(const HttpRequest& request) {
  return std::make_unique<RawHttpResponse>("", "");
}

// /auth-basic?password=PASS&realm=REALM
// Performs "Basic" HTTP authentication using expected password PASS and
// realm REALM.
std::unique_ptr<HttpResponse> HandleAuthBasic(const HttpRequest& request) {
  GURL request_url = request.GetURL();
  RequestQuery query = ParseQuery(request_url);

  std::string expected_password = kDefaultPassword;
  if (query.find("password") != query.end())
    expected_password = query.at("password").front();
  std::string realm = kDefaultRealm;
  if (query.find("realm") != query.end())
    realm = query.at("realm").front();

  bool authed = false;
  std::string error;
  std::string auth;
  std::string username;
  std::string userpass;
  std::string password;
  std::string b64str;
  if (request.headers.find("Authorization") == request.headers.end()) {
    error = "Missing Authorization Header";
  } else {
    auth = request.headers.at("Authorization");
    if (auth.find("Basic ") == std::string::npos) {
      error = "Invalid Authorization Header";
    } else {
      b64str = auth.substr(std::string("Basic ").size());
      base::Base64Decode(b64str, &userpass);
      size_t delimiter = userpass.find(":");
      if (delimiter != std::string::npos) {
        username = userpass.substr(0, delimiter);
        password = userpass.substr(delimiter + 1);
        if (password == expected_password)
          authed = true;
        else
          error = "Invalid Credentials";
      } else {
        error = "Invalid Credentials";
      }
    }
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  if (!authed) {
    http_response->set_code(HTTP_UNAUTHORIZED);
    http_response->set_content_type("text/html");
    http_response->AddCustomHeader("WWW-Authenticate",
                                   "Basic realm=\"" + realm + "\"");
    if (query.find("set-cookie-if-challenged") != query.end())
      http_response->AddCustomHeader("Set-Cookie", "got_challenged=true");
    if (query.find("set-secure-cookie-if-challenged") != query.end())
      http_response->AddCustomHeader("Set-Cookie",
                                     "got_challenged=true;Secure");
    http_response->set_content(base::StringPrintf(
        "<!doctype html><title>Denied: %s</title>"
        "<p>auth=%s<p>b64str=%s<p>username: %s<p>userpass: %s"
        "<p>password: %s<p>You sent:<br>%s",
        error.c_str(), auth.c_str(), b64str.c_str(), username.c_str(),
        userpass.c_str(), password.c_str(), request.all_headers.c_str()));
    return http_response;
  }

  if (query.find("set-cookie-if-not-challenged") != query.end())
    http_response->AddCustomHeader("Set-Cookie", "got_challenged=true");

  if (request.headers.find("If-None-Match") != request.headers.end() &&
      request.headers.at("If-None-Match") == kEtag) {
    http_response->set_code(HTTP_NOT_MODIFIED);
    return http_response;
  }

  base::FilePath file_path =
      base::FilePath().AppendASCII(request.relative_url.substr(1));
  if (file_path.FinalExtension() == FILE_PATH_LITERAL("gif")) {
    base::FilePath server_root;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &server_root);
    base::FilePath gif_path = server_root.AppendASCII(kLogoPath);
    std::string gif_data;
    base::ReadFileToString(gif_path, &gif_data);
    http_response->set_content_type("image/gif");
    http_response->set_content(gif_data);
  } else {
    http_response->set_content_type("text/html");
    http_response->set_content(
        base::StringPrintf("<!doctype html><title>%s/%s</title>"
                           "<p>auth=%s<p>You sent:<br>%s",
                           username.c_str(), password.c_str(), auth.c_str(),
                           request.all_headers.c_str()));
  }

  http_response->AddCustomHeader("Cache-Control", "max-age=60000");
  http_response->AddCustomHeader("Etag", kEtag);
  return http_response;
}

// /auth-digest
// Performs "Digest" HTTP authentication.
std::unique_ptr<HttpResponse> HandleAuthDigest(const HttpRequest& request) {
  std::string nonce = base::MD5String(
      base::StringPrintf("privatekey%s", request.relative_url.c_str()));
  std::string opaque = base::MD5String("opaque");
  std::string password = kDefaultPassword;
  std::string realm = kDefaultRealm;

  bool authed = false;
  std::string error;
  std::string auth;
  std::string digest_str = "Digest";
  std::string username;
  if (request.headers.find("Authorization") == request.headers.end()) {
    error = "no auth";
  } else if (request.headers.at("Authorization").substr(0, digest_str.size()) !=
             digest_str) {
    error = "not digest";
  } else {
    auth = request.headers.at("Authorization").substr(digest_str.size() + 1);

    std::map<std::string, std::string> auth_pairs;
    base::StringPairs auth_vector;
    base::SplitStringIntoKeyValuePairs(auth, '=', ',', &auth_vector);
    for (const auto& auth_pair : auth_vector) {
      std::string key;
      std::string value;
      base::TrimWhitespaceASCII(auth_pair.first, base::TRIM_ALL, &key);
      base::TrimWhitespaceASCII(auth_pair.second, base::TRIM_ALL, &value);
      if (value.size() > 2 && value.at(0) == '"' &&
          value.at(value.size() - 1) == '"') {
        value = value.substr(1, value.size() - 2);
      }
      auth_pairs[key] = value;
    }

    if (auth_pairs["nonce"] != nonce) {
      error = "wrong nonce";
    } else if (auth_pairs["opaque"] != opaque) {
      error = "wrong opaque";
    } else {
      username = auth_pairs["username"];

      std::string hash1 = base::MD5String(
          base::StringPrintf("%s:%s:%s", auth_pairs["username"].c_str(),
                             realm.c_str(), password.c_str()));
      std::string hash2 = base::MD5String(base::StringPrintf(
          "%s:%s", request.method_string.c_str(), auth_pairs["uri"].c_str()));

      std::string response;
      if (auth_pairs.find("qop") != auth_pairs.end() &&
          auth_pairs.find("nc") != auth_pairs.end() &&
          auth_pairs.find("cnonce") != auth_pairs.end()) {
        response = base::MD5String(base::StringPrintf(
            "%s:%s:%s:%s:%s:%s", hash1.c_str(), nonce.c_str(),
            auth_pairs["nc"].c_str(), auth_pairs["cnonce"].c_str(),
            auth_pairs["qop"].c_str(), hash2.c_str()));
      } else {
        response = base::MD5String(base::StringPrintf(
            "%s:%s:%s", hash1.c_str(), nonce.c_str(), hash2.c_str()));
      }

      if (auth_pairs["response"] == response)
        authed = true;
      else
        error = "wrong password";
    }
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  if (!authed) {
    http_response->set_code(HTTP_UNAUTHORIZED);
    http_response->set_content_type("text/html");
    std::string auth_header = base::StringPrintf(
        "Digest realm=\"%s\", "
        "domain=\"/\", qop=\"auth\", algorithm=MD5, nonce=\"%s\", "
        "opaque=\"%s\"",
        realm.c_str(), nonce.c_str(), opaque.c_str());
    http_response->AddCustomHeader("WWW-Authenticate", auth_header);
    http_response->set_content(
        base::StringPrintf("<!doctype html><title>Denied: %s</title>"
                           "<p>auth=%s"
                           "You sent:<br>%s<p>We are replying:<br>%s",
                           error.c_str(), auth.c_str(),
                           request.all_headers.c_str(), auth_header.c_str()));
    return http_response;
  }

  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<!doctype html><title>%s/%s</title>"
                         "<p>auth=%s",
                         username.c_str(), password.c_str(), auth.c_str()));

  return http_response;
}

// 1. /server-redirect?URL or /server-redirect-xxx?URL
//    Returns a server redirect to URL.
// 2. /no-cors-server-redirect?URL or /no-cors-server-redirect-xxx?URL
//    Returns a server redirect to URL which does not allow CORS.
std::unique_ptr<HttpResponse> HandleServerRedirect(HttpStatusCode redirect_code,
                                                   bool allow_cors,
                                                   const HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  RequestQuery query = ParseQuery(request_url);

  if (request.method == METHOD_OPTIONS) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(HTTP_OK);
    if (allow_cors) {
      http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
      http_response->AddCustomHeader("Access-Control-Allow-Methods", "*");
      http_response->AddCustomHeader("Access-Control-Allow-Headers", "*");
    }
    return http_response;
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(redirect_code);
  http_response->AddCustomHeader("Location", dest);
  if (allow_cors) {
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  }
  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<!doctype html><p>Redirecting to %s", dest.c_str()));
  return http_response;
}
// /server-redirect-with-cookie?URL
// Returns a server redirect to URL, and sets the cookie server-redirect=true.
std::unique_ptr<HttpResponse> HandleServerRedirectWithCookie(
    HttpStatusCode redirect_code,
    const HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  RequestQuery query = ParseQuery(request_url);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(redirect_code);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie", "server-redirect=true");
  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<!doctype html><p>Redirecting to %s", dest.c_str()));
  return http_response;
}

// /server-redirect-with-secure-cookie?URL
// Returns a server redirect to URL, and sets the cookie
// server-redirect=true;Secure.
std::unique_ptr<HttpResponse> HandleServerRedirectWithSecureCookie(
    HttpStatusCode redirect_code,
    const HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  RequestQuery query = ParseQuery(request_url);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(redirect_code);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie", "server-redirect=true;Secure");
  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<!doctype html><p>Redirecting to %s", dest.c_str()));
  return http_response;
}

// /cross-site?URL (also /cross-site-with-cookie?URL)
// Returns a cross-site redirect to URL.
std::unique_ptr<HttpResponse> HandleCrossSiteRedirect(
    EmbeddedTestServer* server,
    const std::string& prefix,
    bool set_cookie,
    const HttpRequest& request) {
  if (!ShouldHandle(request, prefix))
    return nullptr;

  std::string dest_all = base::UnescapeBinaryURLComponent(
      request.relative_url.substr(prefix.size() + 1));

  std::string dest;
  size_t delimiter = dest_all.find("/");
  if (delimiter != std::string::npos) {
    dest = base::StringPrintf(
        "//%s:%hu/%s", dest_all.substr(0, delimiter).c_str(), server->port(),
        dest_all.substr(delimiter + 1).c_str());
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", dest);
  if (set_cookie) {
    http_response->AddCustomHeader("Set-Cookie", "server-redirect=true");
  }
  http_response->set_content_type("text/html");
  http_response->set_content(
      base::StringPrintf("<!doctype html><p>Redirecting to %s", dest.c_str()));
  return http_response;
}

// /client-redirect?URL
// Returns a meta redirect to URL.
std::unique_ptr<HttpResponse> HandleClientRedirect(const HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string dest =
      base::UnescapeBinaryURLComponent(request_url.query_piece());

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<!doctype html><meta http-equiv=\"refresh\" content=\"0;url=%s\">"
      "<p>Redirecting to %s",
      dest.c_str(), dest.c_str()));
  return http_response;
}

// /defaultresponse
// Returns a valid 200 response.
std::unique_ptr<HttpResponse> HandleDefaultResponse(
    const HttpRequest& request) {
  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content("Default response given for path: " +
                             request.relative_url);
  return http_response;
}

// /slow?N
// Returns a response to the server delayed by N seconds.
std::unique_ptr<HttpResponse> HandleSlowServer(const HttpRequest& request) {
  double delay = 1.0f;

  GURL request_url = request.GetURL();
  if (request_url.has_query())
    delay = std::atof(request_url.query().c_str());

  auto http_response =
      std::make_unique<DelayedHttpResponse>(base::Seconds(delay));
  http_response->set_content_type("text/plain");
  http_response->set_content(base::StringPrintf("waited %.1f seconds", delay));
  return http_response;
}

// /hung
// Never returns a response.
std::unique_ptr<HttpResponse> HandleHungResponse(const HttpRequest& request) {
  return std::make_unique<HungResponse>();
}

// /hung-after-headers
// Never returns a response.
std::unique_ptr<HttpResponse> HandleHungAfterHeadersResponse(
    const HttpRequest& request) {
  return std::make_unique<HungAfterHeadersHttpResponse>();
}

// /exabyte_response
// A HttpResponse that is almost never ending (with an Exabyte content-length).
class ExabyteResponse : public BasicHttpResponse {
 public:
  ExabyteResponse() = default;

  ExabyteResponse(const ExabyteResponse&) = delete;
  ExabyteResponse& operator=(const ExabyteResponse&) = delete;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override {
    // Use 10^18 bytes (exabyte) as the content length so that the client will
    // be expecting data.
    delegate->SendResponseHeaders(HTTP_OK, "OK",
                                  {{"Content-Length", "1000000000000000000"}});
    SendExabyte(delegate);
  }

 private:
  // Keeps sending the word "echo" over and over again. It can go further to
  // limit the response to exactly an exabyte, but it shouldn't be necessary
  // for the purpose of testing.
  void SendExabyte(base::WeakPtr<HttpResponseDelegate> delegate) {
    delegate->SendContents(
        "echo", base::BindOnce(&ExabyteResponse::PostSendExabyteTask,
                               weak_factory_.GetWeakPtr(), delegate));
  }

  void PostSendExabyteTask(base::WeakPtr<HttpResponseDelegate> delegate) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExabyteResponse::SendExabyte,
                                  weak_factory_.GetWeakPtr(), delegate));
  }

  base::WeakPtrFactory<ExabyteResponse> weak_factory_{this};
};

// /exabyte_response
// Almost never ending response.
std::unique_ptr<HttpResponse> HandleExabyteResponse(
    const HttpRequest& request) {
  return std::make_unique<ExabyteResponse>();
}

// /gzip-body?<body>
// Returns a response with a gzipped body of "<body>". Attempts to allocate
// enough memory to contain the body, but DCHECKs if that fails.
std::unique_ptr<HttpResponse> HandleGzipBody(const HttpRequest& request) {
  std::string uncompressed_body = request.GetURL().query();
  // Attempt to pick size that's large enough even in the worst case (deflate
  // block headers should be shorter than 512 bytes, and deflating should never
  // double size of data, modulo headers).
  // TODO(mmenke): This is rather awkward. Worth improving CompressGzip?
  std::vector<char> compressed_body(uncompressed_body.size() * 2 + 512);
  size_t compressed_size = compressed_body.size();
  CompressGzip(uncompressed_body.c_str(), uncompressed_body.size(),
               compressed_body.data(), &compressed_size,
               true /* gzip_framing */);
  // CompressGzip should DCHECK itself if this fails, anyways.
  DCHECK_GE(compressed_body.size(), compressed_size);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_content(
      std::string(compressed_body.data(), compressed_size));
  http_response->AddCustomHeader("Content-Encoding", "gzip");
  http_response->AddCustomHeader("Cache-Control", "max-age=60");
  return http_response;
}

// /self.pac
// Returns a response that is a PAC script making requests use the
// EmbeddedTestServer itself as a proxy.
std::unique_ptr<HttpResponse> HandleSelfPac(const HttpRequest& request) {
  std::unique_ptr<BasicHttpResponse> http_response =
      std::make_unique<BasicHttpResponse>();
  http_response->set_content(base::StringPrintf(
      "function FindProxyForURL(url, host) {\n"
      "return 'PROXY %s';\n"
      "}",
      net::HostPortPair::FromURL(request.base_url).ToString().c_str()));
  return http_response;
}

// A chunked HTTP response, with optional delays between chunks. See
// HandleChunks() for argument details.
class DelayedChunkedHttpResponse : public HttpResponse {
 public:
  DelayedChunkedHttpResponse(base::TimeDelta delay_before_headers,
                             base::TimeDelta delay_between_chunks,
                             int chunk_size,
                             int num_chunks)
      : delay_before_headers_(delay_before_headers),
        delay_between_chunks_(delay_between_chunks),
        chunk_size_(chunk_size),
        remaining_chunks_(num_chunks) {}

  ~DelayedChunkedHttpResponse() override = default;

  DelayedChunkedHttpResponse(const DelayedChunkedHttpResponse&) = delete;
  DelayedChunkedHttpResponse& operator=(const DelayedChunkedHttpResponse&) =
      delete;

  void SendResponse(base::WeakPtr<HttpResponseDelegate> delegate) override {
    delegate_ = delegate;

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DelayedChunkedHttpResponse::SendHeaders,
                       weak_ptr_factory_.GetWeakPtr()),
        delay_before_headers_);
  }

 private:
  void SendHeaders() {
    base::StringPairs headers = {{"Content-Type", "text/plain"},
                                 {"Connection", "close"},
                                 {"Transfer-Encoding", "chunked"}};
    delegate_->SendResponseHeaders(HTTP_OK, "OK", headers);
    PrepareToSendNextChunk();
  }

  void PrepareToSendNextChunk() {
    if (remaining_chunks_ == 0) {
      delegate_->SendContentsAndFinish(CreateChunk(0 /* chunk_size */));
      return;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DelayedChunkedHttpResponse::SendNextChunk,
                       weak_ptr_factory_.GetWeakPtr()),
        delay_between_chunks_);
  }

  void SendNextChunk() {
    DCHECK_GT(remaining_chunks_, 0);
    remaining_chunks_--;

    delegate_->SendContents(
        CreateChunk(chunk_size_),
        base::BindOnce(&DelayedChunkedHttpResponse::PrepareToSendNextChunk,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  static std::string CreateChunk(int chunk_size) {
    return base::StringPrintf(
        "%x\r\n"
        "%s"
        "\r\n",
        chunk_size, std::string(chunk_size, '*').c_str());
  }

  base::TimeDelta delay_before_headers_;
  base::TimeDelta delay_between_chunks_;
  int chunk_size_;
  int remaining_chunks_;

  base::WeakPtr<HttpResponseDelegate> delegate_ = nullptr;

  base::WeakPtrFactory<DelayedChunkedHttpResponse> weak_ptr_factory_{this};
};

// /chunked
// Returns a chunked response.
//
// Optional query parameters:
// * waitBeforeHeaders: Delays the specified number milliseconds before sending
// a response header. Defaults to 0.
// * waitBetweenChunks: Delays the specified number milliseconds before sending
// each chunk, except the last. Defaults to 0.
// * chunkSize: Size of each chunk, in bytes. Defaults to 5.
// * chunksNumber: Number of non-empty chunks. Defaults to 5.
std::unique_ptr<HttpResponse> HandleChunked(const HttpRequest& request) {
  GURL request_url = request.GetURL();

  base::TimeDelta delay_before_headers;
  base::TimeDelta delay_between_chunks;
  int chunk_size = 5;
  int num_chunks = 5;

  for (QueryIterator query(request_url); !query.IsAtEnd(); query.Advance()) {
    int value;
    CHECK(base::StringToInt(query.GetValue(), &value));
    CHECK_GE(value, 0);
    if (query.GetKey() == "waitBeforeHeaders") {
      delay_before_headers = base::Milliseconds(value);
    } else if (query.GetKey() == "waitBetweenChunks") {
      delay_between_chunks = base::Milliseconds(value);
    } else if (query.GetKey() == "chunkSize") {
      // A 0-size chunk indicates completion.
      CHECK_LT(0, value);
      chunk_size = value;
    } else if (query.GetKey() == "chunksNumber") {
      num_chunks = value;
    } else {
      NOTREACHED_IN_MIGRATION()
          << query.GetKey() << "Is not a valid argument of /chunked";
    }
  }

  return std::make_unique<DelayedChunkedHttpResponse>(
      delay_before_headers, delay_between_chunks, chunk_size, num_chunks);
}

EmbeddedTestServer::HandleRequestCallback PrefixHandler(
    const std::string& prefix,
    std::unique_ptr<HttpResponse> (*handler)(const HttpRequest& request)) {
  return base::BindRepeating(&HandlePrefixedRequest, prefix,
                             base::BindRepeating(handler));
}

EmbeddedTestServer::HandleRequestCallback ServerRedirectHandler(
    const std::string& prefix,
    std::unique_ptr<HttpResponse> (*handler)(HttpStatusCode redirect_code,
                                             bool allow_cors,
                                             const HttpRequest& request),
    HttpStatusCode redirect_code) {
  return base::BindRepeating(
      &HandlePrefixedRequest, prefix,
      base::BindRepeating(handler, redirect_code, /*allow_cors=*/true));
}

EmbeddedTestServer::HandleRequestCallback NoCorsServerRedirectHandler(
    const std::string& prefix,
    std::unique_ptr<HttpResponse> (*handler)(HttpStatusCode redirect_code,
                                             bool allow_cors,
                                             const HttpRequest& request),
    HttpStatusCode redirect_code) {
  return base::BindRepeating(
      &HandlePrefixedRequest, prefix,
      base::BindRepeating(handler, redirect_code, /*allow_cors=*/false));
}

EmbeddedTestServer::HandleRequestCallback ServerRedirectWithCookieHandler(
    const std::string& prefix,
    std::unique_ptr<HttpResponse> (*handler)(HttpStatusCode redirect_code,
                                             const HttpRequest& request),
    HttpStatusCode redirect_code) {
  return base::BindRepeating(&HandlePrefixedRequest, prefix,
                             base::BindRepeating(handler, redirect_code));
}

}  // anonymous namespace

void RegisterDefaultHandlers(EmbeddedTestServer* server) {
  server->RegisterDefaultHandler(base::BindRepeating(&HandleDefaultConnect));

  server->RegisterDefaultHandler(PrefixHandler("/cachetime", &HandleCacheTime));
  server->RegisterDefaultHandler(
      base::BindRepeating(&HandleEchoHeader, "/echoheader", "no-cache"));
  server->RegisterDefaultHandler(base::BindRepeating(
      &HandleEchoCookieWithStatus, "/echo-cookie-with-status"));
  server->RegisterDefaultHandler(base::BindRepeating(
      &HandleEchoHeader, "/echoheadercache", "max-age=60000"));
  server->RegisterDefaultHandler(PrefixHandler("/echo", &HandleEcho));
  server->RegisterDefaultHandler(PrefixHandler("/echotitle", &HandleEchoTitle));
  server->RegisterDefaultHandler(PrefixHandler("/echoall", &HandleEchoAll));
  server->RegisterDefaultHandler(PrefixHandler("/echo-raw", &HandleEchoRaw));
  server->RegisterDefaultHandler(
      PrefixHandler("/echocriticalheader", &HandleEchoCriticalHeader));
  server->RegisterDefaultHandler(
      PrefixHandler("/set-cookie", &HandleSetCookie));
  server->RegisterDefaultHandler(
      PrefixHandler("/set-invalid-cookie", &HandleSetInvalidCookie));
  server->RegisterDefaultHandler(
      PrefixHandler("/expect-and-set-cookie", &HandleExpectAndSetCookie));
  server->RegisterDefaultHandler(
      PrefixHandler("/set-header", &HandleSetHeader));
  server->RegisterDefaultHandler(
      base::BindRepeating(&HandleSetHeaderWithFile, "/set-header-with-file"));
  server->RegisterDefaultHandler(PrefixHandler("/iframe", &HandleIframe));
  server->RegisterDefaultHandler(PrefixHandler("/nocontent", &HandleNoContent));
  server->RegisterDefaultHandler(
      PrefixHandler("/close-socket", &HandleCloseSocket));
  server->RegisterDefaultHandler(
      PrefixHandler("/auth-basic", &HandleAuthBasic));
  server->RegisterDefaultHandler(
      PrefixHandler("/auth-digest", &HandleAuthDigest));

  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect", &HandleServerRedirect, HTTP_MOVED_PERMANENTLY));
  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect-301", &HandleServerRedirect, HTTP_MOVED_PERMANENTLY));
  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect-302", &HandleServerRedirect, HTTP_FOUND));
  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect-303", &HandleServerRedirect, HTTP_SEE_OTHER));
  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect-307", &HandleServerRedirect, HTTP_TEMPORARY_REDIRECT));
  server->RegisterDefaultHandler(ServerRedirectHandler(
      "/server-redirect-308", &HandleServerRedirect, HTTP_PERMANENT_REDIRECT));

  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect", &HandleServerRedirect,
      HTTP_MOVED_PERMANENTLY));
  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect-301", &HandleServerRedirect,
      HTTP_MOVED_PERMANENTLY));
  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect-302", &HandleServerRedirect, HTTP_FOUND));
  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect-303", &HandleServerRedirect, HTTP_SEE_OTHER));
  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect-307", &HandleServerRedirect,
      HTTP_TEMPORARY_REDIRECT));
  server->RegisterDefaultHandler(NoCorsServerRedirectHandler(
      "/no-cors-server-redirect-308", &HandleServerRedirect,
      HTTP_PERMANENT_REDIRECT));

  server->RegisterDefaultHandler(ServerRedirectWithCookieHandler(
      "/server-redirect-with-cookie", &HandleServerRedirectWithCookie,
      HTTP_MOVED_PERMANENTLY));
  server->RegisterDefaultHandler(ServerRedirectWithCookieHandler(
      "/server-redirect-with-secure-cookie",
      &HandleServerRedirectWithSecureCookie, HTTP_MOVED_PERMANENTLY));

  server->RegisterDefaultHandler(base::BindRepeating(&HandleCrossSiteRedirect,
                                                     server, "/cross-site",
                                                     /*set_cookie=*/false));
  server->RegisterDefaultHandler(
      base::BindRepeating(&HandleCrossSiteRedirect, server,
                          "/cross-site-with-cookie", /*set_cookie=*/true));
  server->RegisterDefaultHandler(
      PrefixHandler("/client-redirect", &HandleClientRedirect));
  server->RegisterDefaultHandler(
      PrefixHandler("/defaultresponse", &HandleDefaultResponse));
  server->RegisterDefaultHandler(PrefixHandler("/slow", &HandleSlowServer));
  server->RegisterDefaultHandler(PrefixHandler("/hung", &HandleHungResponse));
  server->RegisterDefaultHandler(
      PrefixHandler("/hung-after-headers", &HandleHungAfterHeadersResponse));
  server->RegisterDefaultHandler(
      PrefixHandler("/exabyte_response", &HandleExabyteResponse));
  server->RegisterDefaultHandler(PrefixHandler("/gzip-body", &HandleGzipBody));
  server->RegisterDefaultHandler(PrefixHandler("/self.pac", &HandleSelfPac));
  server->RegisterDefaultHandler(PrefixHandler("/chunked", &HandleChunked));

  // TODO(svaldez): HandleDownload
  // TODO(svaldez): HandleDownloadFinish
  // TODO(svaldez): HandleZipFile
  // TODO(svaldez): HandleSSLManySmallRecords
  // TODO(svaldez): HandleGetSSLSessionCache
  // TODO(svaldez): HandleGetChannelID
  // TODO(svaldez): HandleGetClientCert
  // TODO(svaldez): HandleClientCipherList
  // TODO(svaldez): HandleEchoMultipartPost
}

}  // namespace net::test_server
