// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_host.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/compute_pressure/compute_pressure_test_support.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

class ComputePressureHostTest : public RenderViewHostImplTestHarness {
 public:
  ComputePressureHostTest() = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    // base::Unretained use is safe here because the callback will only be
    // called while the ComputePressureHost is alive, and this instance owns the
    // ComputePressureHost.
    SetHostImpl(std::make_unique<ComputePressureHost>(
        kTestOrigin, /*is_supported=*/true, base::Seconds(1),
        base::BindRepeating(&ComputePressureHostTest::DidHostConnectionsChange,
                            base::Unretained(this))));
  }

  void SetHostImpl(std::unique_ptr<ComputePressureHost> new_host) {
    host_impl_ = std::move(new_host);
    int process_id = main_rfh()->GetProcess()->GetID();
    int routing_id = main_rfh()->GetRoutingID();
    main_frame_id_ = GlobalRenderFrameHostId(process_id, routing_id);

    host_.reset();
    host_impl_->BindReceiver(main_frame_id_,
                             host_.BindNewPipeAndPassReceiver());
    host_sync_ = std::make_unique<ComputePressureHostSync>(host_.get());
  }

  void DidHostConnectionsChange(ComputePressureHost* host) {
    DCHECK_EQ(host_impl_.get(), host);
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kThirdPartyOrigin =
      url::Origin::Create(GURL("https://other.com"));

  // Quantization scheme used in most tests.
  const blink::mojom::ComputePressureQuantization kQuantization = {
      {0.2, 0.5, 0.8},
      {0.5}};

  GlobalRenderFrameHostId main_frame_id_;
  // This member is a std::unique_ptr instead of a plain ComputePressureHost
  // so it can be replaced inside tests.
  std::unique_ptr<ComputePressureHost> host_impl_;
  mojo::Remote<blink::mojom::ComputePressureHost> host_;
  std::unique_ptr<ComputePressureHostSync> host_sync_;
};

TEST_F(ComputePressureHostTest, OneObserver) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              now + base::Seconds(1));
  observer.WaitForUpdate();
  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_EQ(observer.updates()[0],
            blink::mojom::ComputePressureState(0.35, 0.75));
}

TEST_F(ComputePressureHostTest, OneObserver_UpdateRateLimiting) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time after_add = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              after_add + base::Milliseconds(1000));
  observer.WaitForUpdate();
  observer.updates().clear();

  // The first update should be blocked due to rate-limiting.
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              after_add + base::Milliseconds(1500));
  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              after_add + base::Milliseconds(2100));
  observer.WaitForUpdate();

  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_EQ(observer.updates()[0],
            blink::mojom::ComputePressureState(0.1, 0.25));
}

TEST_F(ComputePressureHostTest, OneObserver_NoCallbackInvoke) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              now + base::Seconds(1));
  observer.WaitForUpdate();
  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_EQ(observer.updates()[0],
            blink::mojom::ComputePressureState(0.35, 0.75));

  // The first update should be discarded due to same bucket
  host_impl_->UpdateObservers({.cpu_utilization = 0.37, .cpu_speed = 0.70},
                              now + base::Seconds(2));
  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.42},
                              now + base::Seconds(3));
  observer.WaitForUpdate();
  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(2u)));
  EXPECT_EQ(observer.updates()[1],
            blink::mojom::ComputePressureState(0.35, 0.25));
}

TEST_F(ComputePressureHostTest, OneObserver_AddRateLimiting) {
  base::Time before_add = base::Time::Now();

  FakeComputePressureObserver observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time after_add = base::Time::Now();

  ASSERT_LE(after_add - before_add, base::Milliseconds(500))
      << "test timings assume that AddObserver completes in at most 500ms";

  // The first update should be blocked due to rate-limiting.
  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              before_add + base::Milliseconds(700));
  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              before_add + base::Milliseconds(1600));
  observer.WaitForUpdate();

  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_EQ(observer.updates()[0],
            blink::mojom::ComputePressureState(0.1, 0.25));
}

