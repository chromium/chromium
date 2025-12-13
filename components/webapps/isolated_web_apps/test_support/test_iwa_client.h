// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_IWA_CLIENT_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_IWA_CLIENT_H_

#include "base/notreached.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app::test {

class TestIwaClient : public IwaClient {
 public:
  // IwaClient:
  void RunWhenAppCloses(content::BrowserContext* browser_context,
                        const web_package::SignedWebBundleId& web_bundle_id,
                        base::OnceClosure callback) override;
  void GetIwaSourceForRequest(
      content::BrowserContext* browser_context,
      const web_package::SignedWebBundleId& web_bundle_id,
      const network::ResourceRequest& request,
      const std::optional<content::FrameTreeNodeId>& frame_tree_node,
      base::OnceCallback<void(
          base::expected<IwaSourceWithModeOrGeneratedResponse, std::string>)>
          callback) override;
  IwaRuntimeDataProvider* GetRuntimeDataProvider() override;
};

class MockIwaClient : public TestIwaClient {
 public:
  MockIwaClient();
  ~MockIwaClient() override;

  MOCK_METHOD((base::expected<void, std::string>),
              ValidateTrust,
              (content::BrowserContext*,
               const web_package::SignedWebBundleId&,
               bool),
              (override));
};

}  // namespace web_app::test

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TEST_SUPPORT_TEST_IWA_CLIENT_H_
