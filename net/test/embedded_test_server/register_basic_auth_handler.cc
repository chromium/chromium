// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/register_basic_auth_handler.h"

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

// Creates a 401 Unauthorized error response with the required WWW-Authenticate
// header.
std::unique_ptr<HttpResponse> CreateUnauthorizedResponse() {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(HttpStatusCode::HTTP_UNAUTHORIZED);
  response->AddCustomHeader("WWW-Authenticate", "Basic realm=\"TestServer\"");
  response->set_content("Unauthorized");
  response->set_content_type("text/plain");
  return response;
}

// Callback to handle BasicAuth validation.
std::unique_ptr<HttpResponse> HandleBasicAuth(
    const std::string& expected_auth_header,
    const HttpRequest& request) {
  auto auth_header = request.headers.find("Authorization");

  if (auth_header == request.headers.end() ||
      auth_header->second != expected_auth_header) {
    DVLOG(1) << "Authorization failed or header missing.";
    return CreateUnauthorizedResponse();
  }

  DVLOG(3) << "Authorization successful.";
  return nullptr;
}

}  // namespace

void RegisterBasicAuthHandler(EmbeddedTestServer& server,
                              std::string_view username,
                              std::string_view password) {
  // Construct the expected authorization header value (e.g., "Basic
  // dXNlcm5hbWU6cGFzc3dvcmQ=")
  const std::string credentials = base::StrCat({username, ":", password});
  const std::string encoded_credentials = base::Base64Encode(credentials);
  const std::string expected_auth_header =
      base::StrCat({"Basic ", encoded_credentials});

  // Register the BasicAuth handler with the server.
  server.RegisterAuthHandler(
      base::BindRepeating(&HandleBasicAuth, expected_auth_header));
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