TEST_F(ComputePressureHostTest, ThreeObservers) {
  FakeComputePressureObserver observer1;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer1.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer2;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer2.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer3;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer3.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              now + base::Seconds(1));
  FakeComputePressureObserver::WaitForUpdates(
      {&observer1, &observer2, &observer3});

  EXPECT_THAT(observer1.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_THAT(
      observer1.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
  EXPECT_THAT(observer2.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_THAT(
      observer2.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
  EXPECT_THAT(observer3.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.35, 0.75)));
}

TEST_F(ComputePressureHostTest, AddObserver_NewQuantization) {
  // 0.42, 0.84 would quantize as 0.4, 0.6
  blink::mojom::ComputePressureQuantization quantization1 = {{0.8}, {0.2}};
  FakeComputePressureObserver observer1;
  ASSERT_EQ(host_sync_->AddObserver(quantization1,
                                    observer1.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  // 0.42, 0.84 would quantize as 0.6, 0.4
  blink::mojom::ComputePressureQuantization quantization2 = {{0.2}, {0.8}};
  FakeComputePressureObserver observer2;
  ASSERT_EQ(host_sync_->AddObserver(quantization2,
                                    observer2.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  // 0.42, 0.84 will quantize as 0.25, 0.75
  blink::mojom::ComputePressureQuantization quantization3 = {{0.5}, {0.5}};
  FakeComputePressureObserver observer3;
  ASSERT_EQ(host_sync_->AddObserver(quantization3,
                                    observer3.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.42, .cpu_speed = 0.84},
                              now + base::Milliseconds(1000));
  observer3.WaitForUpdate();
  host_impl_->UpdateObservers({.cpu_utilization = 0.84, .cpu_speed = 0.42},
                              now + base::Milliseconds(2000));
  observer3.WaitForUpdate();

  EXPECT_THAT(observer3.updates(), testing::SizeIs(testing::Eq(2u)));
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.25, 0.75)));
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(blink::mojom::ComputePressureState(0.75, 0.25)));

  EXPECT_THAT(observer1.updates(), testing::SizeIs(testing::Eq(0u)));
  EXPECT_THAT(observer2.updates(), testing::SizeIs(testing::Eq(0u)));
}

TEST_F(ComputePressureHostTest, AddObserver_ThirdPartyFrame) {
  ComputePressureHost host_3p_impl(
      kThirdPartyOrigin, /*is_supported=*/true, base::Seconds(1),
      /*did_connections_change*/ base::DoNothing());
  mojo::Remote<blink::mojom::ComputePressureHost> host_3p;
  host_3p_impl.BindReceiver(main_frame_id_,
                            host_3p.BindNewPipeAndPassReceiver());
  auto host_3p_sync = std::make_unique<ComputePressureHostSync>(host_3p.get());
  FakeComputePressureObserver first_party_observer;
  ASSERT_EQ(host_sync_->AddObserver(
                kQuantization, first_party_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver third_party_observer;
  EXPECT_EQ(host_3p_sync->AddObserver(
                kQuantization, third_party_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kSecurityError);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              now + base::Milliseconds(1000));
  first_party_observer.WaitForUpdate();
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              now + base::Milliseconds(2000));
  first_party_observer.WaitForUpdate();

  EXPECT_THAT(first_party_observer.updates(), testing::SizeIs(testing::Eq(2u)));
  EXPECT_THAT(first_party_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.1, 0.25)));
  EXPECT_THAT(first_party_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.9, 0.75)));

  EXPECT_THAT(third_party_observer.updates(), testing::SizeIs(testing::Eq(0u)));
}

