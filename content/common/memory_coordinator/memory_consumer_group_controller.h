// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_

#include <optional>
#include <string_view>

#include "base/memory_coordinator/traits.h"
#include "content/common/buildflags.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

class MemoryConsumerGroupHost;

// An interface that allows an external component to control or act upon
// consumer groups.
class MemoryConsumerGroupController {
 public:
  virtual ~MemoryConsumerGroupController() = default;

  // Called when a new host is added/removed.
  virtual void AddMemoryConsumerGroupHost(ChildProcessId child_process_id,
                                          MemoryConsumerGroupHost* host) = 0;
  virtual void RemoveMemoryConsumerGroupHost(
      ChildProcessId child_process_id) = 0;

  // Called when a new consumer group is added/removed to/from the host.
  virtual void OnConsumerGroupAdded(
      std::string_view consumer_id,
      std::optional<base::MemoryConsumerTraits> traits,
      ProcessType process_type,
      ChildProcessId child_process_id) = 0;
  virtual void OnConsumerGroupRemoved(std::string_view consumer_id,
                                      ChildProcessId child_process_id) = 0;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // Called when the aggregate memory limit for a consumer group changes in the
  // child process.
  virtual void OnMemoryLimitChanged(std::string_view consumer_id,
                                    ChildProcessId child_process_id,
                                    int memory_limit) = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_
