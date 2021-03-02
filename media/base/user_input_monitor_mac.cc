// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <ApplicationServices/ApplicationServices.h>
#include <stddef.h>
#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"

namespace media {
namespace {

// Update key press count in shared memory twice as frequent as
// AudioInputController::AudioCallback::OnData() callback for WebRTC.
constexpr base::TimeDelta kUpdateKeyPressCountIntervalMs =
    base::TimeDelta::FromMilliseconds(5);

class UserInputMonitorMac : public UserInputMonitorBase {
 public:
  UserInputMonitorMac();
  ~UserInputMonitorMac() override;

  uint32_t GetKeyPressCount() const override;

 private:
  void StartKeyboardMonitoring() override;
  void StartKeyboardMonitoring(
      base::WritableSharedMemoryMapping mapping) override;
  void StopKeyboardMonitoring() override;

  void UpdateKeyPressCountShmem();

  // Used for sharing key press count value.
  std::unique_ptr<base::WritableSharedMemoryMapping> key_press_count_mapping_;

  // Timer for updating key press count in |key_press_count_mapping_|.
  base::RepeatingTimer key_press_count_timer_;

  DISALLOW_COPY_AND_ASSIGN(UserInputMonitorMac);
};

UserInputMonitorMac::UserInputMonitorMac() {}

UserInputMonitorMac::~UserInputMonitorMac() {}

uint32_t UserInputMonitorMac::GetKeyPressCount() const {
  // Use |kCGEventSourceStateHIDSystemState| since we only want to count
  // hardware generated events.
  return CGEventSourceCounterForEventType(kCGEventSourceStateHIDSystemState,
                                          kCGEventKeyDown);
}

void UserInputMonitorMac::StartKeyboardMonitoring() {}

void UserInputMonitorMac::StartKeyboardMonitoring(
    base::WritableSharedMemoryMapping mapping) {
  key_press_count_mapping_ =
      std::make_unique<base::WritableSharedMemoryMapping>(std::move(mapping));
  key_press_count_timer_.Start(FROM_HERE, kUpdateKeyPressCountIntervalMs, this,
                               &UserInputMonitorMac::UpdateKeyPressCountShmem);
}

void UserInputMonitorMac::StopKeyboardMonitoring() {
  if (!key_press_count_mapping_)
    return;

  key_press_count_timer_.AbandonAndStop();
  key_press_count_mapping_.reset();
}

void UserInputMonitorMac::UpdateKeyPressCountShmem() {
  DCHECK(key_press_count_mapping_);
  WriteKeyPressMonitorCount(*key_press_count_mapping_, GetKeyPressCount());
}

}  // namespace

std::unique_ptr<UserInputMonitor> UserInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return std::make_unique<UserInputMonitorMac>();
}

}  // namespace media
