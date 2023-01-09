// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {

/******** PowerSaveBlocker::Delegate ********/

// Lacros-chrome PowerSaveBlocker uses ash-chrome ProwerSaveBlocker via crosapi.
// RAII style is maintained by keeping a crosapi::mojom::PowerWakeLock Mojo
// connection, whose disconnection triggers resource release in ash-chrome.
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
        ui_task_runner_(ui_task_runner) {}
  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  void ApplyBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

    auto* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service->IsAvailable<crosapi::mojom::Power>()) {
      lacros_service->GetRemote<crosapi::mojom::Power>()->AddPowerSaveBlocker(
          receiver_.BindNewPipeAndPassRemote(), type_, reason_, description_);
    }
  }

  void RemoveBlock() {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

    // Disconnect to make ash-chrome release its PowerSaveBlocker.
    receiver_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() = default;

  // Connection to ash-chrome via crosapi. Disconnection from RemoveBlock() or
  // Lacros termination triggers resource release in ash-chrome.
  crosapi::mojom::PowerWakeLock lock_;
  mojo::Receiver<crosapi::mojom::PowerWakeLock> receiver_{&lock_};

  mojom::WakeLockType type_;
  mojom::WakeLockReason reason_;
  std::string description_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

/******** PowerSaveBlocker ********/

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(base::MakeRefCounted<Delegate>(type,
                                               reason,
                                               description,
                                               ui_task_runner)),
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
