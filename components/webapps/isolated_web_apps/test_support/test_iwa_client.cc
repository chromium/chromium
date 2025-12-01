// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/test_support/test_iwa_client.h"

#include "base/notreached.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app::test {

void TestIwaClient::RunWhenAppCloses(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::OnceClosure callback) {
  NOTREACHED();
}

void TestIwaClient::GetIwaSourceForRequest(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& request,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node,
    base::OnceCallback<void(base::expected<IwaSourceWithModeOrGeneratedResponse,
                                           std::string>)> callback) {
  NOTREACHED();
}

IwaRuntimeDataProvider* TestIwaClient::GetRuntimeDataProvider() {
  return nullptr;
}

MockIwaClient::MockIwaClient() = default;
MockIwaClient::~MockIwaClient() = default;

}  // namespace web_app::test
