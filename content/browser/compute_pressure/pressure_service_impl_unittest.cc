// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_impl.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/scoped_pressure_manager_overrider.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"
#include "url/gurl.h"

namespace content {

using blink::mojom::PressureQuantization;
using device::mojom::PressureState;

namespace {

constexpr base::TimeDelta kRateLimit =
    PressureServiceImpl::kDefaultVisibleObserverRateLimit;

// Synchronous proxy to a blink::mojom::PressureService.
class PressureServiceImplSync {
 public:
  explicit PressureServiceImplSync(blink::mojom::PressureService* service)
      : service_(*service) {
    DCHECK(service);
  }
  ~PressureServiceImplSync() = default;

  PressureServiceImplSync(const PressureServiceImplSync&) = delete;
  PressureServiceImplSync& operator=(const PressureServiceImplSync&) = delete;

  blink::mojom::PressureStatus AddObserver(
      const PressureQuantization& quantization,
      mojo::PendingRemote<blink::mojom::PressureObserver> observer) {
    base::test::TestFuture<blink::mojom::PressureStatus> future;
    service_.AddObserver(std::move(observer), quantization.Clone(),
                         future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // blink::mojom::PressureService implementation is called synchronously,
  // so it's acceptable to rely on its own thread-safety checks.
  blink::mojom::PressureService& service_;
};

// Test double for PressureObserver that records all updates.
class FakePressureObserver : public blink::mojom::PressureObserver {
 public:
  FakePressureObserver() : receiver_(this) {}
  ~FakePressureObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakePressureObserver(const FakePressureObserver&) = delete;
  FakePressureObserver& operator=(const FakePressureObserver&) = delete;

  // blink::mojom::PressureObserver implementation.
  void OnUpdate(device::mojom::PressureStatePtr state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    updates_.push_back(*state);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  std::vector<PressureState>& updates() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return updates_;
  }

  void SetNextUpdateCallback(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!update_callback_) << " already called before update received";

    update_callback_ = std::move(callback);
  }

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  static void WaitForUpdates(
      std::initializer_list<FakePressureObserver*> observers) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(observers.size(), run_loop.QuitClosure());
    for (FakePressureObserver* observer : observers)
      observer->SetNextUpdateCallback(update_barrier);
    run_loop.Run();
  }

  mojo::PendingRemote<blink::mojom::PressureObserver>
  BindNewPipeAndPassRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<PressureState> updates_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<blink::mojom::PressureObserver> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

class PressureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PressureServiceImplTest() = default;
  ~PressureServiceImplTest() override = default;

  PressureServiceImplTest(const PressureServiceImplTest&) = delete;
  PressureServiceImplTest& operator=(const PressureServiceImplTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetPressureServiceImpl();
  }

  void TearDown() override {
    pressure_service_impl_sync_.reset();
    pressure_manager_overrider_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetPressureServiceImpl() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedPressureManagerOverrider>();
    pressure_service_.reset();
    PressureServiceImpl::Create(contents()->GetPrimaryMainFrame(),
                                pressure_service_.BindNewPipeAndPassReceiver());
    pressure_service_impl_sync_ =
        std::make_unique<PressureServiceImplSync>(pressure_service_.get());
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kInsecureUrl{"http://example.com/compute_pressure.html"};
  // Quantization scheme used in most tests.
  const PressureQuantization kQuantization{{0.2, 0.5, 0.8}};

  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::Remote<blink::mojom::PressureService> pressure_service_;
  std::unique_ptr<PressureServiceImplSync> pressure_service_impl_sync_;
  std::unique_ptr<device::ScopedPressureManagerOverrider>
      pressure_manager_overrider_;
};

TEST_F(PressureServiceImplTest, OneObserver) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const PressureState state{0.42};
  pressure_manager_overrider_->UpdateClients(state, time);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.35});
}

TEST_F(PressureServiceImplTest, OneObserver_UpdateRateLimiting) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now();
  const PressureState state1{0.42};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  observer.WaitForUpdate();
  observer.updates().clear();

  // The first update should be blocked due to rate-limiting.
  const PressureState state2{1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 1.5);
  const PressureState state3{0.0};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.1});
}

TEST_F(PressureServiceImplTest, OneObserver_NoCallbackInvoked) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const PressureState state1{0.42};
  pressure_manager_overrider_->UpdateClients(state1, time);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.35});

  // The first update should be discarded due to same bucket
  const PressureState state2{0.37};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit);
  const PressureState state3{0.52};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 2u);
  EXPECT_EQ(observer.updates()[1], PressureState{0.65});
}

