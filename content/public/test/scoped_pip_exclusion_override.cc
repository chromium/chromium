// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_pip_exclusion_override.h"

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"
#include "content/test/test_pip_screen_capture_coordinator.h"

namespace content {

ScopedPipExclusionOverride::ScopedPipExclusionOverride(bool is_excluded)
    : test_coordinator_(
          std::make_unique<TestPipScreenCaptureCoordinator>(is_excluded)) {
  PipScreenCaptureCoordinator::SetInstanceForTesting(test_coordinator_.get());
}

ScopedPipExclusionOverride::~ScopedPipExclusionOverride() {
  PipScreenCaptureCoordinator::SetInstanceForTesting(nullptr);
}

void ScopedPipExclusionOverride::SetExcluded(bool is_excluded) {
  test_coordinator_->SetExcluded(is_excluded);
}

}  // namespace content
