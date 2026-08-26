// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_impl.h"

#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "https://example.com";

}  // namespace

class WakeLockServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  WakeLockServiceImplTest() = default;
  ~WakeLockServiceImplTest() override = default;

  WakeLockServiceImplTest(const WakeLockServiceImplTest&) = delete;
  WakeLockServiceImplTest& operator=(const WakeLockServiceImplTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(GURL(kTestUrl));
  }

  mojo::Remote<blink::mojom::WakeLockService> BindWakeLockService(
      RenderFrameHost* rfh = nullptr) {
    if (!rfh) {
      rfh = main_rfh();
    }
    mojo::Remote<blink::mojom::WakeLockService> service;
    WakeLockServiceImpl::Create(rfh, service.BindNewPipeAndPassReceiver());
    return service;
  }
};

TEST_F(WakeLockServiceImplTest, RequestScreenWakeLockSuccess) {
  mojo::Remote<blink::mojom::WakeLockService> service = BindWakeLockService();

  mojo::Remote<device::mojom::WakeLock> wake_lock;
  service->GetWakeLock(device::mojom::WakeLockType::kPreventDisplaySleep,
                       device::mojom::WakeLockReason::kOther, "TestDescription",
                       wake_lock.BindNewPipeAndPassReceiver());

  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
  wake_lock.FlushForTesting();
  EXPECT_TRUE(wake_lock.is_connected());
}

TEST_F(WakeLockServiceImplTest, RejectAppSuspensionWakeLock) {
  mojo::Remote<blink::mojom::WakeLockService> service = BindWakeLockService();

  mojo::Remote<device::mojom::WakeLock> wake_lock;
  service->GetWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                       device::mojom::WakeLockReason::kOther, "TestDescription",
                       wake_lock.BindNewPipeAndPassReceiver());

  wake_lock.FlushForTesting();
  EXPECT_FALSE(wake_lock.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

TEST_F(WakeLockServiceImplTest, RejectDisplaySleepAllowDimmingWakeLock) {
  mojo::Remote<blink::mojom::WakeLockService> service = BindWakeLockService();

  mojo::Remote<device::mojom::WakeLock> wake_lock;
  service->GetWakeLock(
      device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming,
      device::mojom::WakeLockReason::kOther, "TestDescription",
      wake_lock.BindNewPipeAndPassReceiver());

  wake_lock.FlushForTesting();
  EXPECT_FALSE(wake_lock.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

TEST_F(WakeLockServiceImplTest, DisallowedByPermissionsPolicy) {
  // Navigate with permissions policy disallowing screen-wake-lock.
  network::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      network::mojom::PermissionsPolicyFeature::kScreenWakeLock;

  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl), main_rfh());
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  mojo::Remote<blink::mojom::WakeLockService> service = BindWakeLockService();

  mojo::Remote<device::mojom::WakeLock> wake_lock;
  service->GetWakeLock(device::mojom::WakeLockType::kPreventDisplaySleep,
                       device::mojom::WakeLockReason::kOther, "TestDescription",
                       wake_lock.BindNewPipeAndPassReceiver());

  wake_lock.FlushForTesting();
  EXPECT_FALSE(wake_lock.is_connected());
  service.FlushForTesting();
  EXPECT_TRUE(service.is_connected());
}

}  // namespace content
