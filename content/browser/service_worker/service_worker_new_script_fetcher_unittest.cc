// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_new_script_fetcher.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerNewScriptFetcherTest : public testing::Test {
 public:
  ServiceWorkerNewScriptFetcherTest()
      : helper_(std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath())),
        registration_(CreateRegistration(kScriptUrl.GetWithoutFilename())) {}

  scoped_refptr<ServiceWorkerVersion> CreateNewVersion(const GURL& script_url) {
    auto version = CreateNewServiceWorkerVersion(
        context()->registry(), registration_, script_url,
        blink::mojom::ScriptType::kClassic);
    version->SetStatus(ServiceWorkerVersion::NEW);

    return version;
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  const GURL kScriptUrl{"https://example.com/fake-script.js"};

 private:
  scoped_refptr<ServiceWorkerRegistration> CreateRegistration(
      const GURL& scope) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    auto registration = CreateNewServiceWorkerRegistration(
        context()->registry(), options,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)));
    return registration;
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
};

TEST_F(ServiceWorkerNewScriptFetcherTest, Basic) {
  // Create a brand new ServiceWorkerVersion which is about to be registered.
  scoped_refptr<ServiceWorkerVersion> version = CreateNewVersion(kScriptUrl);
  EXPECT_EQ(
      blink::mojom::kInvalidServiceWorkerResourceId,
      version->script_cache_map()->LookupResourceId(version->script_url()));
  EXPECT_FALSE(version->policy_container_host());

  const std::string kBody = "/* body */";
  FakeNetworkURLLoaderFactory fake_factory{
      "HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n", kBody,
      /*network_accessed=*/true, net::OK};
  auto fetcher = std::make_unique<ServiceWorkerNewScriptFetcher>(
      *context(), version,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &fake_factory),
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId());

  // Start a fetcher and wait to get the result. The script loaded from
  // `loader_factory` is set to the `main_script_load_params` through
  // ServiceWorkerNewScriptLoader and ServiceWorkerNewScriptFetcher.
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params;
  base::RunLoop loop;
  fetcher->Start(base::BindLambdaForTesting(
      [&](blink::mojom::WorkerMainScriptLoadParamsPtr params) {
        main_script_load_params = std::move(params);
        loop.Quit();
      }));
  loop.Run();

  // The `main_script_load_params` contains the response header provided by
  // `loader_factory`.
  EXPECT_TRUE(main_script_load_params);
  EXPECT_EQ("text/javascript",
            main_script_load_params->response_head->mime_type);
  // Also some parameters are set to `version` before the callback of Start() is
  // called.
  EXPECT_NE(
      blink::mojom::kInvalidServiceWorkerResourceId,
      version->script_cache_map()->LookupResourceId(version->script_url()));
  EXPECT_TRUE(version->policy_container_host());

  // Wait until the network request for the main script completes.
  network::TestURLLoaderClient client;
  mojo::Receiver<network::mojom::URLLoaderClient> receiver(
      &client, std::move(main_script_load_params->url_loader_client_endpoints
                             ->url_loader_client));
  client.RunUntilComplete();

  std::string loaded_body;
  EXPECT_TRUE(mojo::BlockingCopyToString(
      std::move(main_script_load_params->response_body), &loaded_body));
  EXPECT_EQ(kBody, loaded_body);
}

}  // namespace content
