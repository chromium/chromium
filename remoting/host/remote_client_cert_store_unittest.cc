// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_client_cert_store.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class RemoteClientCertStoreTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(RemoteClientCertStoreTest, StubReturnsEmptyList) {
  RemoteClientCertStore store;
  base::test::TestFuture<net::ClientCertIdentityList> future;
  store.GetClientCerts(base::MakeRefCounted<net::SSLCertRequestInfo>(),
                       future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace remoting
