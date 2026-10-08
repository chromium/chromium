// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/no_op_mdns_responder.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class NoOpMdnsResponderTest : public testing::Test {
 public:
  NoOpMdnsResponderTest() {
    NoOpMdnsResponder::Create(responder_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<network::mojom::MdnsResponder> responder_;
};

// CreateNameForAddress() must always run its callback, without creating a name
// or scheduling an announcement.
TEST_F(NoOpMdnsResponderTest, CreateNameForAddressFails) {
  base::test::TestFuture<const std::string&, bool> future;
  responder_->CreateNameForAddress(net::IPAddress(192, 168, 0, 1),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get<0>().empty());
  EXPECT_FALSE(future.Get<1>());
  EXPECT_TRUE(responder_.is_connected());
}

// Repeated requests for the same address also fail; no name is ever retained.
TEST_F(NoOpMdnsResponderTest, CreateNameForAddressRepeatedlyFails) {
  const net::IPAddress address(10, 0, 0, 1);
  for (int i = 0; i < 3; ++i) {
    base::test::TestFuture<const std::string&, bool> future;
    responder_->CreateNameForAddress(address, future.GetCallback());
    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get<0>().empty());
    EXPECT_FALSE(future.Get<1>());
  }
  EXPECT_TRUE(responder_.is_connected());
}

// RemoveNameForAddress() must always run its callback, reporting that nothing
// was removed and no goodbye was scheduled.
TEST_F(NoOpMdnsResponderTest, RemoveNameForAddressFails) {
  base::test::TestFuture<bool, bool> future;
  responder_->RemoveNameForAddress(net::IPAddress(192, 168, 0, 1),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
  EXPECT_TRUE(responder_.is_connected());
}

// Works for IPv6 addresses too.
TEST_F(NoOpMdnsResponderTest, Ipv6AddressFails) {
  base::test::TestFuture<const std::string&, bool> create_future;
  responder_->CreateNameForAddress(net::IPAddress::IPv6Localhost(),
                                   create_future.GetCallback());
  ASSERT_TRUE(create_future.Wait());
  EXPECT_TRUE(create_future.Get<0>().empty());
  EXPECT_FALSE(create_future.Get<1>());

  base::test::TestFuture<bool, bool> remove_future;
  responder_->RemoveNameForAddress(net::IPAddress::IPv6Localhost(),
                                   remove_future.GetCallback());
  ASSERT_TRUE(remove_future.Wait());
  EXPECT_FALSE(remove_future.Get<0>());
  EXPECT_FALSE(remove_future.Get<1>());
}

}  // namespace

}  // namespace content
