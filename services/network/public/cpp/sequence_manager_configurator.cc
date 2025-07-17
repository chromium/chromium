// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sequence_manager_configurator.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "services/network/public/cpp/network_service_task_priority.h"

namespace network {

namespace {

// Set to true if the current thread's SequenceManager is configured correctly
// to support the NetworkServiceTaskScheduler priorities.
//
// TODO(crbug.com/421051258): Make this flag thread local. Currently this flag
// is set on the main thread which starts IO thread.
bool g_is_sequence_manager_configured = false;

}  // namespace

void ConfigureSequenceManager(base::Thread::Options& options) {
  options.sequence_manager_settings =
      std::make_unique<base::sequence_manager::SequenceManagerSettings>(
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetPrioritySettings(CreateNetworkServiceTaskPrioritySettings())
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

}  // namespace network
