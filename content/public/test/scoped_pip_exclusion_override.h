// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_PIP_EXCLUSION_OVERRIDE_H_
#define CONTENT_PUBLIC_TEST_SCOPED_PIP_EXCLUSION_OVERRIDE_H_

#include <memory>

namespace content {

class TestPipScreenCaptureCoordinator;

// Scoped object that overrides the value of IsPipExcludedFromScreenCapture()
// for testing and notifies observers of the change.
class ScopedPipExclusionOverride {
 public:
  explicit ScopedPipExclusionOverride(bool is_excluded);
  ~ScopedPipExclusionOverride();

  ScopedPipExclusionOverride(const ScopedPipExclusionOverride&) = delete;
  ScopedPipExclusionOverride& operator=(const ScopedPipExclusionOverride&) =
      delete;

  void SetExcluded(bool is_excluded);

 private:
  std::unique_ptr<TestPipScreenCaptureCoordinator> test_coordinator_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_PIP_EXCLUSION_OVERRIDE_H_
