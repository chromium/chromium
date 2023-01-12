// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <windows.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

namespace device {
namespace {

HANDLE CreatePowerRequest(POWER_REQUEST_TYPE type,
                          const std::string& description) {
  std::wstring wide_description = base::ASCIIToWide(description);
  REASON_CONTEXT context = {0};
  context.Version = POWER_REQUEST_CONTEXT_VERSION;
  context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
  context.Reason.SimpleReasonString =
      const_cast<wchar_t*>(wide_description.c_str());

  base::win::ScopedHandle handle(::PowerCreateRequest(&context));
  if (!handle.IsValid())
    return INVALID_HANDLE_VALUE;

  if (::PowerSetRequest(handle.Get(), type))
    return handle.Take();

  // Something went wrong.
  return INVALID_HANDLE_VALUE;
}

// Takes ownership of the |handle|.
void DeletePowerRequest(POWER_REQUEST_TYPE type, HANDLE handle) {
  base::win::ScopedHandle request_handle(handle);
  if (!request_handle.IsValid())
    return;

  BOOL success = ::PowerClearRequest(request_handle.Get(), type);
  DCHECK(success);
}

}  // namespace

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(mojom::WakeLockType type,
           const std::string& description,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : type_(type),
        description_(description),
        ui_task_runner_(ui_task_runner) {}

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

  // Returns the equivalent POWER_REQUEST_TYPE for this request.
  POWER_REQUEST_TYPE RequestType();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}

  mojom::WakeLockType type_;
  const std::string description_;
  base::win::ScopedHandle handle_;
  // Bug: 1182771
  // The Windows owners recommended acquiring both system and video wake
  // locks during onscreen media playback scenarios for Win10 and below to
  // ensure sleep timeout puts the system to sleep only after the display has
  // turned off. This is not necessary for Win11+ since only a display request
  // is needed and it will behave the same on both a S3 based system and a
  // Modern Standby one.
  base::win::ScopedHandle system_sleep_prevention_handle_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

void PowerSaveBlocker::Delegate::ApplyBlock() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  handle_.Set(CreatePowerRequest(RequestType(), description_));
  // See comment on instance variable above
  if (type_ == mojom::WakeLockType::kPreventDisplaySleep &&
      base::win::GetVersion() < base::win::Version::WIN11) {
    system_sleep_prevention_handle_.Set(
        CreatePowerRequest(PowerRequestSystemRequired, description_));
  }
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DeletePowerRequest(RequestType(), handle_.Take());
  DeletePowerRequest(PowerRequestSystemRequired,
                     system_sleep_prevention_handle_.Take());
}

POWER_REQUEST_TYPE PowerSaveBlocker::Delegate::RequestType() {
  if (type_ == mojom::WakeLockType::kPreventDisplaySleep ||
      type_ == mojom::WakeLockType::kPreventDisplaySleepAllowDimming)
    return PowerRequestDisplayRequired;

  return PowerRequestExecutionRequired;
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate(type, description, ui_task_runner)),
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
