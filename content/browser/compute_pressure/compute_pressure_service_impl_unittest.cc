// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_service_impl.h"

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/device/public/cpp/test/scoped_compute_pressure_manager_overrider.h"
#include "services/device/public/mojom/compute_pressure_manager.mojom.h"
#include "services/device/public/mojom/compute_pressure_state.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr base::TimeDelta kRateLimit =
    ComputePressureServiceImpl::kDefaultVisibleObserverRateLimit;

// Synchronous proxy to a blink::mojom::ComputePressureService.
class ComputePressureServiceImplSync {
 public:
  explicit ComputePressureServiceImplSync(
      blink::mojom::ComputePressureService* service)
      : service_(*service) {
    DCHECK(service);
  }
  ~ComputePressureServiceImplSync() = default;

  ComputePressureServiceImplSync(const ComputePressureServiceImplSync&) =
      delete;
  ComputePressureServiceImplSync& operator=(
      const ComputePressureServiceImplSync&) = delete;

  blink::mojom::ComputePressureStatus AddObserver(
      const blink::mojom::ComputePressureQuantization& quantization,
      mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer) {
    base::test::TestFuture<blink::mojom::ComputePressureStatus> future;
    service_.AddObserver(std::move(observer), quantization.Clone(),
                         future.GetCallback());
    return future.Get();
  }

 private:
  // The reference is immutable, so accessing it is thread-safe. The referenced
  // blink::mojom::ComputePressureService implementation is called
  // synchronously, so it's acceptable to rely on its own thread-safety checks.
  blink::mojom::ComputePressureService& service_;
};

// Test double for ComputePressureObserver that records all updates.
class FakeComputePressureObserver
    : public blink::mojom::ComputePressureObserver {
 public:
  FakeComputePressureObserver() : receiver_(this) {}
  ~FakeComputePressureObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  FakeComputePressureObserver(const FakeComputePressureObserver&) = delete;
  FakeComputePressureObserver& operator=(const FakeComputePressureObserver&) =
      delete;

  // blink::mojom::ComputePressureObserver implementation.
  void OnUpdate(device::mojom::ComputePressureStatePtr state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    updates_.push_back(*state);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  std::vector<device::mojom::ComputePressureState>& updates() {
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
      std::initializer_list<FakeComputePressureObserver*> observers) {
    base::RunLoop run_loop;
    base::RepeatingClosure update_barrier =
        base::BarrierClosure(observers.size(), run_loop.QuitClosure());
    for (FakeComputePressureObserver* observer : observers)
      observer->SetNextUpdateCallback(update_barrier);
    run_loop.Run();
  }

  mojo::PendingRemote<blink::mojom::ComputePressureObserver>
  BindNewPipeAndPassRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<device::mojom::ComputePressureState> updates_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<blink::mojom::ComputePressureObserver> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace

class ComputePressureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  ComputePressureServiceImplTest() = default;
  ~ComputePressureServiceImplTest() override = default;

  ComputePressureServiceImplTest(const ComputePressureServiceImplTest&) =
      delete;
  ComputePressureServiceImplTest& operator=(
      const ComputePressureServiceImplTest&) = delete;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kTestUrl);

    SetComputePressureServiceImpl();
  }

  void TearDown() override {
    pressure_service_impl_sync_.reset();
    pressure_manager_overrider_.reset();
    task_environment()->RunUntilIdle();

    RenderViewHostImplTestHarness::TearDown();
  }

  void SetComputePressureServiceImpl() {
    pressure_manager_overrider_ =
        std::make_unique<device::ScopedComputePressureManagerOverrider>();
    pressure_service_.reset();
    ComputePressureServiceImpl::Create(
        contents()->GetPrimaryMainFrame(),
        pressure_service_.BindNewPipeAndPassReceiver());
    pressure_service_impl_sync_ =
        std::make_unique<ComputePressureServiceImplSync>(
            pressure_service_.get());
  }

 protected:
  const GURL kTestUrl{"https://example.com/compute_pressure.html"};
  const GURL kInsecureUrl{"http://example.com/compute_pressure.html"};
  // Quantization scheme used in most tests.
  const blink::mojom::ComputePressureQuantization kQuantization = {
      {0.2, 0.5, 0.8},
      {0.5}};

  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::Remote<blink::mojom::ComputePressureService> pressure_service_;
  std::unique_ptr<ComputePressureServiceImplSync> pressure_service_impl_sync_;
  std::unique_ptr<device::ScopedComputePressureManagerOverrider>
      pressure_manager_overrider_;
};

