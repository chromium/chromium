// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

namespace device {

namespace {

class TestWakeLockObserver : public mojom::WakeLockObserver {
 public:
  TestWakeLockObserver() {
    wake_lock_events_.emplace(mojom::WakeLockType::kPreventAppSuspension,
                              EventCount());
    wake_lock_events_.emplace(mojom::WakeLockType::kPreventDisplaySleep,
                              EventCount());
    wake_lock_events_.emplace(
        mojom::WakeLockType::kPreventDisplaySleepAllowDimming, EventCount());
  }

  TestWakeLockObserver(const TestWakeLockObserver&) = delete;
  TestWakeLockObserver& operator=(const TestWakeLockObserver&) = delete;

  ~TestWakeLockObserver() override = default;

  void AddReceiver(mojo::PendingReceiver<mojom::WakeLockObserver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::WakeLockObserver overrides.
  void OnWakeLockDeactivated(mojom::WakeLockType type) override {
    wake_lock_events_[type].on_deactivation_count++;
  }

  // Returns the number of calls to |OnWakeLockDeactivated|.
  int64_t GetOnDeactivationCount(mojom::WakeLockType type) {
    DCHECK(wake_lock_events_.find(type) != wake_lock_events_.end());
    return wake_lock_events_[type].on_deactivation_count;
  }

 private:
  struct EventCount {
    int64_t on_activation_count = 0;
    int64_t on_deactivation_count = 0;
  };

  mojo::ReceiverSet<mojom::WakeLockObserver> receivers_;

  std::map<mojom::WakeLockType, EventCount> wake_lock_events_;
};

class WakeLockTest : public DeviceServiceTestBase {
 public:
  WakeLockTest() = default;

  WakeLockTest(const WakeLockTest&) = delete;
  WakeLockTest& operator=(const WakeLockTest&) = delete;

  ~WakeLockTest() override = default;

 protected:
  void SetUp() override {
    DeviceServiceTestBase::SetUp();
    device_service()->BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());

    wake_lock_provider_->GetWakeLockWithoutContext(
        mojom::WakeLockType::kPreventAppSuspension,
        mojom::WakeLockReason::kOther, "WakeLockTest",
        wake_lock_.BindNewPipeAndPassReceiver());
  }

  void OnChangeType(base::OnceClosure quit_closure, bool result) {
    result_ = result;
    std::move(quit_closure).Run();
  }

  void OnHasWakeLock(base::OnceClosure quit_closure, bool has_wakelock) {
    has_wakelock_ = has_wakelock;
    std::move(quit_closure).Run();
  }

