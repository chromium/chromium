// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/transitional_url_loader_factory_owner.h"

#include <memory>
#include <string>

#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

class TransitionalURLLoaderFactoryOwnerTest : public ::testing::Test {
 public:
  TransitionalURLLoaderFactoryOwnerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    net::test_server::RegisterDefaultHandlers(&test_server_);
    ASSERT_TRUE(test_server_.Start());
  }

  void TestOnTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        base::OnceClosure flush_thread) {
    auto url_request_context_getter =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(task_runner);
    auto owner = std::make_unique<TransitionalURLLoaderFactoryOwner>(
        url_request_context_getter);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        owner->GetURLLoaderFactory();
    ASSERT_TRUE(url_loader_factory != nullptr);

    // Try fetching something --- see if |url_loader_factory| is usable.
    auto request = std::make_unique<ResourceRequest>();
    request->url = test_server_.GetURL("/cachetime");
    std::unique_ptr<SimpleURLLoader> loader = SimpleURLLoader::Create(
        std::move(request), TRAFFIC_ANNOTATION_FOR_TESTS);

    base::RunLoop run_loop;
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindLambdaForTesting([&](std::unique_ptr<std::string> body) {
          ASSERT_TRUE(body);
          EXPECT_NE(std::string::npos, body->find("<title>Cache:")) << *body;
          run_loop.Quit();
        }));
    run_loop.Run();

    // Clean stuff up, should be clean on lsan.
    owner = nullptr;
    std::move(flush_thread).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer test_server_;
};

TEST_F(TransitionalURLLoaderFactoryOwnerTest, CrossThread) {
  base::Thread io_thread("IO");
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  ASSERT_TRUE(io_thread.StartWithOptions(options));

  TestOnTaskRunner(io_thread.task_runner(), base::BindLambdaForTesting([&]() {
                     io_thread.FlushForTesting();
                   }));
}

TEST_F(TransitionalURLLoaderFactoryOwnerTest, SameThread) {
  TestOnTaskRunner(
      task_environment_.GetMainThreadTaskRunner(),
      base::BindLambdaForTesting([&]() { task_environment_.RunUntilIdle(); }));
}

}  // namespace
}  // namespace network
