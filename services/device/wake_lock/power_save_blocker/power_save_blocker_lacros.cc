// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "ui/display/screen.h"

namespace device {

/******** PowerSaveBlocker::Delegate ********/

// Lacros-chrome PowerSaveBlocker uses ash-chrome ProwerSaveBlocker via Wayland.

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  explicit Delegate(scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : ui_task_runner_(ui_task_runner) {}
  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  void ApplyBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!screen_saver_suspender_);

    if (auto* const screen = display::Screen::GetScreen()) {
      screen_saver_suspender_ = screen->SuspendScreenSaver();
    }
  }

  void RemoveBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

    screen_saver_suspender_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() = default;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  std::unique_ptr<display::Screen::ScreenSaverSuspender>
      screen_saver_suspender_;
};

/******** PowerSaveBlocker ********/

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(base::MakeRefCounted<Delegate>(ui_task_runner)),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlocker::~PowerSaveBlocker() {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Delegate::RemoveBlock, delegate_));
}

}  // namespace device
