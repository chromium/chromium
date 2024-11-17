// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_with_task_environment.h"

#include <memory>

#include "base/command_line.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/test/test_net_log_manager.h"

namespace net {

WithTaskEnvironment::WithTaskEnvironment(
    base::test::TaskEnvironment::TimeSource time_source)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                        time_source) {
  MaybeStartNetLog();
}

WithTaskEnvironment::~WithTaskEnvironment() = default;

void WithTaskEnvironment::MaybeStartNetLog() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(TestNetLogManager::kLogNetLogSwitch)) {
    net_log_manager_ = std::make_unique<TestNetLogManager>(
        NetLog::Get(), NetLogCaptureMode::kEverything);
  }
}

}  // namespace net
