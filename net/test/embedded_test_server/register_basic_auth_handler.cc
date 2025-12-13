// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/register_basic_auth_handler.h"

#include <ios>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net::test_server {

namespace {

// Constructs an expected authorization header value (e.g., "Basic
// dXNlcm5hbWU6cGFzc3dvcmQ="). Works with both "WWW-Authenticate" and
// "Proxy-Authenticate" request lines.
std::string CreateExpectedBasicAuthHeader(std::string_view username,
                                          std::string_view password) {
  const std::string credentials = base::StrCat({username, ":", password});
  const std::string encoded_credentials = base::Base64Encode(credentials);
  return base::StrCat({"Basic ", encoded_credentials});
}

// Creates a 401 Unauthorized error response with the required WWW-Authenticate
// header.
std::unique_ptr<HttpResponse> CreateUnauthorizedResponse(bool is_proxy_auth) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(is_proxy_auth
                         ? HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED
                         : HttpStatusCode::HTTP_UNAUTHORIZED);
  response->AddCustomHeader(
      is_proxy_auth ? "Proxy-Authenticate" : "WWW-Authenticate",
      "Basic realm=\"TestServer\"");
  response->set_content("Unauthorized");
  response->set_content_type("text/plain");
  return response;
}

// Callback to handle BasicAuth validation.
std::unique_ptr<HttpResponse> HandleBasicAuth(
    const std::string& expected_auth_header,
    bool is_proxy_auth,
    const HttpRequest& request) {
  auto auth_header = request.headers.find(is_proxy_auth ? "Proxy-Authorization"
                                                        : "Authorization");

  if (auth_header == request.headers.end() ||
      auth_header->second != expected_auth_header) {
    VLOG(1) << "Authorization failed or header missing. For Proxy: "
            << std::boolalpha << is_proxy_auth;
    return CreateUnauthorizedResponse(is_proxy_auth);
  }

  VLOG(3) << "Authorization successful. For Proxy: " << is_proxy_auth;
  return nullptr;
}

}  // namespace

void RegisterBasicAuthHandler(EmbeddedTestServer& server,
                              std::string_view username,
                              std::string_view password) {
  // Register the BasicAuth handler with the server.
  server.RegisterAuthHandler(base::BindRepeating(
      &HandleBasicAuth, CreateExpectedBasicAuthHeader(username, password),
      /*is_proxy_auth=*/false));
}

void RegisterProxyBasicAuthHandler(EmbeddedTestServer& server,
                                   std::string_view username,
                                   std::string_view password) {
  // Register the BasicAuth handler with the server.
  server.RegisterAuthHandler(base::BindRepeating(
      &HandleBasicAuth, CreateExpectedBasicAuthHeader(username, password),
      /*is_proxy_auth=*/true));
}

GURL GetURLWithUser(const EmbeddedTestServer& server,
                    std::string_view path,
                    std::string_view user) {
  GURL url = server.GetURL(path);
  GURL::Replacements replacements;
  replacements.SetUsernameStr(user);
  return url.ReplaceComponents(replacements);
}

GURL GetURLWithUserAndPassword(const EmbeddedTestServer& server,
                               std::string_view path,
                               std::string_view user,
                               std::string_view password) {
  GURL url = server.GetURL(path);
  GURL::Replacements replacements;
  replacements.SetUsernameStr(user);
  replacements.SetPasswordStr(password);
  return url.ReplaceComponents(replacements);
}

}  // namespace net::test_server
