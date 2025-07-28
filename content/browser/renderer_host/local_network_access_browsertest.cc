// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/local_network_access_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class LocalNetworkAccessDeprecationBrowserTest : public ContentBrowserTest {
 public:
  LocalNetworkAccessDeprecationBrowserTest() {
    // Some builders run with field_trial disabled, need to enable this
    // manually.
    base::FieldTrialParams params;
    params["LocalNetworkAccessChecksWarn"] = "false";
    features_.InitAndEnableFeatureWithParameters(
        network::features::kLocalNetworkAccessChecks, params);
  }

  RenderFrameHostImpl* root_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialOriginEnabledHttp) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledHttpUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialOriginEnabledHttps) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.EnabledHttpsUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialOriginDisabledHttp) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.DisabledHttpUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  // kBlock instead of kPermissionBlock because the URL is http.
  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kBlock);
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessDeprecationBrowserTest,
                       DeprecationTrialOriginDisabledHttps) {
  DeprecationTrialURLLoaderInterceptor interceptor;

  EXPECT_TRUE(NavigateToURL(shell(), interceptor.DisabledHttpsUrl()));

  const network::mojom::ClientSecurityStatePtr security_state =
      root_frame_host()->BuildClientSecurityState();
  ASSERT_FALSE(security_state.is_null());

  EXPECT_EQ(security_state->private_network_request_policy,
            network::mojom::PrivateNetworkRequestPolicy::kPermissionBlock);
}
}  // namespace content
