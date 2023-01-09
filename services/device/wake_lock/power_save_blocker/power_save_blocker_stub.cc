// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace device {

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate() {}

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}
};

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate()),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {}

PowerSaveBlocker::~PowerSaveBlocker() {}

}  // namespace device
