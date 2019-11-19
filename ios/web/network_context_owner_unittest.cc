// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/network_context_owner.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

class NetworkContextOwnerTest : public PlatformTest {
 protected:
  NetworkContextOwnerTest()
      : saw_connection_error_(false),
        context_getter_(base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunner({WebThread::IO}))) {}

  ~NetworkContextOwnerTest() override {
    // Tests should cleanup after themselves.
    EXPECT_EQ(network_context_owner_.get(), nullptr);
  }

  void WatchForErrors() {
    ASSERT_TRUE(network_context_.is_bound());
    network_context_.set_disconnect_handler(base::BindOnce(
        &NetworkContextOwnerTest::SawError, base::Unretained(this)));
  }

  void SawError() { saw_connection_error_ = true; }

  bool saw_connection_error_;
  WebTaskEnvironment task_environment_;
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<NetworkContextOwner> network_context_owner_;
};

// Test that NetworkContextOwner actually creates a NetworkContext owner and
// connects a pipe to it, and destroys its end of the pipe when it's gone.
TEST_F(NetworkContextOwnerTest, Basic) {
  EXPECT_FALSE(network_context_.is_bound());
  network_context_owner_ = std::make_unique<NetworkContextOwner>(
      context_getter_.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>(),
      &network_context_);
  EXPECT_TRUE(network_context_.is_bound());
  WatchForErrors();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(saw_connection_error_);

  base::DeleteSoon(FROM_HERE, {web::WebThread::IO},
                   network_context_owner_.release());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(saw_connection_error_);  // other end gone
}

// Test to make sure that explicit shutdown of URLRequestContextGetter destroys
// the NetworkContext object as expected.
TEST_F(NetworkContextOwnerTest, ShutdownHandling) {
  EXPECT_FALSE(network_context_.is_bound());
  network_context_owner_ = std::make_unique<NetworkContextOwner>(
      context_getter_.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>(),
      &network_context_);
  EXPECT_TRUE(network_context_.is_bound());
  WatchForErrors();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(saw_connection_error_);

  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(
          &net::TestURLRequestContextGetter::NotifyContextShuttingDown,
          context_getter_));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(saw_connection_error_);  // other end gone post-shutdown.

  base::DeleteSoon(FROM_HERE, {web::WebThread::IO},
                   network_context_owner_.release());
}

}  // namespace web
