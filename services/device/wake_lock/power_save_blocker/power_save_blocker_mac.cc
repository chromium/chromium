// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"

namespace device {
namespace {

// Power management cannot be done on the UI thread. IOPMAssertionCreate does a
// synchronous MIG call to configd, so if it is called on the main thread the UI
// is at the mercy of another process. See http://crbug.com/79559 and
// http://www.opensource.apple.com/source/IOKitUser/IOKitUser-514.16.31/pwr_mgt.subproj/IOPMLibPrivate.c
// .
struct PowerSaveBlockerLazyInstanceTraits {
  static const bool kRegisterOnExit = false;
#if DCHECK_IS_ON()
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static base::Thread* New(void* instance) {
    base::Thread* thread = new (instance) base::Thread("PowerSaveBlocker");
    thread->Start();
    return thread;
  }
  static void Delete(base::Thread* instance) {}
};
base::LazyInstance<base::Thread, PowerSaveBlockerLazyInstanceTraits>
    g_power_thread = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(mojom::WakeLockType type, const std::string& description)
      : type_(type),
        description_(description),
        assertion_(kIOPMNullAssertionID) {}

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}
  mojom::WakeLockType type_;
  std::string description_;
  IOPMAssertionID assertion_;
};

void PowerSaveBlocker::Delegate::ApplyBlock() {
  DCHECK_EQ(base::PlatformThread::CurrentId(),
            g_power_thread.Pointer()->GetThreadId());

  CFStringRef level = NULL;
  // See QA1340 <http://developer.apple.com/library/mac/#qa/qa1340/> for more
  // details.
  switch (type_) {
    case mojom::WakeLockType::kPreventAppSuspension:
      level = kIOPMAssertionTypeNoIdleSleep;
      break;
    case mojom::WakeLockType::kPreventDisplaySleep:
    case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
      level = kIOPMAssertionTypeNoDisplaySleep;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  if (level) {
    base::apple::ScopedCFTypeRef<CFStringRef> cf_description(
        base::SysUTF8ToCFStringRef(description_));
    IOReturn result = IOPMAssertionCreateWithName(
        level, kIOPMAssertionLevelOn, cf_description.get(), &assertion_);
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionCreate: " << result;
  }
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK_EQ(base::PlatformThread::CurrentId(),
            g_power_thread.Pointer()->GetThreadId());

  if (assertion_ != kIOPMNullAssertionID) {
    IOReturn result = IOPMAssertionRelease(assertion_);
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionRelease: " << result;
  }
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate(type, description)),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  g_power_thread.Pointer()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlocker::~PowerSaveBlocker() {
  g_power_thread.Pointer()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::RemoveBlock, delegate_));
}

}  // namespace device
