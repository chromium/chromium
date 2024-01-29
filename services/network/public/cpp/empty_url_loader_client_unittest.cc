// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/empty_url_loader_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

void CheckBodyIsDrained(const std::string& body) {
  auto empty_client = std::make_unique<EmptyURLLoaderClient>();
  base::RunLoop loop;
  URLLoaderCompletionStatus result;
  // Set an arbitrary error code other than net::OK to check `result` is
  // set later.
  result.error_code = net::ERR_ABORTED;
  empty_client->Drain(
      base::BindLambdaForTesting([&](const URLLoaderCompletionStatus& status) {
        result = status;
        loop.Quit();
      }));

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(/*options=*/nullptr, producer_handle,
                                 consumer_handle),
            MOJO_RESULT_OK);

  // Upcasting to mojom::URLLoaderClient to access mojom methods.
  mojom::URLLoaderClient* client = empty_client.get();
  client->OnReceiveResponse(nullptr, std::move(consumer_handle), std::nullopt);
  client->OnComplete(URLLoaderCompletionStatus(net::OK));

  EXPECT_TRUE(mojo::BlockingCopyFromString(body, producer_handle));
  producer_handle.reset();

  loop.Run();
  EXPECT_EQ(result.error_code, net::OK);
}

}  // namespace

TEST(EmptyURLLoaderClientTest, WithoutBody) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto empty_client = std::make_unique<EmptyURLLoaderClient>();
  base::RunLoop loop;
  URLLoaderCompletionStatus result;
  empty_client->Drain(
      base::BindLambdaForTesting([&](const URLLoaderCompletionStatus& status) {
        result = status;
        loop.Quit();
      }));

  mojom::URLLoaderClient* client = empty_client.get();
  client->OnComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));

  loop.Run();
  EXPECT_EQ(result.error_code, net::ERR_ABORTED);
}

TEST(EmptyURLLoaderClientTest, DrainBody) {
  base::test::SingleThreadTaskEnvironment task_environment;
  CheckBodyIsDrained("body");
}

TEST(EmptyURLLoaderClientTest, DrainEmptyBody) {
  base::test::SingleThreadTaskEnvironment task_environment;
  CheckBodyIsDrained("");
}

}  // namespace network
