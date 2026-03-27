// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
constexpr char kTestUrl[] = "https://google.com";
}

class PermissionServiceImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    const GURL url(kTestUrl);
    NavigateAndCommit(url);

    permission_controller_ =
        PermissionControllerImpl::FromBrowserContext(browser_context());
    permission_service_context_ =
        PermissionServiceContext::GetOrCreateForCurrentDocument(main_rfh());
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionControllerDelegate(
            permissions::GetPermissionControllerDelegate(browser_context()));
    permission_service_context_->CreateService(
        remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    permission_controller_ = nullptr;
    permission_service_context_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return permissions::PermissionsClient::Get()->GetSettingsMap(
        browser_context());
  }

  PermissionControllerImpl* permission_controller() {
    return permission_controller_;
  }

  mojo::Remote<blink::mojom::PermissionService>& remote() { return remote_; }

 private:
  base::test::ScopedFeatureList enable_approximate_location_{
      content_settings::features::kApproximateGeolocationPermission};

  mojo::Remote<blink::mojom::PermissionService> remote_;
  raw_ptr<PermissionControllerImpl> permission_controller_;
  raw_ptr<PermissionServiceContext> permission_service_context_;
  permissions::TestPermissionsClient client_;
};

TEST_F(PermissionServiceImplTest, HasPermission) {
  GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
      GURL(kTestUrl), GURL(), ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      GeolocationSetting{.approximate = PermissionOption::kAllowed,
                         .precise = PermissionOption::kAllowed});

  auto descriptor = blink::mojom::PermissionDescriptor::New();
  descriptor->name = blink::mojom::PermissionName::GEOLOCATION;
  base::test::TestFuture<blink::mojom::PermissionStatusWithDetailsPtr> future;
  remote()->HasPermission(std::move(descriptor), future.GetCallback());
  EXPECT_EQ(future.Take(),
            blink::mojom::PermissionStatusWithDetails::New(
                blink::mojom::PermissionStatus::GRANTED,
                blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                    blink::mojom::GeolocationAccuracy::kPrecise)));
}

}  // namespace content
