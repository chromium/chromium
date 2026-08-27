// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEMORY_CONSUMER_UPDATE_H_
#define CONTENT_PUBLIC_COMMON_MEMORY_CONSUMER_UPDATE_H_

#include <optional>
#include <string>

#include "base/memory_coordinator/memory_limit.h"
#include "content/common/content_export.h"

namespace content {

// Represents a memory update for a consumer. `memory_limit` is the new memory
// limit to apply if it has a value, or null if the limit remains unchanged.
// `release_memory` is true if the consumer should be notified to release
// its memory.
struct CONTENT_EXPORT MemoryConsumerUpdate {
  uint32_t consumer_id;
  std::optional<base::MemoryLimit> memory_limit;
  bool release_memory = false;

  bool operator==(const MemoryConsumerUpdate&) const = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MEMORY_CONSUMER_UPDATE_H_