TEST_F(ComputePressureServiceImplTest, OneObserver) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const device::mojom::ComputePressureState state{0.42, 0.84};
  pressure_manager_overrider_->UpdateClients(state, time);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0],
            device::mojom::ComputePressureState(0.35, 0.75));
}

TEST_F(ComputePressureServiceImplTest, OneObserver_UpdateRateLimiting) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now();
  const device::mojom::ComputePressureState state1{0.42, 0.84};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  observer.WaitForUpdate();
  observer.updates().clear();

  // The first update should be blocked due to rate-limiting.
  const device::mojom::ComputePressureState state2{1.0, 1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 1.5);
  const device::mojom::ComputePressureState state3{0.0, 0.0};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0],
            device::mojom::ComputePressureState(0.1, 0.25));
}

TEST_F(ComputePressureServiceImplTest, OneObserver_NoCallbackInvoked) {
  FakeComputePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const device::mojom::ComputePressureState state1{0.42, 0.84};
  pressure_manager_overrider_->UpdateClients(state1, time);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0],
            device::mojom::ComputePressureState(0.35, 0.75));

  // The first update should be discarded due to same bucket
  const device::mojom::ComputePressureState state2{0.37, 0.70};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit);
  const device::mojom::ComputePressureState state3{0.42, 0.42};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 2u);
  EXPECT_EQ(observer.updates()[1],
            device::mojom::ComputePressureState(0.35, 0.25));
}

TEST_F(ComputePressureServiceImplTest, OneObserver_AddRateLimiting) {
  const base::Time before_add = base::Time::Now();

  FakeComputePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time after_add = base::Time::Now();

  ASSERT_LE(after_add - before_add, base::Milliseconds(500))
      << "test timings assume that AddObserver completes in at most 500ms";

  // The first update should be blocked due to rate-limiting.
  const device::mojom::ComputePressureState state1{0.42, 0.84};
  const base::Time time1 = before_add + base::Milliseconds(700);
  pressure_manager_overrider_->UpdateClients(state1, time1);
  const device::mojom::ComputePressureState state2{0.0, 0.0};
  const base::Time time2 = before_add + base::Milliseconds(1600);
  pressure_manager_overrider_->UpdateClients(state2, time2);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0],
            device::mojom::ComputePressureState(0.1, 0.25));
}

TEST_F(ComputePressureServiceImplTest, ThreeObservers) {
  FakeComputePressureObserver observer1;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer1.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer2;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer2.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);
  FakeComputePressureObserver observer3;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer3.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const device::mojom::ComputePressureState state{0.42, 0.84};
  pressure_manager_overrider_->UpdateClients(state, time);
  FakeComputePressureObserver::WaitForUpdates(
      {&observer1, &observer2, &observer3});

  ASSERT_EQ(observer1.updates().size(), 1u);
  EXPECT_THAT(
      observer1.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.35, 0.75)));
  ASSERT_EQ(observer2.updates().size(), 1u);
  EXPECT_THAT(
      observer2.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.35, 0.75)));
  ASSERT_EQ(observer3.updates().size(), 1u);
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.35, 0.75)));
}

