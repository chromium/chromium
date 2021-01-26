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
const int32_t process_id1 = 100;
const int32_t process_id2 = 200;

}  // namespace

class WebBundleManagerTest : public testing::Test {
 public:
  WebBundleManagerTest() = default;
  ~WebBundleManagerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleManagerTest, NoFactoryExistsForDifferentProcessId) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = process_id1;
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(token, std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, factory_params);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params(token,
                                                    mojom::kInvalidProcessId);
  ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(find_params,
                                                   factory_params->process_id));
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params, process_id2));
}

TEST_F(WebBundleManagerTest, UseProcesIdInTokenParamsForRequestsFromBrowser) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = process_id1;
  mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
  mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
      handle.InitWithNewPipeAndPassReceiver();
  ResourceRequest::WebBundleTokenParams create_params(token, std::move(handle));

  auto factory = manager.CreateWebBundleURLLoaderFactory(
      GURL(kBundleUrl), create_params, factory_params);
  ASSERT_TRUE(factory);

  ResourceRequest::WebBundleTokenParams find_params1(token, process_id1);
  ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(find_params1,
                                                   mojom::kBrowserProcessId));
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params1, process_id2));
  ResourceRequest::WebBundleTokenParams find_params2(token, process_id2);
  ASSERT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params2,
                                                    mojom::kBrowserProcessId));
}

TEST_F(WebBundleManagerTest, RemoveFactoryWhenDisconnected) {
  WebBundleManager manager;
  base::UnguessableToken token = base::UnguessableToken::Create();
  ResourceRequest::WebBundleTokenParams find_params(token,
                                                    mojom::kInvalidProcessId);
  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = 123;
  {
    mojo::PendingRemote<network::mojom::WebBundleHandle> handle;
    mojo::PendingReceiver<network::mojom::WebBundleHandle> receiver =
        handle.InitWithNewPipeAndPassReceiver();
    ResourceRequest::WebBundleTokenParams create_params(token,
                                                        std::move(handle));

    auto factory = manager.CreateWebBundleURLLoaderFactory(
        GURL(kBundleUrl), create_params, factory_params);
    ASSERT_TRUE(factory);
    ASSERT_TRUE(manager.GetWebBundleURLLoaderFactory(
        find_params, factory_params->process_id));
    // Getting out of scope to delete |receiver|.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(manager.GetWebBundleURLLoaderFactory(find_params,
                                                    factory_params->process_id))
      << "The manager should remove a factory when the handle is disconnected.";
}

}  // namespace network
