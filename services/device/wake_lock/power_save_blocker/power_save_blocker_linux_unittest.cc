// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/switches.h"

namespace device {

namespace {

// A TestScreen that tracks the number of outstanding screensaver suspensions.
class ScreenSaverTrackingScreen : public display::test::TestScreen {
 public:
  ScreenSaverTrackingScreen()
      : display::test::TestScreen(/*create_display=*/true,
                                  /*register_screen=*/true) {}
  ~ScreenSaverTrackingScreen() override = default;

  int suspend_count() const { return suspend_count_; }

  // display::Screen:
  std::unique_ptr<ScreenSaverSuspender> SuspendScreenSaver() override {
    return std::make_unique<Suspender>(suspend_count_);
  }

 private:
  class Suspender : public ScreenSaverSuspender {
   public:
    explicit Suspender(int& count) : count_(count) { ++*count_; }
    ~Suspender() override { --*count_; }

   private:
    const raw_ref<int> count_;
  };

  int suspend_count_ = 0;
};

class PowerSaveBlockerLinuxTest : public testing::Test {
 protected:
  PowerSaveBlockerLinuxTest() {
    // Skip the D-Bus inhibit calls, which aren't available in tests. The
    // screensaver suspension is still exercised.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kHeadless);
  }

  std::unique_ptr<PowerSaveBlocker> CreateBlocker(mojom::WakeLockType type) {
    return std::make_unique<PowerSaveBlocker>(
        type, mojom::WakeLockReason::kOther, "test",
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  // PowerSaveBlocker posts its work to the current sequence, so a task posted
  // afterwards runs once that work has completed.
  void FlushTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedCommandLine scoped_command_line_;
  ScreenSaverTrackingScreen screen_;
};

}  // namespace

// kPreventAppSuspension (eg. audio-only playback) must not suspend the
// screensaver, otherwise the display is prevented from dimming and sleeping.
// On Wayland this creates a zwp_idle_inhibitor_v1. https://crbug.com/570067499
TEST_F(PowerSaveBlockerLinuxTest, AppSuspensionDoesNotSuspendScreenSaver) {
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventAppSuspension);
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

TEST_F(PowerSaveBlockerLinuxTest, DisplaySleepSuspendsScreenSaver) {
  auto blocker = CreateBlocker(mojom::WakeLockType::kPreventDisplaySleep);
  FlushTasks();
  EXPECT_EQ(1, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

TEST_F(PowerSaveBlockerLinuxTest, DisplaySleepAllowDimmingSuspendsScreenSaver) {
  auto blocker =
      CreateBlocker(mojom::WakeLockType::kPreventDisplaySleepAllowDimming);
  FlushTasks();
  EXPECT_EQ(1, screen_.suspend_count());

  blocker.reset();
  FlushTasks();
  EXPECT_EQ(0, screen_.suspend_count());
}

}  // namespace device
