// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_test_helper_win.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"

namespace content {
MockDirectManipulationContent::MockDirectManipulationContent() {}
MockDirectManipulationContent::~MockDirectManipulationContent() = default;

void MockDirectManipulationContent::SetContentTransform(float scale,
                                                        float scroll_x,
                                                        float scroll_y) {
  // Values in position 1 and 2 are both 0, because those store rotation
  // transforms. 0 and 3 are both scale because 0 stores the x scale and 3
  // stores the y scale. See comment in header file for more information.
  transforms_[0] = scale;
  transforms_[1] = 0.0f;
  transforms_[2] = 0.0f;
  transforms_[3] = scale;
  transforms_[4] = scroll_x;
  transforms_[5] = scroll_y;
}

UNSAFE_BUFFER_USAGE
// Safety: This is a mock for a Win32 API[1] and the method signature cannot be
// changed to take a span.
// [1]https://learn.microsoft.com/en-us/windows/win32/api/directmanipulation/nf-directmanipulation-idirectmanipulationcontent-getcontenttransform
HRESULT MockDirectManipulationContent::GetContentTransform(float* transforms,
                                                           DWORD point_count) {
  DCHECK_EQ(point_count, transforms_.size());

  for (size_t i = 0; i < transforms_.size(); ++i)
    UNSAFE_BUFFERS(transforms[i]) = transforms_[i];
  return S_OK;
}

// Other Overrides
HRESULT MockDirectManipulationContent::GetContentRect(RECT* contentSize) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT
MockDirectManipulationContent::SetContentRect(const RECT* contentSize) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT MockDirectManipulationContent::GetViewport(REFIID riid, void** object) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT MockDirectManipulationContent::GetTag(REFIID riid,
                                              void** object,
                                              UINT32* id) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT MockDirectManipulationContent::SetTag(IUnknown* object, UINT32 id) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT MockDirectManipulationContent::GetOutputTransform(float* matrix,
                                                          DWORD point_count) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

HRESULT
MockDirectManipulationContent::SyncContentTransform(const float* matrix,
                                                    DWORD point_count) {
  NOTREACHED_IN_MIGRATION();
  return S_OK;
}

}  // namespace content
