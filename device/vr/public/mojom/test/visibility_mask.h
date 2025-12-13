// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_VISIBILITY_MASK_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_VISIBILITY_MASK_H_

#include <array>

#include "base/component_export.h"

namespace device {

inline constexpr unsigned int kNumVisibilityMaskVerticesForTest = 6;
inline constexpr unsigned int kNumVisibilityMaskIndicesForTest = 3;

struct COMPONENT_EXPORT(VR_PUBLIC_TEST_TYPEMAPS) VisibilityMaskData {
  std::array<float, kNumVisibilityMaskVerticesForTest> vertices;
  std::array<uint32_t, kNumVisibilityMaskIndicesForTest> indices;
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_VISIBILITY_MASK_H_
