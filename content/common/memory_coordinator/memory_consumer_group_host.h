// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/memory_consumer_update.h"

namespace content {

// An interface for an object that handles memory consumer groups.
// This allows the registry/controller to dispatch updates to the host,
// which then propagates them to the specific consumer group(s).
class CONTENT_EXPORT MemoryConsumerGroupHost {
 public:
  virtual ~MemoryConsumerGroupHost() = default;

  // Notifies of multiple changes at once.
  virtual void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) = 0;

  // Sets or clears an override limit on consumers in this host.
  virtual void SetOverrideLimit(uint32_t consumer_id, int percentage) = 0;
  virtual void ClearOverrideLimit(uint32_t consumer_id, int policy_limit) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_
