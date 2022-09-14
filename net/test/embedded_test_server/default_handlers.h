// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_DEFAULT_HANDLERS_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_DEFAULT_HANDLERS_H_

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace net::test_server {

// This file is only meant for compatibility with testserver.py. No
// additional handlers should be added here that don't affect multiple
// distinct tests.

// Registers default handlers for use in tests.
void RegisterDefaultHandlers(EmbeddedTestServer* server);

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_DEFAULT_HANDLERS_H_
