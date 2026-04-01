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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::Not;

constexpr char kTestUrl[] = "https://google.com";
}  // namespace

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

TEST_F(PermissionServiceImplTest, RequestPermission) {
  for (const auto& [geolocation_setting, expected_status] : {
           std::make_pair(
               GeolocationSetting{.approximate = PermissionOption::kAllowed,
                                  .precise = PermissionOption::kAllowed},
               blink::mojom::PermissionStatusWithDetails::New(
                   blink::mojom::PermissionStatus::GRANTED,
                   blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                       blink::mojom::GeolocationAccuracy::kPrecise))),
           std::make_pair(
               GeolocationSetting{.approximate = PermissionOption::kAllowed,
                                  .precise = PermissionOption::kDenied},
               blink::mojom::PermissionStatusWithDetails::New(
                   blink::mojom::PermissionStatus::GRANTED,
                   blink::mojom::PermissionDetails::NewGeolocationAccuracy(
                       blink::mojom::GeolocationAccuracy::kApproximate))),
           std::make_pair(
               GeolocationSetting{.approximate = PermissionOption::kDenied,
                                  .precise = PermissionOption::kDenied},
               blink::mojom::PermissionStatusWithDetails::New(
                   blink::mojom::PermissionStatus::DENIED, nullptr)),
       }) {
    GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
        GURL(kTestUrl), GURL(), ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
        geolocation_setting);

    auto descriptor = blink::mojom::PermissionDescriptor::New();
    descriptor->name = blink::mojom::PermissionName::GEOLOCATION;

    base::test::TestFuture<blink::mojom::PermissionStatusWithDetailsPtr> future;
    remote()->RequestPermission(std::move(descriptor), future.GetCallback());
    EXPECT_EQ(future.Take(), expected_status);
  }
}

class MockPermissionObserver : public blink::mojom::PermissionObserver {
 public:
  explicit MockPermissionObserver(
      mojo::PendingReceiver<blink::mojom::PermissionObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  MOCK_METHOD(void,
              OnPermissionStatusChange,
              (blink::mojom::PermissionStatusWithDetailsPtr),
              (override));

 private:
  mojo::Receiver<blink::mojom::PermissionObserver> receiver_;
};

TEST_F(PermissionServiceImplTest, AddPermissionObserver) {
  GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
      GURL(kTestUrl), GURL(), ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      GeolocationSetting{.approximate = PermissionOption::kAllowed,
                         .precise = PermissionOption::kDenied});

  auto descriptor = blink::mojom::PermissionDescriptor::New();
  descriptor->name = blink::mojom::PermissionName::GEOLOCATION;

  base::test::TestFuture<blink::mojom::PermissionStatusWithDetailsPtr> future;
  remote()->HasPermission(descriptor.Clone(), future.GetCallback());

  auto initial_status = blink::mojom::PermissionStatusWithDetails::New(
      blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionDetails::NewGeolocationAccuracy(
          blink::mojom::GeolocationAccuracy::kApproximate));
  EXPECT_EQ(future.Take(), initial_status);

  mojo::PendingRemote<blink::mojom::PermissionObserver> observer_remote;
  MockPermissionObserver observer(
      observer_remote.InitWithNewPipeAndPassReceiver());
  remote()->AddPermissionObserver(descriptor.Clone(), std::move(initial_status),
                                  std::move(observer_remote));

  // Changing precise from Denied to Ask shouldn't result in a status update.
  GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
      GURL(kTestUrl), GURL(), ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      GeolocationSetting{.approximate = PermissionOption::kAllowed,
                         .precise = PermissionOption::kAsk});

  // Changing precise from Ask to Allowed should trigger an update.
  GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
      GURL(kTestUrl), GURL(), ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      GeolocationSetting{.approximate = PermissionOption::kAllowed,
                         .precise = PermissionOption::kAllowed});

  auto expected_new_status = blink::mojom::PermissionStatusWithDetails::New(
      blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionDetails::NewGeolocationAccuracy(
          blink::mojom::GeolocationAccuracy::kPrecise));

  // OnPermissionStatusChange should be called only once with
  // expected_new_status.
  EXPECT_CALL(observer,
              OnPermissionStatusChange(Not(Eq(std::ref(expected_new_status)))))
      .Times(0);
  base::RunLoop run_loop;
  base::RepeatingClosure done = run_loop.QuitClosure();
  EXPECT_CALL(observer,
              OnPermissionStatusChange(Eq(std::ref(expected_new_status))))
      .WillOnce(InvokeWithoutArgs([done]() { done.Run(); }));
  run_loop.Run();
}

}  // namespace content
