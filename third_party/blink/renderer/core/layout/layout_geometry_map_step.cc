// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_geometry_map_step.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void LayoutGeometryMapStep::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
}

}  // namespace blink
