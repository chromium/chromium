// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheduler/sequence_manager_configurator.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "net/base/scheduler/net_task_priority.h"

namespace net {

namespace {

// Set to true if the current thread's SequenceManager is configured correctly
// to support the NetTaskScheduler priorities.
//
// TODO(crbug.com/421051258): Make this flag thread local. Currently this flag
// is set on the main thread which starts IO thread.
bool g_is_sequence_manager_configured = false;

}  // namespace

void ConfigureSequenceManager(base::Thread::Options& options) {
  options.sequence_manager_settings =
      std::make_unique<base::sequence_manager::SequenceManagerSettings>(
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetPrioritySettings(CreateNetTaskPrioritySettings())
              .SetMessagePumpType(options.message_pump_type)
              .SetCanRunTasksByBatches(true)
              .SetAddQueueTimeToTasks(true)
              .SetShouldSampleCPUTime(true)
              .Build());
  g_is_sequence_manager_configured = true;
}

bool IsSequenceManagerConfigured() {
  return g_is_sequence_manager_configured;
}

}  // namespace net
