// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SPAWNED_TEST_SERVER_SPAWNED_TEST_SERVER_H_
#define NET_TEST_SPAWNED_TEST_SERVER_SPAWNED_TEST_SERVER_H_

#include "build/build_config.h"

#if defined(USE_REMOTE_TEST_SERVER)
#include "net/test/spawned_test_server/remote_test_server.h"
#else
#include "net/test/spawned_test_server/local_test_server.h"
#endif

namespace net {

#if defined(USE_REMOTE_TEST_SERVER)
typedef RemoteTestServer SpawnedTestServer;
#else
typedef LocalTestServer SpawnedTestServer;
#endif

}  // namespace net

#endif  // NET_TEST_SPAWNED_TEST_SERVER_SPAWNED_TEST_SERVER_H_