TEST_F(ComputePressureServiceImplTest, AddObserver_NewQuantization) {
  // 0.42, 0.84 would quantize as 0.4, 0.6
  blink::mojom::ComputePressureQuantization quantization1 = {{0.8}, {0.2}};
  FakeComputePressureObserver observer1;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization1, observer1.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  // 0.42, 0.84 would quantize as 0.6, 0.4
  blink::mojom::ComputePressureQuantization quantization2 = {{0.2}, {0.8}};
  FakeComputePressureObserver observer2;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization2, observer2.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  // 0.42, 0.84 will quantize as 0.25, 0.75
  blink::mojom::ComputePressureQuantization quantization3 = {{0.5}, {0.5}};
  FakeComputePressureObserver observer3;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                quantization3, observer3.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now() + kRateLimit;
  const device::mojom::ComputePressureState state1{0.42, 0.84};
  pressure_manager_overrider_->UpdateClients(state1, time);
  observer3.WaitForUpdate();
  const device::mojom::ComputePressureState state2{0.84, 0.42};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit);
  observer3.WaitForUpdate();

  ASSERT_EQ(observer3.updates().size(), 2u);
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.25, 0.75)));
  EXPECT_THAT(
      observer3.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.75, 0.25)));

  ASSERT_EQ(observer1.updates().size(), 0u);
  ASSERT_EQ(observer2.updates().size(), 0u);
}

TEST_F(ComputePressureServiceImplTest, AddObserver_NoVisibility) {
  FakeComputePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  const base::Time time = base::Time::Now();

  test_rvh()->SimulateWasHidden();

  // The first two updates should be blocked due to invisibility.
  const device::mojom::ComputePressureState state1{0.0, 0.0};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  const device::mojom::ComputePressureState state2{1.0, 1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 2);
  task_environment()->RunUntilIdle();

  test_rvh()->SimulateWasShown();

  // The third update should be dispatched. It should not be rate-limited by the
  // time proximity to the second update, because the second update is not
  // dispatched.
  const device::mojom::ComputePressureState state3{1.0, 1.0};
  pressure_manager_overrider_->UpdateClients(state3, time + kRateLimit * 2.5);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0],
            device::mojom::ComputePressureState(0.9, 0.75));
}

TEST_F(ComputePressureServiceImplTest, AddObserver_InvalidQuantization) {
  FakeComputePressureObserver valid_observer;
  ASSERT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, valid_observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kOk);

  FakeComputePressureObserver invalid_observer;
  blink::mojom::ComputePressureQuantization invalid_quantization(
      std::vector<double>{-1.0}, std::vector<double>{0.5});

  {
    mojo::test::BadMessageObserver bad_message_observer;
    EXPECT_EQ(
        pressure_service_impl_sync_->AddObserver(
            invalid_quantization, invalid_observer.BindNewPipeAndPassRemote()),
        blink::mojom::ComputePressureStatus::kSecurityError);
    EXPECT_EQ("Invalid quantization", bad_message_observer.WaitForBadMessage());
  }

  const base::Time time = base::Time::Now();

  const device::mojom::ComputePressureState state1{0.0, 0.0};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  valid_observer.WaitForUpdate();
  const device::mojom::ComputePressureState state2{1.0, 1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 2);
  valid_observer.WaitForUpdate();

  ASSERT_EQ(valid_observer.updates().size(), 2u);
  EXPECT_THAT(
      valid_observer.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.1, 0.25)));
  EXPECT_THAT(
      valid_observer.updates(),
      testing::Contains(device::mojom::ComputePressureState(0.9, 0.75)));

  ASSERT_EQ(invalid_observer.updates().size(), 0u);
}

TEST_F(ComputePressureServiceImplTest, AddObserver_NotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakeComputePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->AddObserver(
                kQuantization, observer.BindNewPipeAndPassRemote()),
            blink::mojom::ComputePressureStatus::kNotSupported);
}

TEST_F(ComputePressureServiceImplTest, InsecureOrigin) {
  NavigateAndCommit(kInsecureUrl);

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  SetComputePressureServiceImpl();
  EXPECT_EQ("Compute Pressure access from an insecure origin",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
