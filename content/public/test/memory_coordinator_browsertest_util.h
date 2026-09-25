// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory_coordinator/memory_limit.h"
#include "content/common/content_export.h"

namespace content::test {

// A scoped object to override the memory limit of a specific consumer for
// tests. This can be used by tests outside of content/ (e.g., in chrome/).
// Note: This class does not support nested overrides for the same consumer
// name.
class ScopedMemoryLimitOverride {
 public:
  explicit ScopedMemoryLimitOverride(std::string_view consumer_name);
  ~ScopedMemoryLimitOverride();

  void SetLimit(base::MemoryLimit memory_limit);
  void ClearLimit();
  void NotifyReleaseMemory();

  ScopedMemoryLimitOverride(const ScopedMemoryLimitOverride&) = delete;
  ScopedMemoryLimitOverride& operator=(const ScopedMemoryLimitOverride&) =
      delete;

 private:
  const uint32_t consumer_id_;
  std::optional<base::MemoryLimit> limit_;
};

}  // namespace content::test

#endif  // CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
