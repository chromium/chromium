// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_TEST_FAKE_OPENXR_IMPL_API_H_
#define DEVICE_VR_OPENXR_TEST_FAKE_OPENXR_IMPL_API_H_

#include "third_party/openxr/src/include/openxr/openxr.h"

// Returns the main process xrGetInstanceProcAddr function pointer for the mock
// implementation.
PFN_xrGetInstanceProcAddr GetMockXrGetInstanceProcAddr();

#endif  // DEVICE_VR_OPENXR_TEST_FAKE_OPENXR_IMPL_API_H_
