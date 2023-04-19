// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

class WebNNContextImplTest : public testing::Test {
 public:
  WebNNContextImplTest(const WebNNContextImplTest&) = delete;
  WebNNContextImplTest& operator=(const WebNNContextImplTest&) = delete;

 protected:
  WebNNContextImplTest() = default;
  ~WebNNContextImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebNNContextImplTest, CreateWebNNGraphTest) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;
  mojo::Remote<mojom::WebNNContext> webnn_context_remote;

  WebNNContextProviderImpl::Create(
      provider_remote.BindNewPipeAndPassReceiver());

  bool is_callback_called = false;
  base::RunLoop run_loop_create_context;
  auto options = mojom::CreateContextOptions::New();
  provider_remote->CreateWebNNContext(
      std::move(options),
      base::BindLambdaForTesting(
          [&](mojom::CreateContextResult result,
              mojo::PendingRemote<mojom::WebNNContext> remote) {
#if BUILDFLAG(IS_WIN)
            EXPECT_EQ(result, mojom::CreateContextResult::kOk);
            webnn_context_remote.Bind(std::move(remote));
#else
            EXPECT_EQ(result, mojom::CreateContextResult::kNotSupported);
#endif
            is_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(is_callback_called);

  if (!webnn_context_remote.is_bound()) {
    // Don't continue testing for unsupported platforms.
    return;
  }

  base::RunLoop run_loop_create_graph;
  is_callback_called = false;
  webnn_context_remote->CreateGraph(base::BindLambdaForTesting(
      [&](mojo::PendingRemote<mojom::WebNNGraph> remote) {
        EXPECT_TRUE(remote.is_valid());
        is_callback_called = true;
        run_loop_create_graph.Quit();
      }));
  run_loop_create_graph.Run();
  EXPECT_TRUE(is_callback_called);
}

}  // namespace webnn
