// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_properties.h"

#include "third_party/blink/renderer/core/paint/object_paint_properties_impl.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties_sparse.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ObjectPaintProperties::~ObjectPaintProperties() = default;

// static
std::unique_ptr<ObjectPaintProperties> ObjectPaintProperties::Create() {
  if (RuntimeEnabledFeatures::SparseObjectPaintPropertiesEnabled()) {
    return std::make_unique<ObjectPaintPropertiesSparse>();
  }
  return std::make_unique<ObjectPaintPropertiesImpl>();
}

}  // namespace blink
