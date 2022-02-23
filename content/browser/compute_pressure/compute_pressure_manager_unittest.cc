// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_manager.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/compute_pressure/compute_pressure_test_support.h"
#include "content/public/browser/global_routing_id.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

class ComputePressureManagerTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetManager(ComputePressureManager::CreateForTesting(
        std::make_unique<FakeCpuProbe>(), base::Milliseconds(1),
        base::Milliseconds(1)));
  }

  void TearDown() override {
    // Get the CpuProbe freed.
    manager_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetManager(std::unique_ptr<ComputePressureManager> new_manager) {
    manager_ = std::move(new_manager);
    int process_id = main_rfh()->GetProcess()->GetID();
    int routing_id = main_rfh()->GetRoutingID();
    main_frame_id_ = GlobalRenderFrameHostId(process_id, routing_id);

    main_host_.reset();
    manager_->BindReceiver(kTestOrigin, main_frame_id_,
                           main_host_.BindNewPipeAndPassReceiver());
    main_host_sync_ =
        std::make_unique<ComputePressureHostSync>(main_host_.get());
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://example.com"));
  const url::Origin kThirdPartyOrigin =
      url::Origin::Create(GURL("https://other.com"));

  // Quantization scheme used in most tests.
  const blink::mojom::ComputePressureQuantization kQuantization = {
      {0.2, 0.5, 0.8},
      {0.5}};

  base::test::ScopedFeatureList scoped_feature_list_;

  GlobalRenderFrameHostId main_frame_id_;
  // This member is a std::unique_ptr instead of a plain ComputePressureManager
  // so it can be replaced inside tests.
  std::unique_ptr<ComputePressureManager> manager_;
  mojo::Remote<blink::mojom::ComputePressureHost> main_host_;
  std::unique_ptr<ComputePressureHostSync> main_host_sync_;
};

// Disabled on Fuchsia arm64 debug builds: https://crbug.com/1250654
#if BUILDFLAG(IS_FUCHSIA) && defined(_DEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_OneObserver DISABLED_OneObserver
#elif BUILDFLAG(IS_LINUX) && defined(USE_OZONE)  // https://crbug.com/1226086
#define MAYBE_OneObserver DISABLED_OneObserver
#else
#define MAYBE_OneObserver OneObserver
#endif
TEST_F(ComputePressureManagerTest, MAYBE_OneObserver) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  observer.WaitForUpdate();
  EXPECT_THAT(observer.updates(), testing::SizeIs(testing::Ge(1u)));
  EXPECT_THAT(
      observer.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
}

// Disabled on Fuchsia arm64 debug builds: https://crbug.com/1250654
#if BUILDFLAG(IS_FUCHSIA) && defined(_DEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_ThreeObservers DISABLED_ThreeObservers
#elif BUILDFLAG(IS_LINUX) && defined(USE_OZONE)  // https://crbug.com/1226086
#define MAYBE_ThreeObservers DISABLED_ThreeObservers
#else
#define MAYBE_ThreeObservers ThreeObservers
#endif
TEST_F(ComputePressureManagerTest, MAYBE_ThreeObservers) {
  FakeComputePressureObserver observer1;
  ASSERT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer1.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer2;
  ASSERT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer2.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer3;
  ASSERT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer3.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  FakeComputePressureObserver::WaitForUpdates(
      {&observer1, &observer2, &observer3});

  EXPECT_THAT(observer1.updates(), testing::SizeIs(testing::Ge(1u)));
  EXPECT_THAT(
      observer1.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
  EXPECT_THAT(observer2.updates(), testing::SizeIs(testing::Ge(1u)));
  EXPECT_THAT(
      observer2.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
  EXPECT_THAT(observer3.updates(), testing::SizeIs(testing::Ge(1u)));
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
}

TEST_F(ComputePressureManagerTest, AddObserver_ThirdPartyFrame) {
  mojo::Remote<blink::mojom::ComputePressureHost> third_party_host;
  manager_->BindReceiver(kThirdPartyOrigin, main_frame_id_,
                         third_party_host.BindNewPipeAndPassReceiver());
  auto third_party_host_sync =
      std::make_unique<ComputePressureHostSync>(third_party_host.get());

  FakeComputePressureObserver third_party_observer;
  EXPECT_EQ(third_party_host_sync->AddObserver(
                kQuantization, third_party_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kSecurityError);
  FakeComputePressureObserver first_party_observer;
  EXPECT_EQ(main_host_sync_->AddObserver(
                kQuantization, first_party_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
}

TEST_F(ComputePressureManagerTest, AddObserver_NoVisibility) {
  test_rvh()->SimulateWasHidden();

  FakeComputePressureObserver observer;
  EXPECT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
}

TEST_F(ComputePressureManagerTest, AddObserver_InvalidFrame) {
  GlobalRenderFrameHostId invalid_routing_id(
      main_rfh()->GetProcess()->GetID() + 1, main_rfh()->GetRoutingID() + 1);
  mojo::Remote<blink::mojom::ComputePressureHost> invalid_host;
  manager_->BindReceiver(kTestOrigin, invalid_routing_id,
                         invalid_host.BindNewPipeAndPassReceiver());
  auto invalid_host_sync =
      std::make_unique<ComputePressureHostSync>(invalid_host.get());
  FakeComputePressureObserver invalid_observer;
  EXPECT_EQ(invalid_host_sync->AddObserver(
                kQuantization, invalid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kSecurityError);
  FakeComputePressureObserver valid_observer;
  EXPECT_EQ(main_host_sync_->AddObserver(
                kQuantization, valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
}

TEST_F(ComputePressureManagerTest, AddObserver_InvalidQuantization) {
  FakeComputePressureObserver invalid_observer;
  blink::mojom::ComputePressureQuantization invalid_quantization(
      std::vector<double>{-1.0}, std::vector<double>{0.5});

  {
    mojo::test::BadMessageObserver bad_message_observer;
    EXPECT_EQ(
        main_host_sync_->AddObserver(
            invalid_quantization, invalid_observer.BindNewPipeAndPassRemote()),
        blink::mojom::ComputePressureStatus::kSecurityError);
    EXPECT_EQ("Invalid quantization", bad_message_observer.WaitForBadMessage());
  }

  FakeComputePressureObserver valid_observer;
  EXPECT_EQ(main_host_sync_->AddObserver(
                kQuantization, valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
}

TEST_F(ComputePressureManagerTest, AddObserver_InsecureOrigin) {
  {
    mojo::FakeMessageDispatchContext fake_dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::Remote<blink::mojom::ComputePressureHost> insecure_host;
    manager_->BindReceiver(kInsecureOrigin, main_frame_id_,
                           insecure_host.BindNewPipeAndPassReceiver());
    EXPECT_EQ("Compute Pressure access from an insecure origin",
              bad_message_observer.WaitForBadMessage());
  }

  FakeComputePressureObserver valid_observer;
  EXPECT_EQ(main_host_sync_->AddObserver(
                kQuantization, valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
}

TEST_F(ComputePressureManagerTest, AddObserver_NoProbe) {
  SetManager(ComputePressureManager::CreateForTesting(
      /*cpu_probe=*/nullptr, base::Milliseconds(1), base::Milliseconds(1)));

  FakeComputePressureObserver observer;
  EXPECT_EQ(main_host_sync_->AddObserver(kQuantization,
                                         observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kNotSupported);
}

TEST_F(ComputePressureManagerTest, AddObserver_NoFeature) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(blink::features::kComputePressure);

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  mojo::Remote<blink::mojom::ComputePressureHost> insecure_host;
  manager_->BindReceiver(kInsecureOrigin, main_frame_id_,
                         insecure_host.BindNewPipeAndPassReceiver());
  EXPECT_EQ("Compute Pressure not enabled",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