  bool ChangeType(mojom::WakeLockType type) {
    result_ = false;

    base::RunLoop run_loop;
    wake_lock_->ChangeType(
        type, base::BindOnce(&WakeLockTest::OnChangeType,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    return result_;
  }

  bool HasWakeLock() {
    has_wakelock_ = false;

    base::RunLoop run_loop;
    wake_lock_->HasWakeLockForTests(base::BindOnce(&WakeLockTest::OnHasWakeLock,
                                                   base::Unretained(this),
                                                   run_loop.QuitClosure()));
    run_loop.Run();

    return has_wakelock_;
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(mojom::WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_->GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

  bool has_wakelock_;
  bool result_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate LacrosService for WakeLock support.
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider_;
  mojo::Remote<mojom::WakeLock> wake_lock_;
};

// Request a wake lock, then cancel.
TEST_F(WakeLockTest, RequestThenCancel) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// Cancel a wake lock first, which should have no effect.
TEST_F(WakeLockTest, CancelThenRequest) {
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// Send multiple requests, which should be coalesced as one request.
TEST_F(WakeLockTest, MultipleRequests) {
  EXPECT_FALSE(HasWakeLock());

  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// Test Change Type. ChangeType() has no effect when wake lock is shared by
// multiple clients. Has no effect on Android either.
TEST_F(WakeLockTest, ChangeType) {
  EXPECT_FALSE(HasWakeLock());
#if !BUILDFLAG(IS_ANDROID)
  // Call ChangeType() on a wake lock that is in inactive status.
  EXPECT_TRUE(ChangeType(device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_TRUE(ChangeType(device::mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_TRUE(ChangeType(
      device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming));
  // Still inactive.
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  // At this point the wake lock is of type |kPreventDisplaySleepAllowDimming|.
  // Check for activation count of that type.
  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  // Call ChangeType() on a wake lock that is in active status.
  // No effect when the type is the same.
  EXPECT_TRUE(ChangeType(device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  EXPECT_TRUE(ChangeType(device::mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  EXPECT_TRUE(ChangeType(
      device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  // Still active.
  EXPECT_TRUE(HasWakeLock());

  // Send multiple requests, should be coalesced as usual.
  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));

  mojo::Remote<mojom::WakeLock> wake_lock_1;
  wake_lock_->AddClient(wake_lock_1.BindNewPipeAndPassReceiver());
  // Not allowed to change type when shared by multiple clients.
  EXPECT_FALSE(ChangeType(device::mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->CancelWakeLock();
  wake_lock_1->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));
#else  // BUILDFLAG(IS_ANDROID):
  EXPECT_FALSE(ChangeType(device::mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_FALSE(ChangeType(device::mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_FALSE(ChangeType(
      device::mojom::WakeLockType::kPreventDisplaySleepAllowDimming));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(0, GetActiveWakeLocks(
                   mojom::WakeLockType::kPreventDisplaySleepAllowDimming));
#endif
}

// WakeLockProvider connection broken doesn't affect WakeLock.
TEST_F(WakeLockTest, OnWakeLockProviderConnectionError) {
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  int32_t count =
      GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension);
  EXPECT_EQ(1, count);

  // Reset wake lock provider and check if the wake lock is still valid.
  wake_lock_provider_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWakeLock());

  // Instantiate wake lock provider and check if the wake lock count remains the
  // same as before since the provider implementation is a singleton.
  device_service()->BindWakeLockProvider(
      wake_lock_provider_.BindNewPipeAndPassReceiver());
  EXPECT_EQ(count,
            GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  // Cancel wake lock and check the count.
  wake_lock_->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// One WakeLock instance can serve multiple clients at same time.
TEST_F(WakeLockTest, MultipleClients) {
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  mojo::Remote<mojom::WakeLock> wake_lock_1;
  mojo::Remote<mojom::WakeLock> wake_lock_2;
  mojo::Remote<mojom::WakeLock> wake_lock_3;
  wake_lock_->AddClient(wake_lock_1.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_2.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_3.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1->RequestWakeLock();
  wake_lock_2->RequestWakeLock();
  wake_lock_3->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1->CancelWakeLock();
  wake_lock_2->CancelWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_3->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// WakeLock should update the wake lock status correctly when
// connection error happens.
TEST_F(WakeLockTest, OnWakeLockConnectionError) {
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  mojo::Remote<mojom::WakeLock> wake_lock_1;
  mojo::Remote<mojom::WakeLock> wake_lock_2;
  mojo::Remote<mojom::WakeLock> wake_lock_3;
  wake_lock_->AddClient(wake_lock_1.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_2.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_3.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1->RequestWakeLock();
  wake_lock_2->RequestWakeLock();
  wake_lock_3->RequestWakeLock();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_2.reset();
  wake_lock_3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

// Test mixed operations.
TEST_F(WakeLockTest, MixedTest) {
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  mojo::Remote<mojom::WakeLock> wake_lock_1;
  mojo::Remote<mojom::WakeLock> wake_lock_2;
  mojo::Remote<mojom::WakeLock> wake_lock_3;
  wake_lock_->AddClient(wake_lock_1.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_2.BindNewPipeAndPassReceiver());
  wake_lock_->AddClient(wake_lock_3.BindNewPipeAndPassReceiver());

  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  // Execute a series of calls that should result in |wake_lock_1| and
  // |wake_lock_3| having outstanding wake lock requests.
  wake_lock_1->RequestWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  wake_lock_1->CancelWakeLock();
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_2->RequestWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1->RequestWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  wake_lock_1->RequestWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_3->CancelWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  wake_lock_3->CancelWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_2->CancelWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_3->RequestWakeLock();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_2.reset();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));

  wake_lock_3->CancelWakeLock();
  EXPECT_FALSE(HasWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
}

TEST_F(WakeLockTest, SameWakeLockTypeObserverTest) {
  // Set up observer for |kPreventAppSuspension| wake lock events.
  mojo::PendingRemote<mojom::WakeLockObserver> observer;
  TestWakeLockObserver test_wake_lock_observer;
  test_wake_lock_observer.AddReceiver(
      observer.InitWithNewPipeAndPassReceiver());
  wake_lock_provider_->NotifyOnWakeLockDeactivation(
      mojom::WakeLockType::kPreventAppSuspension, std::move(observer));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  // Observer should be triggered since the wake lock wasn't held.
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));

  // Make two wake lock requests from the same client, these should be coalesced
  // into one and result in the first |kPreventAppSuspension| wake lock being
  // created. This should result in an acquire event.
  wake_lock_->RequestWakeLock();
  wake_lock_->RequestWakeLock();
  wake_lock_.FlushForTesting();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));

  // Add another client for the same wake lock type and make a request. This
  // shouldn't affect wake up counts as a |kPreventAppSuspension| wake lock is
  // already held.
  mojo::Remote<mojom::WakeLock> wake_lock2;
  wake_lock_provider_->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther, "WakeLockTest",
      wake_lock2.BindNewPipeAndPassReceiver());
  wake_lock2->RequestWakeLock();
  wake_lock2.FlushForTesting();
  EXPECT_EQ(2, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));

  // Cancel request should result in no change in counts as two clients
  // requested wake locks.
  wake_lock_->CancelWakeLock();
  wake_lock_.FlushForTesting();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));

  // Resetting second client should result in it's wake lock be released and no
  // |kPreventAppSuspension| wake locks being present in the system i.e. on a
  // release event. For reset events |FlushForTesting| can't be used.
  base::RunLoop run_loop4;
  wake_lock2.reset();
  run_loop4.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(2, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));

  // Resetting first wake lock client should result in no change in event count.
  // For reset events |FlushForTesting| can't be used.
  base::RunLoop run_loop5;
  wake_lock_.reset();
  run_loop5.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(2, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));
}

TEST_F(WakeLockTest, DifferentWakeLockTypesObserverTest) {
  // Setup observer that will observe events for two different types of wake
  // locks. No wake locks should be active and deactivation counts should be
  // received for each type of observed wake lock.
  mojo::PendingRemote<mojom::WakeLockObserver> observer;
  TestWakeLockObserver test_wake_lock_observer;
  test_wake_lock_observer.AddReceiver(
      observer.InitWithNewPipeAndPassReceiver());
  wake_lock_provider_->NotifyOnWakeLockDeactivation(
      mojom::WakeLockType::kPreventAppSuspension, std::move(observer));
  observer.reset();
  test_wake_lock_observer.AddReceiver(
      observer.InitWithNewPipeAndPassReceiver());
  wake_lock_provider_->NotifyOnWakeLockDeactivation(
      mojom::WakeLockType::kPreventDisplaySleep, std::move(observer));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventDisplaySleep));

  // Acquire two different type of wake locks and check if the observer for each
  // gets an acquire event.
  mojo::Remote<mojom::WakeLock> wake_lock2;
  wake_lock_provider_->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventDisplaySleep,
      device::mojom::WakeLockReason::kOther, "WakeLockTest",
      wake_lock2.BindNewPipeAndPassReceiver());
  wake_lock_->RequestWakeLock();
  wake_lock2->RequestWakeLock();
  wake_lock_.FlushForTesting();
  wake_lock2.FlushForTesting();
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(1, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventDisplaySleep));

  // Release wake locks of both types and check if observers for each got on
  // release events. For reset events |FlushForTesting| can't be used.
  base::RunLoop run_loop2;
  wake_lock_.reset();
  wake_lock2.reset();
  run_loop2.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(0, GetActiveWakeLocks(mojom::WakeLockType::kPreventDisplaySleep));
  EXPECT_EQ(2, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventAppSuspension));
  EXPECT_EQ(2, test_wake_lock_observer.GetOnDeactivationCount(
                   mojom::WakeLockType::kPreventDisplaySleep));
}

}  // namespace

}  // namespace device
