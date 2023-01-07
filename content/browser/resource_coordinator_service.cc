// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_coordinator_service.h"

#include "base/trace_event/memory_dump_manager.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/registry.h"

namespace content {

memory_instrumentation::Registry* GetMemoryInstrumentationRegistry() {
  DCHECK(base::trace_event::MemoryDumpManager::GetInstance()
             ->GetDumpThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  static memory_instrumentation::Registry* registry =
      new memory_instrumentation::CoordinatorImpl();
  return registry;
}

}  // namespace content
