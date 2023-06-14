// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_

#include "third_party/blink/renderer/core/paint/object_paint_properties_impl.h"

namespace blink {

// TODO(https://crbug.com/1454638): replace with pure virtual base class.
class ObjectPaintProperties : public ObjectPaintPropertiesImpl {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_PROPERTIES_H_
