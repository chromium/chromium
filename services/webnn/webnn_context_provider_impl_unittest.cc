// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/webnn_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

class WebNNContextProviderImplTest : public testing::Test {
 public:
  WebNNContextProviderImplTest(const WebNNContextProviderImplTest&) = delete;
  WebNNContextProviderImplTest& operator=(const WebNNContextProviderImplTest&) =
      delete;

 protected:
  WebNNContextProviderImplTest() = default;
  ~WebNNContextProviderImplTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

// `WebNNContextProviderImplTest` only focuses on the non-supported platforms.
// For supported platforms, it should be tested by the backend specific test
// cases.
//
// For Windows platform, `dml::ContextImpl` is implemented by the DirectML
// backend. It relies on a real GPU adapter and is tested by
// `WebNNContextDMLImplTest`.

#if !BUILDFLAG(IS_WIN)

TEST_F(WebNNContextProviderImplTest, CreateWebNNContextTest) {
  mojo::Remote<mojom::WebNNContextProvider> provider_remote;

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
            EXPECT_EQ(result, mojom::CreateContextResult::kNotSupported);
            is_callback_called = true;
            run_loop_create_context.Quit();
          }));
  run_loop_create_context.Run();
  EXPECT_TRUE(is_callback_called);
}

#endif

}  // namespace webnn
