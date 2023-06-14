// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_paint_properties.h"

#include "third_party/blink/renderer/core/paint/object_paint_properties_impl.h"

namespace blink {

ObjectPaintProperties::~ObjectPaintProperties() = default;

// static
std::unique_ptr<ObjectPaintProperties> ObjectPaintProperties::Create() {
  return std::make_unique<ObjectPaintPropertiesImpl>();
}

}  // namespace blink
