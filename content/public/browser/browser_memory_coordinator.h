// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MEMORY_COORDINATOR_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MEMORY_COORDINATOR_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

class MemoryCoordinatorPolicyManager;

// Public interface to the browser-side MemoryCoordinator singleton.
class CONTENT_EXPORT BrowserMemoryCoordinator {
 public:
  static BrowserMemoryCoordinator& Get();

  // Creates a BrowserMemoryCoordinator instance for unit tests that do not run
  // full browser initialization.
  static std::unique_ptr<BrowserMemoryCoordinator> CreateForTesting();

  virtual ~BrowserMemoryCoordinator() = default;

  virtual MemoryCoordinatorPolicyManager& policy_manager() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MEMORY_COORDINATOR_H_
