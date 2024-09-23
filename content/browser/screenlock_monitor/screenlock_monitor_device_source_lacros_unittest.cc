// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestScreenlockObserver : public ScreenlockObserver {
 public:
  TestScreenlockObserver() = default;
  ~TestScreenlockObserver() override = default;

  TestScreenlockObserver(const TestScreenlockObserver&) = delete;
  TestScreenlockObserver& operator=(const TestScreenlockObserver&) = delete;

  // ScreenlockObserver:
  void OnScreenLocked() override {
    locked_notified_ = true;
    if (notified_callback_)
      notified_callback_.Run();
  }
  void OnScreenUnlocked() override {
    unlocked_notified_ = true;
    if (notified_callback_)
      notified_callback_.Run();
  }

  bool locked_notified() const { return locked_notified_; }
  bool unlocked_notified() const { return unlocked_notified_; }

  void Reset() {
    locked_notified_ = false;
    unlocked_notified_ = false;
    notified_callback_ = base::RepeatingClosure();
  }

  void set_notified_callback(base::RepeatingClosure callback) {
    notified_callback_ = std::move(callback);
  }

 private:
  bool locked_notified_ = false;
  bool unlocked_notified_ = false;

  base::RepeatingClosure notified_callback_;
};

class ScreenlockMonitorDeviceSourceLacrosTest
    : public testing::Test,
      public crosapi::mojom::LoginState {
 public:
  ScreenlockMonitorDeviceSourceLacrosTest(
      const ScreenlockMonitorDeviceSourceLacrosTest&) = delete;
  ScreenlockMonitorDeviceSourceLacrosTest& operator=(
      const ScreenlockMonitorDeviceSourceLacrosTest&) = delete;

 protected:
  ScreenlockMonitorDeviceSourceLacrosTest() : receiver_(this) {}
  ~ScreenlockMonitorDeviceSourceLacrosTest() override = default;

  void SetUp() override {
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        receiver_.BindNewPipeAndPassRemote());

    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::make_unique<ScreenlockMonitorDeviceSource>());
    screenlock_monitor_->AddObserver(&observer_);

    observer_pending_receiver_ = observer_remote_.BindNewPipeAndPassReceiver();
  }

  // crosapi::mojom::LoginState:
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::SessionStateChangedEventObserver>
          observer) override {
    DCHECK(observer_pending_receiver_);
    mojo::FusePipes(std::move(observer_pending_receiver_), std::move(observer));
  }
  void GetSessionState(GetSessionStateCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetSessionStateAndNotify(crosapi::mojom::SessionState state) {
    observer_remote_->OnSessionStateChanged(state);
  }

  base::test::TaskEnvironment task_environment_;
  chromeos::ScopedLacrosServiceTestHelper lacros_service_test_helper_;

  TestScreenlockObserver observer_;
  std::unique_ptr<ScreenlockMonitor> screenlock_monitor_;

  mojo::Receiver<crosapi::mojom::LoginState> receiver_;
  mojo::Remote<crosapi::mojom::SessionStateChangedEventObserver>
      observer_remote_;
  mojo::PendingReceiver<crosapi::mojom::SessionStateChangedEventObserver>
      observer_pending_receiver_;
};

}  // namespace

TEST_F(ScreenlockMonitorDeviceSourceLacrosTest, Notifications) {
  // Use RunUntilIdle() for some places where it is expected that something
  // doesn't happen and therefore no suitable place to call QuitClosure().
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer_.locked_notified());
  EXPECT_FALSE(observer_.unlocked_notified());

  {
    base::RunLoop run_loop;
    observer_.set_notified_callback(run_loop.QuitClosure());
    SetSessionStateAndNotify(crosapi::mojom::SessionState::kInSession);
    run_loop.Run();
    EXPECT_FALSE(observer_.locked_notified());
    EXPECT_TRUE(observer_.unlocked_notified());
  }

  observer_.Reset();

  // Test that there are no surplus notifications for unlocked state.
  SetSessionStateAndNotify(crosapi::mojom::SessionState::kInOobeScreen);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer_.locked_notified());
  EXPECT_FALSE(observer_.unlocked_notified());

  {
    base::RunLoop run_loop;
    observer_.set_notified_callback(run_loop.QuitClosure());
    SetSessionStateAndNotify(crosapi::mojom::SessionState::kInLockScreen);
    run_loop.Run();
    EXPECT_TRUE(observer_.locked_notified());
    EXPECT_FALSE(observer_.unlocked_notified());
  }

  observer_.Reset();

  // Test that there are no surplus notifications for locked state.
  SetSessionStateAndNotify(crosapi::mojom::SessionState::kInLockScreen);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer_.locked_notified());
  EXPECT_FALSE(observer_.unlocked_notified());
}

}  // namespace content
