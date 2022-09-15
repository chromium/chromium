// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_impl.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
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

  blink::mojom::PressureStatus BindObserver(
      mojo::PendingRemote<blink::mojom::PressureObserver> observer) {
    base::test::TestFuture<blink::mojom::PressureStatus> future;
    service_.BindObserver(std::move(observer), future.GetCallback());
    return future.Get();
  }

  blink::mojom::SetQuantizationStatus SetQuantization(
      const PressureQuantization& quantization) {
    base::test::TestFuture<blink::mojom::SetQuantizationStatus> future;
    service_.SetQuantization(quantization.Clone(), future.GetCallback());
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

TEST_F(PressureServiceImplTest, BindObserver) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time time = base::Time::Now() + kRateLimit;
  const PressureState state{0.42};
  pressure_manager_overrider_->UpdateClients(state, time);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.35});
}

TEST_F(PressureServiceImplTest, UpdateRateLimiting) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

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

TEST_F(PressureServiceImplTest, NoCallbackInvoked_SameBucket) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

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

TEST_F(PressureServiceImplTest, BindRateLimiting) {
  const base::Time before_add = base::Time::Now();

  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time after_add = base::Time::Now();

  ASSERT_LE(after_add - before_add, base::Milliseconds(500))
      << "test timings assume that BindObserver completes in at most 500ms";

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

TEST_F(PressureServiceImplTest, NewQuantization) {
  const PressureState state{0.42};

  // 0.42 would quantize as 0.4
  PressureQuantization quantization1{{0.8}};
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(quantization1),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time time1 = base::Time::Now() + kRateLimit;
  pressure_manager_overrider_->UpdateClients(state, time1);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.4});
  observer.updates().clear();

  // 0.42 would quantize as 0.6
  PressureQuantization quantization2{{0.2}};
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(quantization2),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time time2 = base::Time::Now() + kRateLimit;
  pressure_manager_overrider_->UpdateClients(state, time2);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.6});
  observer.updates().clear();

  // 0.42 would quantize as 0.25
  PressureQuantization quantization3{{0.5}};
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(quantization3),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time time3 = base::Time::Now() + kRateLimit;
  pressure_manager_overrider_->UpdateClients(state, time3);
  observer.WaitForUpdate();
  ASSERT_EQ(observer.updates().size(), 1u);
  EXPECT_EQ(observer.updates()[0], PressureState{0.25});
  observer.updates().clear();
}

TEST_F(PressureServiceImplTest, NoVisibility) {
  FakePressureObserver observer;
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

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

TEST_F(PressureServiceImplTest, InvalidQuantization) {
  FakePressureObserver observer;
  PressureQuantization invalid_quantization{{-1.0}};
  ASSERT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
            blink::mojom::PressureStatus::kOk);
  ASSERT_EQ(pressure_service_impl_sync_->SetQuantization(kQuantization),
            blink::mojom::SetQuantizationStatus::kChanged);

  const base::Time time = base::Time::Now();

  const PressureState state1{0.0};
  pressure_manager_overrider_->UpdateClients(state1, time + kRateLimit);
  observer.WaitForUpdate();

  {
    mojo::test::BadMessageObserver bad_message_observer;
    pressure_service_->SetQuantization(invalid_quantization.Clone(),
                                       base::DoNothing());
    EXPECT_EQ("Invalid quantization", bad_message_observer.WaitForBadMessage());
  }

  const PressureState state2{1.0};
  pressure_manager_overrider_->UpdateClients(state2, time + kRateLimit * 2);
  observer.WaitForUpdate();

  ASSERT_EQ(observer.updates().size(), 2u);
  EXPECT_THAT(observer.updates(), testing::Contains(PressureState{0.1}));
  EXPECT_THAT(observer.updates(), testing::Contains(PressureState{0.9}));
}

TEST_F(PressureServiceImplTest, BindObserver_NotSupported) {
  pressure_manager_overrider_->set_is_supported(false);

  FakePressureObserver observer;
  EXPECT_EQ(pressure_service_impl_sync_->BindObserver(
                observer.BindNewPipeAndPassRemote()),
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

// Allows callers to run a custom callback before running
// FakePressureManager::AddClient().
class InterceptingFakePressureManager : public device::FakePressureManager {
 public:
  explicit InterceptingFakePressureManager(
      base::OnceClosure interception_callback)
      : interception_callback_(std::move(interception_callback)) {}

  void AddClient(mojo::PendingRemote<device::mojom::PressureClient> client,
                 AddClientCallback callback) override {
    std::move(interception_callback_).Run();
    device::FakePressureManager::AddClient(std::move(client),
                                           std::move(callback));
  }

 private:
  base::OnceClosure interception_callback_;
};

// Test for https://crbug.com/1355662: destroying PressureServiceImplTest
// between calling PressureServiceImpl::BindObserver() and its |remote_|
// invoking the callback it receives does not crash.
TEST_F(PressureServiceImplTest, DestructionOrderWithOngoingCallback) {
  auto intercepting_fake_pressure_manager =
      std::make_unique<InterceptingFakePressureManager>(
          base::BindLambdaForTesting([&]() {
            // Delete the current WebContents and consequently trigger
            // PressureServiceImpl's destruction between calling
            // PressureServiceImpl::BindObserver() and its |remote_|
            // invoking the callback it receives.
            DeleteContents();
          }));
  pressure_manager_overrider_->set_fake_pressure_manager(
      std::move(intercepting_fake_pressure_manager));

  base::RunLoop run_loop;
  pressure_service_.set_disconnect_handler(run_loop.QuitClosure());
  FakePressureObserver observer;
  pressure_service_->BindObserver(
      observer.BindNewPipeAndPassRemote(),
      base::BindOnce([](blink::mojom::PressureStatus) {
        ADD_FAILURE() << "Reached BindObserver callback unexpectedly";
      }));
  run_loop.Run();
}

}  // namespace content
