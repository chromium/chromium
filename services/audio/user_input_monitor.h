// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_USER_INPUT_MONITOR_H_
#define SERVICES_AUDIO_USER_INPUT_MONITOR_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "media/base/user_input_monitor.h"

namespace audio {

// TODO(crbug.com/40573245) remove inheritance after switching to audio
// service input streams.
class UserInputMonitor : public media::UserInputMonitor {
 public:
  explicit UserInputMonitor(base::ReadOnlySharedMemoryMapping memory_mapping);

  UserInputMonitor(const UserInputMonitor&) = delete;
  UserInputMonitor& operator=(const UserInputMonitor&) = delete;

  ~UserInputMonitor() override;

  // Returns nullptr for invalid handle.
  static std::unique_ptr<UserInputMonitor> Create(
      base::ReadOnlySharedMemoryRegion keypress_count_buffer);

  void EnableKeyPressMonitoring() override;
  void DisableKeyPressMonitoring() override;
  uint32_t GetKeyPressCount() const override;

 private:
  base::ReadOnlySharedMemoryMapping key_press_count_mapping_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_USER_INPUT_MONITOR_H_
