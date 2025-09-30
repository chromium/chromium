// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_
#define NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "net/device_bound_sessions/registration_fetcher.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// Return a hard-coded RS256 public key's SPKI bytes and JWK string for testing.
std::pair<base::span<const uint8_t>, std::string>
GetRS256SpkiAndJwkForTesting();

// Returns the public key used for Origin Trial tokens in
// `GetTestRequestHandler`.
extern const char kTestOriginTrialPublicKey[];

// Returns a request handler suitable for use with
// `EmbeddedTestServer`. The server allows registration of device bound
// sessions.
EmbeddedTestServer::HandleRequestCallback GetTestRequestHandler(
    const GURL& base_url);

// Verify the signature of a JWT using the ES256 JWK stored in it.
bool VerifyEs256Jwt(std::string_view jwt);

#if BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)
// While this class is alive, session registration will always return a
// fixed value.
class ScopedTestRegistrationFetcher {
 public:
  // Creates a `ScopedTestRegistrationFetcher` that always succeeds at
  // registering a session with the given `session_id`,
  // `refresh_url_string`, and `origin_string`.
  static ScopedTestRegistrationFetcher CreateWithSuccess(
      std::string_view session_id,
      std::string_view refresh_url_string,
      std::string_view origin_string);

  // Creates a `ScopedTestRegistrationFetcher` that always fails to register
  static ScopedTestRegistrationFetcher CreateWithFailure(
      SessionError::ErrorType error_type,
      std::string_view refresh_url_string);

  // Creates a `ScopedTestRegistrationFetcher` that always instructs
  // Chrome to terminate the session with given id and site.
  static ScopedTestRegistrationFetcher CreateWithTermination(
      std::string_view session_id,
      std::string_view refresh_url_string);

  explicit ScopedTestRegistrationFetcher(
      RegistrationFetcher::FetcherType fetcher);
  ~ScopedTestRegistrationFetcher();

 private:
  RegistrationFetcher::FetcherType fetcher_;
};
#endif  // BUILDFLAG(ENABLE_DEVICE_BOUND_SESSIONS)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_TEST_SUPPORT_H_
