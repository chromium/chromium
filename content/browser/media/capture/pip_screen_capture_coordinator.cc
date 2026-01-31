// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"

#include "build/build_config.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"

namespace content {

namespace {
PipScreenCaptureCoordinator* g_instance_for_testing = nullptr;
}

PipScreenCaptureCoordinator* PipScreenCaptureCoordinator::GetInstance() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
#if BUILDFLAG(IS_MAC)
  return PipScreenCaptureCoordinatorImpl::GetInstance();
#else
  return nullptr;
#endif
}

void PipScreenCaptureCoordinator::SetInstanceForTesting(  // IN-TEST
    PipScreenCaptureCoordinator* instance) {
  g_instance_for_testing = instance;
}

}  // namespace content
