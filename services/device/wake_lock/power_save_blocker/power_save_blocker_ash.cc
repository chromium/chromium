// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/power/power_policy_controller.h"

namespace device {

namespace {

// Converts a mojom::WakeLockReason to a
// chromeos::PowerPolicyController::WakeLockReason.
chromeos::PowerPolicyController::WakeLockReason GetWakeLockReason(
    mojom::WakeLockReason reason) {
  switch (reason) {
    case mojom::WakeLockReason::kAudioPlayback:
      return chromeos::PowerPolicyController::REASON_AUDIO_PLAYBACK;
    case mojom::WakeLockReason::kVideoPlayback:
      return chromeos::PowerPolicyController::REASON_VIDEO_PLAYBACK;
    case mojom::WakeLockReason::kOther:
      return chromeos::PowerPolicyController::REASON_OTHER;
  }
  return chromeos::PowerPolicyController::REASON_OTHER;
}

}  // namespace

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(mojom::WakeLockType type,
           mojom::WakeLockReason reason,
           const std::string& description,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : type_(type),
        reason_(reason),
        description_(description),
        block_id_(0),
        ui_task_runner_(ui_task_runner) {}

  Delegate(const Delegate&) = delete;
  const Delegate& operator=(const Delegate&) = delete;

  void ApplyBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    auto* controller = chromeos::PowerPolicyController::Get();
    switch (type_) {
      case mojom::WakeLockType::kPreventAppSuspension:
        block_id_ = controller->AddSystemWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      case mojom::WakeLockType::kPreventDisplaySleep:
        block_id_ = controller->AddScreenWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
        block_id_ = controller->AddDimWakeLock(GetWakeLockReason(reason_),
                                               description_);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "Unhandled block type " << type_;
    }
  }

  void RemoveBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    chromeos::PowerPolicyController::Get()->RemoveWakeLock(block_id_);
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  mojom::WakeLockType type_;
  mojom::WakeLockReason reason_;
  std::string description_;

  // ID corresponding to the block request in PowerPolicyController.
  int block_id_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate(type, reason, description, ui_task_runner)),
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
