// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_

#include <string_view>

#include "content/common/content_export.h"

namespace content {

// An interface for an object that handles memory consumer groups.
// This allows the registry/controller to dispatch updates to the host,
// which then propagates them to the specific consumer group(s).
class CONTENT_EXPORT MemoryConsumerGroupHost {
 public:
  virtual ~MemoryConsumerGroupHost() = default;

  // Sets the memory limit for the consumer group with `consumer_id`.
  virtual void UpdateMemoryLimit(std::string_view consumer_id,
                                 int percentage) = 0;

  // Releases memory for the consumer group with `consumer_id`.
  virtual void ReleaseMemory(std::string_view consumer_id) = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_GROUP_HOST_H_