TEST_F(PressureServiceImplTest, OneObserver_AddRateLimiting) {
  const base::Time before_add = base::Time::Now();

  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time after_add = base::Time::Now();

  ASSERT_LE(after_add - before_add, base::Milliseconds(500))
      << "test timings assume that AddObserver completes in at most 500ms";

  // The first update should be blocked due to rate-limiting.
  const PressureState state1{0.42};
  const base::Time time1 = before_add + base::Milliseconds(700);
  pressure_manager_overrider_->UpdateClients(state1, time1);
  const PressureState state2{0.0};
  const base::Time time2 = before_add + base::Milliseconds(1600);
  pressure_manager_overrider_->UpdateClients(state2, time2);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.1});
}

TEST_F(PressureServiceImplTest, ThreeObservers) {
  FakePressureObserver observer1;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer1.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  FakePressureObserver observer2;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer2.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  FakePressureObserver observer3;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer3.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const PressureState state{0.42};
  pressure_manager_overrider_->UpdateClients(state, time);
  FakePressureObserver::WaitForUpdates({&observer1, &observer2, &observer3});

  ASSERT_EQ(observer1.updates().size(), 1u);
  EXPECT_THAT(observer1.updates(), testing::Contains(PressureState{0.35}));
  ASSERT_EQ(observer2.updates().size(), 1u);
  EXPECT_THAT(observer2.updates(), testing::Contains(PressureState{0.35}));
  ASSERT_EQ(observer3.updates().size(), 1u);
  EXPECT_THAT(observer3.updates(), testing::Contains(PressureState{0.35}));
}

TEST_F(PressureServiceImplTest, AddObserver_NewQuantization) {
  // 0.42 would quantize as 0.4
  PressureQuantization quantization1{{0.8}};
  FakePressureObserver observer1;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization1, observer1.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  // 0.42 would quantize as 0.6
  PressureQuantization quantization2{{0.2}};
  FakePressureObserver observer2;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization2, observer2.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  // 0.42 will quantize as 0.25
  PressureQuantization quantization3{{0.5}};
  FakePressureObserver observer3;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization3, observer3.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const PressureState state1{0.42};
  pressure_manager_overrider_->UpdateClients(state1, time);
  observer3.WaitForUpdate();
  const PressureState state2{0.84};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit);
  observer3.WaitForUpdate();

  ASSERT_EQ(observer3.updates().size(), 2u);
  EXPECT_THAT(observer3.updates(), testing::Contains(PressureState{0.25}));
  EXPECT_THAT(observer3.updates(), testing::Contains(PressureState{0.75}));

  ASSERT_EQ(observer1.updates().size(), 0u);
  ASSERT_EQ(observer2.updates().size(), 0u);
}

TEST_F(PressureServiceImplTest, AddObserver_NoVisibility) {
  FakePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  const base::Time time = base::Time::Now();

  test_rvh()->SimulateWasHidden();

  // The first two updates should be blocked due to invisibility.
  const PressureState state1{0.0};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  const PressureState state2{1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 2);
  task_environment()->RunUntilIdle();

  test_rvh()->SimulateWasShown();

  // The third update should be dispatched. It should not be rate-limited by the
  // time proximity to the second update, because the second update is not
  // dispatched.
  const PressureState state3{1.0};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2.5);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.9});
}

TEST_F(PressureServiceImplTest, AddObserver_InvalidQuantization) {
  FakePressureObserver valid_observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);

  FakePressureObserver invalid_observer;
  PressureQuantization invalid_quantization{{-1.0}};

  {
    mojo::test::BadMessageObserver bad_message_observer;
    EXPECT_EQ(
        pressure_service_impl_sync_->AddObserver(
            invalid_quantization, invalid_observer.BindNewPipeAndPassRemote()),
        blink::mojom::PressureStatus::kSecurityError);
    EXPECT_EQ("Invalid quantization", bad_message_observer.WaitForBadMessage());
  }

  const base::Time time = base::Time::Now();

  const PressureState state1{0.0};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  valid_observer.WaitForUpdate();
  const PressureState state2{1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 2);
  valid_observer.WaitForUpdate();

  ASSERT_EQ(valid_observer.updates().size(), 2u);
  EXPECT_THAT(valid_observer.updates(), testing::Contains(PressureState{0.1}));
  EXPECT_THAT(valid_observer.updates(), testing::Contains(PressureState{0.9}));

  ASSERT_EQ(invalid_observer.updates().size(), 0u);
}

TEST_F(PressureServiceImplTest, AddObserver_NotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kNotSupported);
}

TEST_F(PressureServiceImplTest, InsecureOrigin) {
  NavigateAndCommit(kInsecureUrl);

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetPressureServiceImpl();
  EXPECT_EQ("Compute Pressure access from an insecure origin",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
