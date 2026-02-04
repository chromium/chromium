// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_

#include <string_view>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

// An interface that allows an external component to control or act upon
// consumer groups.
class MemoryConsumerGroupController {
 public:
  virtual ~MemoryConsumerGroupController() = default;

  // Called when a new consumer group is added to the registry.
  virtual void OnConsumerGroupAdded(
      std::string_view consumer_id,
      base::MemoryConsumerTraits traits,
      ProcessType process_type,
      ChildProcessId child_process_id,
      base::RegisteredMemoryConsumer consumer) = 0;

  // Called when a consumer group is removed from the registry.
  virtual void OnConsumerGroupRemoved(std::string_view consumer_id,
                                      ChildProcessId child_process_id) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_CONTROLLER_H_
