// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/mojom/test/layer_data.h"

namespace device {

LayerData::LayerData() = default;
LayerData::LayerData(LayerType type_in) : type(type_in) {}
LayerData::~LayerData() = default;
LayerData::LayerData(const LayerData& other) = default;
LayerData& LayerData::operator=(const LayerData& other) = default;
LayerData& LayerData::operator=(LayerData&& other) = default;

}  // namespace device
