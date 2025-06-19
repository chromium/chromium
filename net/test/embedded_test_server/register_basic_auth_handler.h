// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_REGISTER_BASIC_AUTH_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_REGISTER_BASIC_AUTH_HANDLER_H_

#include <string_view>

#include "url/gurl.h"

namespace net::test_server {

class EmbeddedTestServer;

// Registers a BasicAuth handler with a username and password.
void RegisterBasicAuthHandler(EmbeddedTestServer& server,
                              std::string_view username,
                              std::string_view password);

// Registers a BasicAuth handler with a username and password that mimics proxy
// auth. Will overwrite any other auth handler (including non-proxy auth
// handlers).
void RegisterProxyBasicAuthHandler(EmbeddedTestServer& server,
                                   std::string_view username,
                                   std::string_view password);

// Helper to generate a URL with username for Basic Authentication.
GURL GetURLWithUser(const EmbeddedTestServer& server,
                    std::string_view path,
                    std::string_view user);

// Helper to generate a URL with username and password for Basic Authentication.
GURL GetURLWithUserAndPassword(const EmbeddedTestServer& server,
                               std::string_view path,
                               std::string_view user,
                               std::string_view password);

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_REGISTER_BASIC_AUTH_HANDLER_H_
