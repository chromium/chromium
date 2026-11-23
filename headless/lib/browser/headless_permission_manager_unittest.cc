// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_permission_manager.h"

#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace headless {
namespace {

using blink::PermissionType;
using blink::mojom::PermissionStatus;

blink::mojom::PermissionDescriptorPtr CreateDescriptor(PermissionType type) {
  return content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(type);
}

class HeadlessPermissionManagerTest : public testing::Test {
 protected:
  // Returns the status reported by every query method for `type`, after
  // checking that they agree.
  PermissionStatus GetStatus(PermissionType type) {
    const GURL url("https://example.com");
    const url::Origin origin = url::Origin::Create(url);
    blink::mojom::PermissionDescriptorPtr descriptor = CreateDescriptor(type);

    PermissionStatus status =
        manager_.GetPermissionStatus(descriptor, url, url);
    EXPECT_EQ(status, manager_
                          .GetPermissionResultForOriginWithoutContext(
                              descriptor, origin, origin)
                          .status);
    EXPECT_EQ(status, manager_
                          .GetPermissionResultForCurrentDocument(
                              descriptor, /*render_frame_host=*/nullptr,
                              /*should_include_device_status=*/false)
                          .status);
    EXPECT_EQ(status, manager_
                          .GetPermissionResultForWorker(
                              descriptor, /*render_process_host=*/nullptr, url)
                          .status);
    return status;
  }

  HeadlessPermissionManager manager_;
};

TEST_F(HeadlessPermissionManagerTest, PermissionsDefaultToAsk) {
  EXPECT_EQ(GetStatus(PermissionType::GEOLOCATION), PermissionStatus::ASK);
  EXPECT_EQ(GetStatus(PermissionType::NOTIFICATIONS), PermissionStatus::ASK);
}

// The remoteClientDataJSON permission is never prompted for, and headless
// never authorizes a caller to use the extension.
// https://w3c.github.io/webauthn/#sctn-remote-client-data-json-extension
TEST_F(HeadlessPermissionManagerTest, RemoteClientDataJsonIsDenied) {
  EXPECT_EQ(
      GetStatus(PermissionType::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON),
      PermissionStatus::DENIED);
}

TEST_F(HeadlessPermissionManagerTest, RequestDeniesRemoteClientDataJson) {
  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.push_back(CreateDescriptor(PermissionType::GEOLOCATION));
  descriptors.push_back(CreateDescriptor(
      PermissionType::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON));

  std::vector<content::PermissionResult> results;
  manager_.RequestPermissionsFromCurrentDocument(
      /*render_frame_host=*/nullptr,
      content::PermissionRequestDescription(std::move(descriptors)),
      base::BindLambdaForTesting(
          [&](const std::vector<content::PermissionResult>& result) {
            results = result;
          }));

  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].status, PermissionStatus::ASK);
  EXPECT_EQ(results[1].status, PermissionStatus::DENIED);
}

}  // namespace
}  // namespace headless
