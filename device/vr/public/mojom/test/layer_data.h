// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_LAYER_DATA_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_LAYER_DATA_H_

#include <vector>

#include "base/component_export.h"
#include "device/vr/public/mojom/test/color.h"

namespace device {

enum class LayerType {
  kNone = 0,
  kQuad = 1,
  kCylinder = 2,
  kEquirect = 3,
  kCube = 4,
};

struct COMPONENT_EXPORT(VR_PUBLIC_TEST_TYPEMAPS) LayerData {
  LayerType type = LayerType::kNone;
  std::vector<Color> face_colors;

  // We need to explicitly define these ctors/dtor to avoid style warnings.
  LayerData();
  explicit LayerData(LayerType);
  ~LayerData();
  LayerData(const LayerData& other);
  LayerData& operator=(const LayerData& other);
  LayerData& operator=(LayerData&& other);
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_LAYER_DATA_H_
