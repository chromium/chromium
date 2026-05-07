// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

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

  void SetLimit(int percentage);
  void ClearLimit();
  void NotifyReleaseMemory();

  ScopedMemoryLimitOverride(const ScopedMemoryLimitOverride&) = delete;
  ScopedMemoryLimitOverride& operator=(const ScopedMemoryLimitOverride&) =
      delete;

 private:
  const std::string consumer_name_;
  std::optional<int> limit_;
};

}  // namespace content::test

#endif  // CONTENT_PUBLIC_TEST_MEMORY_COORDINATOR_BROWSERTEST_UTIL_H_