TEST_F(ComputePressureHostTest, AddObserver_NoVisibility) {
  test_rvh()->SimulateWasHidden();

  FakeComputePressureObserver observer;
  EXPECT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  base::Time now = base::Time::Now();

  // The first two updates should be blocked due to invisibility.
  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              now + base::Milliseconds(1000));
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              now + base::Milliseconds(2000));

  test_rvh()->SimulateWasShown();

  // The third update should be dispatched. It should not be rate-limited by the
  // time proximity to the second update, because the second update is not
  // dispatched.
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              now + base::Milliseconds(2100));
  observer.WaitForUpdate();

  ASSERT_THAT(observer.updates(), testing::SizeIs(testing::Eq(1u)));
  EXPECT_EQ(observer.updates()[0],
            blink::mojom::ComputePressureState(0.9, 0.75));
}

TEST_F(ComputePressureHostTest, AddObserver_InvalidFrame) {
  GlobalRenderFrameHostId invalid_routing_id(
      main_rfh()->GetProcess()->GetID() + 1, main_rfh()->GetRoutingID() + 1);
  mojo::Remote<blink::mojom::ComputePressureHost> host_invalid;
  host_impl_->BindReceiver(invalid_routing_id,
                           host_invalid.BindNewPipeAndPassReceiver());
  auto host_invalid_sync =
      std::make_unique<ComputePressureHostSync>(host_invalid.get());
  FakeComputePressureObserver valid_observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver invalid_observer;
  EXPECT_EQ(host_invalid_sync->AddObserver(
                kQuantization, invalid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kSecurityError);

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              now + base::Milliseconds(1000));
  valid_observer.WaitForUpdate();
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              now + base::Milliseconds(2000));
  valid_observer.WaitForUpdate();

  EXPECT_THAT(valid_observer.updates(), testing::SizeIs(testing::Eq(2u)));
  EXPECT_THAT(valid_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.1, 0.25)));
  EXPECT_THAT(valid_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.9, 0.75)));

  EXPECT_THAT(invalid_observer.updates(), testing::SizeIs(testing::Eq(0u)));
}

TEST_F(ComputePressureHostTest, AddObserver_InvalidQuantization) {
  FakeComputePressureObserver valid_observer;
  ASSERT_EQ(host_sync_->AddObserver(kQuantization,
                                    valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  FakeComputePressureObserver invalid_observer;
  blink::mojom::ComputePressureQuantization invalid_quantization(
      std::vector<double>{-1.0}, std::vector<double>{0.5});

  {
    mojo::test::BadMessageObserver bad_message_observer;
    EXPECT_EQ(
        host_sync_->AddObserver(invalid_quantization,
                                invalid_observer.BindNewPipeAndPassRemote()),
        blink::mojom::ComputePressureStatus::kSecurityError);
    EXPECT_EQ("Invalid quantization", bad_message_observer.WaitForBadMessage());
  }

  base::Time now = base::Time::Now();

  host_impl_->UpdateObservers({.cpu_utilization = 0.0, .cpu_speed = 0.0},
                              now + base::Milliseconds(1000));
  valid_observer.WaitForUpdate();
  host_impl_->UpdateObservers({.cpu_utilization = 1.0, .cpu_speed = 1.0},
                              now + base::Milliseconds(2000));
  valid_observer.WaitForUpdate();

  EXPECT_THAT(valid_observer.updates(), testing::SizeIs(testing::Eq(2u)));
  EXPECT_THAT(valid_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.1, 0.25)));
  EXPECT_THAT(valid_observer.updates(),
              testing::Contains(blink::mojom::ComputePressureState(0.9, 0.75)));

  EXPECT_THAT(invalid_observer.updates(), testing::SizeIs(testing::Eq(0u)));
}

TEST_F(ComputePressureHostTest, AddObserver_NotSupported) {
  SetHostImpl(std::make_unique<ComputePressureHost>(
      kTestOrigin, /*is_supported=*/false, base::Seconds(1),
      /*did_connections_change=*/base::DoNothing()));

  FakeComputePressureObserver observer;
  EXPECT_EQ(host_sync_->AddObserver(kQuantization,
                                    observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kNotSupported);
}

}  // namespace content
