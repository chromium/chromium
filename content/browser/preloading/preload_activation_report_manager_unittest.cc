// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_manager.h"

#include "base/test/bind.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class PreloadActivationReportManagerTest : public RenderViewHostTestHarness {
 public:
  PreloadActivationReportManagerTest() = default;
};

TEST_F(PreloadActivationReportManagerTest, ReportActivation) {
  auto* manager = PreloadActivationReportManager::GetOrCreateForBrowserContext(
      web_contents()->GetBrowserContext());
  ASSERT_TRUE(manager);

  GURL endpoint("https://example.com/beacon");

  base::RunLoop run_loop;
  bool request_seen = false;

  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == endpoint) {
          request_seen = true;
          EXPECT_EQ(params->url_request.method, "HEAD");
          EXPECT_EQ(params->url_request.credentials_mode,
                    network::mojom::CredentialsMode::kOmit);

          // Respond to the request to avoid hanging.
          URLLoaderInterceptor::WriteResponse("", "", params->client.get());

          run_loop.Quit();
          return true;
        }
        return false;
      }));

  manager->ReportActivation(endpoint, web_contents());
  run_loop.Run();

  EXPECT_TRUE(request_seen);
}

}  // namespace content
