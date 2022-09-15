// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TRUST_TOKEN_TEST_SERVER_HANDLER_REGISTRATION_H_
#define SERVICES_NETWORK_TEST_TRUST_TOKEN_TEST_SERVER_HANDLER_REGISTRATION_H_

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace network {

namespace test {

class TrustTokenRequestHandler;

// Wires |handler|'s issuance, redemption, and signed request verification
// methods up to |test_server| at standard endpoint locations ("/issue",
// "/redeem", and "/sign", relative to |test_server|'s base URL). This lets
// browser test files relying on Trust Tokens server logic share this
// initialization boilerplate.
//
// Usage:
// - This must be called before starting |test_server|.
// - Feel free to call it multiple times with distinct values of |base_url| and
// |handler|, in order to emulate multiple Trust Tokens issuer servers.
//
// Lifetime: because this registers callbacks on |test_server| that call into
// |handler|, |handler| needs to live long enough to service all requests (e.g.
// in a test) that will arrive at |test_server|.
void RegisterTrustTokenTestHandlers(net::EmbeddedTestServer* test_server,
                                    TrustTokenRequestHandler* handler);

}  // namespace test

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TRUST_TOKEN_TEST_SERVER_HANDLER_REGISTRATION_H_
