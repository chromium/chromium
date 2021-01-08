// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle_manager.h"

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

const char kBundleUrl[] = "https://example.com/bundle.wbn";

}  // namespace

class WebBundleManagerTest : public testing::Test {
 public:
  WebBundleManagerTest() = default;
  ~WebBundleManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleManagerTest, RemoveFactoryWhenDisconnected) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  {
    mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
    mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
        handle.InitWithNewPipeAndPassReceiver();
    auto token_params =
        ResourceRequest::WebBundleTokenParams(token, std::move(handle));

    auto factory = manager.CreateWebBundleURLLoaderFactory(
        GURL(kBundleUrl), token_params, mojom::URLLoaderFactoryParams::New());
    ASSERT_TRUE(factory);
    ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(token));
    // Getting out of scope to delete |receiver|.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager.GetWebBundleURLLoaderFactory(token))
      << "The manager should remove a factory when the handle is disconnected.";
}

}  // namespace network
