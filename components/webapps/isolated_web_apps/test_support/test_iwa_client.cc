// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/test_support/test_iwa_client.h"

#include "base/notreached.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_app::test {

base::expected<web_package::SignedWebBundleId, std::string>
TestIwaClient::CreateWebBundleIdFromURL(const GURL& url) {
  if (url.SchemeIs("isolated-app")) {
    return web_package::SignedWebBundleId::Create(url.host_piece());
  }
  return base::unexpected("Wrong scheme.");
}

GURL TestIwaClient::CreateBaseURLForWebBundleId(
    const web_package::SignedWebBundleId& web_bundle_id) {
  return GURL(base::StrCat(
      {"isolated-app", url::kStandardSchemeSeparator, web_bundle_id.id()}));
}

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

content::StoragePartition* TestIwaClient::GetStoragePartition(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id) {
  NOTREACHED();
}

MockIwaClient::MockIwaClient() = default;
MockIwaClient::~MockIwaClient() = default;

}  // namespace web_app::test
